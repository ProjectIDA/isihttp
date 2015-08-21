#pragma ident "$Id: main.c,v 1.23 2014/01/27 18:19:45 dechavez Exp $"
/*======================================================================
 *
 *  HTTP Post IDA NRTS SOH info to IDA STATUS WEB API
 *
 *====================================================================*/
#include "isi.h"
#include "util.h"

// for JSON support
#include <jansson.h>

// using libcurl for HTTP calls
#include <curl/curl.h>

extern char *VersionIdentString;

static time_t now;
static struct {
    struct tm tm;
    char buf[64];
} gm, local;

static BOOL verbose = FALSE;

#ifndef DEFAULT_IDASTAT_API_SOH_URL
#define DEFAULT_IDASTAT_API_SOH_URL "http://idaprog.ucsd.edu/api/nrts/v1/sohs"
#endif /* DEFAULT_IDASTAT_API_SOH_URL */

#ifndef DEFAULT_SERVER
#define DEFAULT_SERVER "idahub.ucsd.edu"
#endif /* DEFAULT_SERVER */

#define LIVE_LATENCY_THRESHOLD 21600  // 6 hrs



static void UcaseStationName(ISI_SOH_REPORT *soh, ISI_CNF_REPORT *cnf)
{
int i;

    for (i = 0; i < soh->nentry; i++) {
        util_ucase(soh->entry[i].name.sta);
        util_ucase(cnf->entry[i].name.sta);
    }
}

static int SortSohName(const void *a, const void *b)
{
int result;

    result = strcmp(((ISI_STREAM_SOH *) a)->name.sta, ((ISI_STREAM_SOH *) b)->name.sta);
    if (result != 0) return result;

    result = strcmp(((ISI_STREAM_SOH *) a)->name.chn, ((ISI_STREAM_SOH *) b)->name.chn);
    if (result != 0) return result;

    return strcmp(((ISI_STREAM_SOH *) a)->name.loc, ((ISI_STREAM_SOH *) b)->name.loc);
}

static VOID SortSoh(ISI_SOH_REPORT *soh)
{
    qsort(soh->entry, soh->nentry, sizeof(ISI_STREAM_SOH), SortSohName);
}

static int SortCnfName(const void *a, const void *b)
{
int result;

    result = strcmp(((ISI_STREAM_CNF *) a)->name.sta, ((ISI_STREAM_CNF *) b)->name.sta);
    if (result != 0) return result;

    result = strcmp(((ISI_STREAM_CNF *) a)->name.chn, ((ISI_STREAM_CNF *) b)->name.chn);
    if (result != 0) return result;

    return strcmp(((ISI_STREAM_CNF *) a)->name.loc, ((ISI_STREAM_CNF *) b)->name.loc);
}

static VOID SortCnf(ISI_CNF_REPORT *cnf)
{
    qsort(cnf->entry, cnf->nentry, sizeof(ISI_STREAM_CNF), SortCnfName);
}


void JSONPost(char *url, char *json) {
/* 
    NOTE: 
    This may be refactorable to improve performance by 
    moving curl initialization to outside of loop just updating the 
    CURLOPTS_POSTFIELDS with each iteration

    TODO: 
    Check configuration curl Timeout settings to handle invalid url.
    Looks like default may be 60 seconds
*/

CURL *curl;
CURLcode res;
struct curl_slist *list = NULL;
char errbuf[CURL_ERROR_SIZE];
  /* In windows, this will init the winsock stuff */ 
  curl_global_init(CURL_GLOBAL_ALL);
 
  /* get a curl handle */ 
  curl = curl_easy_init();
  if(curl) {
    /* First set the URL that is about to receive our POST. This URL can
       just as well be a https:// URL if that is what should receive the
       data. */ 
    curl_easy_setopt(curl, CURLOPT_URL, url);

    // set HTTP headers
    list = curl_slist_append(list, "Accept:");
    list = curl_slist_append(list, "Content-Type: application/json");
    list = curl_slist_append(list, "Accept-Language: en-us");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, list);

    /* Now specify the actual JSON payload as POST data */ 
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json);

    /* provide a buffer to store errors in */
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errbuf);
 
    /* Perform the request, res will get the return code */ 
    res = curl_easy_perform(curl);
    /* Check for errors */ 
    if(res != CURLE_OK) {
        fprintf(stderr, "\n\ncurl_easy_perform() failed: %s\n\n", errbuf);
        exit(1);
    }

    curl_slist_free_all(list); /* free the header list */

    /* always cleanup */ 
    curl_easy_cleanup(curl);

  } else {
     fprintf(stderr, "\nCould not initialize curl_easy_init()\n");
  }
  curl_global_cleanup();
}



json_t *ChannelStatus(char *sta, char *chn, char *loc, REAL64 srate, UINT32 nseg, REAL64 tols, REAL64 tslw)
{
UINT32 latency;
char tbuf[1024];
time_t atime_t;
struct tm atime_tm;

json_t *j_cnf_obj;
// json_error_t *j_err;

    // initializer cnf json obj
    j_cnf_obj = json_object();

    time_t now = time(NULL);
    latency = now - (UINT32) tols;

    json_object_set(j_cnf_obj, "chn", json_string(chn));
    json_object_set(j_cnf_obj, "loc", json_string(loc));

    // use UTC time and format for PostGreSQL "timestamp without a timezone (treated as UTC)"
    strftime(tbuf, sizeof(tbuf), "%Y-%m-%d %H:%M:%S -00:00", gmtime_r(&now, &atime_tm));
    json_object_set(j_cnf_obj, "reported_at", json_string(tbuf));

    json_object_set(j_cnf_obj, "freq", json_real(srate));
    json_object_set(j_cnf_obj, "nseg", json_integer(nseg));
    
    if (tols != (REAL64) ISI_UNDEFINED_TIMESTAMP) {

        // these only exist when for channels with actual telemtry data

        // use UTC time and format for PostGreSQL "timestamp without a timezone (treated as UTC)"
        atime_t = (const time_t)tols;
        strftime(tbuf, sizeof(tbuf), "%Y-%m-%d %H:%M:%S -00:00", gmtime_r(&atime_t, &atime_tm));
        json_object_set(j_cnf_obj, "most_recent_data",      json_string(tbuf));
        json_object_set(j_cnf_obj, "data_latency",   json_integer((INT32)latency));

        // use UTC time and format for PostGreSQL "timestamp without a timezone (treated as UTC)"
        atime_t = (const time_t)(now - tslw);
        strftime(tbuf, sizeof(tbuf), "%Y-%m-%d %H:%M:%S -00:00", gmtime_r(&atime_t, &atime_tm));
        json_object_set(j_cnf_obj, "last_write",      json_string(tbuf));
        json_object_set(j_cnf_obj, "write_latency",   json_integer((INT32)tslw));

    }

    return j_cnf_obj;

}


static json_t *StationStatus(char *sta, REAL64 tols, REAL64 tslw, UINT32 livechn, UINT32 nseg)
{
UINT32 data_latency;
char tbuf[1024];
time_t atime_t;
struct tm atime_tm;

    json_t *j_res = json_object(); 

    time_t now = time(NULL);
    data_latency = time(NULL) - (UINT32) tols;

    // set fields into JSON structs...
    json_object_set(j_res, "sta", json_string(sta));

    // use UTC time and format for PostGreSQL "timestamp without a timezone (treated as UTC)"
    strftime(tbuf, sizeof(tbuf), "%Y-%m-%d %H:%M:%S -00:00", gmtime_r(&now, &atime_tm));
    json_object_set(j_res, "reported_at", json_string(tbuf));

    json_object_set(j_res, "livechn_pcnt", json_integer(livechn));

    if (tols != (REAL64) ISI_UNDEFINED_TIMESTAMP) {

        // these only exist when the station has one or more channels with actual telemtry data

        json_object_set(j_res, "nseg_avg", json_integer(nseg));

        // use UTC time and format for PostGreSQL "timestamp without a timezone (treated as UTC)"
        atime_t = (const time_t)tols;
        strftime(tbuf, sizeof(tbuf), "%Y-%m-%d %H:%M:%S -00:00", gmtime_r(&atime_t, &atime_tm));
        json_object_set(j_res, "sta_most_recent_data", json_string(tbuf));
        json_object_set(j_res, "sta_data_latency_secs",   json_integer((INT32)data_latency));

        // use UTC time and format for PostGreSQL "timestamp without a timezone (treated as UTC)"
        atime_t = (const time_t)(now - tslw);
        strftime(tbuf, sizeof(tbuf), "%Y-%m-%d %H:%M:%S -00:00", gmtime_r(&atime_t, &atime_tm));
        json_object_set(j_res, "sta_last_write_at",      json_string(tbuf));
        json_object_set(j_res, "sta_write_latency_secs",   json_integer((INT32)tslw));

    }

    return j_res;
}

static void ProcessSohInfo(char * url, ISI_SOH_REPORT *soh, ISI_CNF_REPORT *cnf)
{
/*
    Loop through Soh and Cnf contruct "soh" status for each station as JSON
    HTTP Post to DEFAULT_IDASTAT_API_SOH_URL
    Each station posted with separate HTTP call
*/

// summary vars for each sta
int livechn; // percentage of "live" CHNs for STA
int totchn; // total # of CHN's for STA

int sumnseg,totnseg, nsegval;

REAL64 sumtols;
REAL64 sumtslw;
UINT32 nseg;
// ISI_STREAM_CNF *cnfList[soh->nentry];

// loop control vars
UINT32 ndx = 0;
char *cur_sta, *new_sta;
static char *Blank = "    ";

// json related vars
json_t *j_root;
json_t *j_stat_res;
json_t *j_chn_arr;


    if (soh->nentry > 0) {

        // initializer root json obj
        j_root = json_object();
        // create chn array for sta channels
        j_chn_arr = json_array();

        // initialize everything
        cur_sta = soh->entry[ndx].name.sta;
        new_sta = soh->entry[ndx].name.sta;
        ndx = 0;

        sumtols = (REAL64) ISI_UNDEFINED_TIMESTAMP;
        sumtslw = (REAL64) ISI_UNDEFINED_TIMESTAMP;
        nseg = 0;
        livechn = 0;
        totchn = 0;
        totnseg = 0;
        sumnseg = 0;

        // loop through all soh entries...
        do {

            // loop through chn records for sta summary stats
            do {

                if (verbose) { fprintf(stdout, "Processing (sta chn loc) : %s %s %s\n", soh->entry[ndx].name.sta, soh->entry[ndx].name.chn, soh->entry[ndx].name.loc); }

                if (soh->entry[ndx].tols.value != (REAL64) ISI_UNDEFINED_TIMESTAMP) {
                    if (sumtols == (REAL64) ISI_UNDEFINED_TIMESTAMP || soh->entry[ndx].tols.value > sumtols) {
                        sumtols = soh->entry[ndx].tols.value;
                    }
                }
                nseg = soh->entry[ndx].nseg;
                sumnseg = sumnseg + nseg;

                if (soh->entry[ndx].tslw != (REAL64) ISI_UNDEFINED_TIMESTAMP) {
                    if (sumtslw == (REAL64) ISI_UNDEFINED_TIMESTAMP || soh->entry[ndx].tslw < sumtslw) {
                        sumtslw = soh->entry[ndx].tslw;
                    }
                }

                if ((soh->entry[ndx].nrec != 0) && (UINT32) sumtslw <= LIVE_LATENCY_THRESHOLD) livechn++; // if channel is reporting, count it

                // count total channels
                totchn++;    

                // count total nseg for chn if value > 0, used to calc avg nseg for only chn's with nseg > 0
                // totnseg is the number of channels for this sta with > 0 nsegs
                if (nseg > 0) totnseg++;

                REAL64 ssrate = 1.0 / isiSrateToSint(&cnf->entry[ndx].srate);

                int err = json_array_append(j_chn_arr, ChannelStatus(cur_sta, soh->entry[ndx].name.chn, soh->entry[ndx].name.loc, ssrate, soh->entry[ndx].nseg, soh->entry[ndx].tols.value, soh->entry[ndx].tslw));
                if (err != 0) { fprintf(stderr, "Error construcrting channel satus array in StationStatus: %d", err); }

                if (verbose) { fprintf(stdout, "Will append channel info for station: %s\n", cur_sta); }

                // increm,ent loop ndx and 
                // get sta for next chn, or set to blank if at end of list
                ndx++;
                new_sta = (ndx < soh->nentry) ? soh->entry[ndx].name.sta : Blank;

            } while (strcmp(cur_sta, new_sta) == 0);

            // at end of cur_sta channels...

            // compute summary stats for cur_sta
            if (totchn > 0) { livechn = (livechn * 100) / totchn; } else { livechn = 0; }
            if (totnseg > 0) { nsegval = sumnseg / totnseg; } else { nsegval = 0; }


            // construct JSON 
            // get station level results
            j_stat_res = StationStatus(cur_sta, sumtols, sumtslw, livechn, nsegval);
            // append chhannel info...
            json_object_set(j_stat_res, "chns_attributes", j_chn_arr);

            // set station status object into root
            json_object_set(j_root, "soh", j_stat_res);

            if (verbose) { fprintf(stdout, "Will create JSON for station: %s\n", cur_sta); }

            // dump json_t root object to (char *)
            char *json = json_dumps(j_root, JSON_REAL_PRECISION(2) | JSON_ENSURE_ASCII | JSON_INDENT(4) | JSON_PRESERVE_ORDER);

            // Send for Posting...
            if (verbose) { fprintf(stdout, "Will POST JSON for station: %s to %s\n", cur_sta, url); }
            JSONPost(url, json);
            if (verbose) { fprintf(stdout, "Did POST JSON for station: %s to %s\n", cur_sta, url); }

            // cleanup...
            free(json);
            // free j_stat_res created by calling function PrintStatusResults
            free(j_stat_res);

            // clear root 'soh' objects and channel array
            json_array_clear(j_chn_arr);
            json_object_clear(j_root);

            // re-=initialize for next sta
            sumtols = (REAL64) ISI_UNDEFINED_TIMESTAMP;
            sumtslw = (REAL64) ISI_UNDEFINED_TIMESTAMP;
            nseg = 0;
            livechn = 0;
            totchn = 0;
            totnseg = 0;
            sumnseg = 0;

            // set cur_stat to new_sta for next sta
            cur_sta = new_sta;

        } while (ndx < soh->nentry);

        // at end of soh list... our work hrer is done
        // free json mem
        free(j_chn_arr);
        free(j_root);

    }
}

static void help(char *myname)
{
static char *VerboseHelp = 
"The arguments in [ square brackets ] are optional:\n"
"\n"
"isiserver=string  default: idahub.ucsd.edu \n"
"apisohurl=string  default: http://idaprog.ucsd.edu/api/nrts/v1/sohs \n"
;
    fprintf(stderr, "%s %s\n\n", myname, VersionIdentString);
    fprintf(stderr,"usage: %s [ isiserver=string apisohurl=string ] ", myname);
    fprintf(stderr, "\n");
    fprintf(stderr, "%s\n", VerboseHelp);
    exit(1);

}

int main (int argc, char **argv)
{
int i;

ISI_PARAM par;


ISI_SOH_REPORT *soh;
ISI_CNF_REPORT *cnf;

char *isiserver = DEFAULT_SERVER;
char *apisohurl = DEFAULT_IDASTAT_API_SOH_URL;

    utilNetworkInit();
    isiInitDefaultPar(&par);
	now = time(NULL);
    gmtime_r(&now, &gm.tm);
    asctime_r(&gm.tm, gm.buf);
    gm.buf[strlen(gm.buf)-1] = 0;
    localtime_r(&now, &local.tm);
    asctime_r(&local.tm, local.buf);
    local.buf[strlen(local.buf)-1] = 0;

    for (i = 1; i < argc; i++) {

        if (strncmp(argv[i], "isiserver=", strlen("isiserver=")) == 0) {

            isiserver = argv[i] + strlen("isiserver=");

        } else if (strncmp(argv[i], "apisohurl=", strlen("apisohurl=")) == 0) {

            apisohurl = argv[i] + strlen("apisohurl=");

        } else if (strcmp(argv[i], "-v") == 0) {

            verbose = TRUE;

        } else if (strcmp(argv[i], "-help") == 0) {

            help(argv[0]);

        } else {

           fprintf(stderr, "%s: unexpected argument: '%s'\n", argv[0], argv[i]);
           help(argv[0]);
        }
    }
    if (verbose) fprintf(stdout, "\n%s %s\n", argv[0], VersionIdentString);

    // get ISI_SOH_REPORT list
    if ((soh = isiSoh(isiserver, &par)) == NULL) {
        perror("isiSoh");
        exit(1);
    }

    // get ISI_CNF_REPORT list
    if ((cnf = isiCnf(isiserver, &par)) == NULL) {
        perror("isiCnf");
        exit(1);
    }

    // upper case station names using ISI_STREAM_SOH.name.sta and ISI_STREAM_CNF.name.sta
    UcaseStationName(soh, cnf);

    // sort SOH and CNF each by name.sta
    // it is assumed that soh and cnf will be the same length entries for the same channels/stations
    SortSoh(soh);
    SortCnf(cnf);

    // process soh/cnf lists and post data by station to "apisohurl"
    ProcessSohInfo(apisohurl, soh, cnf);

    exit(0);
}

/* Revision History
 *
 * $Log: main.c,v $
 * Revision 1.23  2014/01/27 18:19:45  dechavez
 * Changes to accomodate move to new (NetOps) VM server.  Removed some commented debug code.
 *
 * Revision 1.22  2013/12/17 15:46:18  dechavez
 * Removed MySQL dependencies, added update timestamp to header
 *
 * Revision 1.21  2007/12/11 19:25:04  judy
 * added nseg to main page
 *
 * Revision 1.20  2007/10/03 21:14:00  judy
 * Cleaned up help detail.
 *
 * Revision 1.19  2007/06/20 18:49:34  judy
 * Fixed Safari browser incompatibilities
 *
 * Revision 1.18  2007/06/19 18:12:01  judy
 * Color for link latency in channel view independent of the other columns
 *
 * Revision 1.16  2007/06/11 22:23:38  judy
 * Replaced page1,2,3 design to station/channel pages only (no more "page 1").
 * User specifies webserver, isi server and htdocs path
 *
 * Revision 1.15  2007/06/04 19:14:47  judy
 * Added page 3 option (channel pages and nrts_mon style navigation between stations)
 *
 * Revision 1.14  2007/02/01 19:41:05  judy
 * Fixed parchment display
 *
 * Revision 1.13  2007/01/25 22:21:12  judy
 * Added links to FDSN station pages, used CSS style sheets for link font color
 *
 * Revision 1.12  2007/01/11 18:05:47  dechavez
 * reverted to 1.3.3 since 1.4.0 was prematurely committed
 *
 * Revision 1.11  2006/11/13 17:21:18  judy
 * added db option and used (new) system flags to select which stations to ignore
 *
 * Revision 1.10  2006/10/19 23:52:51  dechavez
 * Added server name to title, moved isiwww version down by the timestamp at the bottom of the page
 *
 * Revision 1.9  2006/10/19 23:09:46  judy
 * Added parchment background
 *
 * Revision 1.8  2006/08/17 19:50:32  judy
 * Re-title and add refresh every 40 seconds
 *
 * Revision 1.7  2006/08/16 21:44:33  judy
 * Adopt BUD style color conventions and added data latency and channel counts
 *
 * Revision 1.6  2005/10/19 23:25:16  dechavez
 * Show latencies of more than one hour and less than one day in orange
 *
 * Revision 1.5  2005/06/10 15:38:10  dechavez
 * Rename isiSetLogging() to isiStartLogging()
 *
 * Revision 1.4  2005/03/23 20:45:40  dechavez
 * Print UTC time in YYYY:DDD-HH:MM:SS format
 *
 * Revision 1.3  2005/02/23 17:29:48  dechavez
 * truncate seconds from page 1 display
 *
 * Revision 1.2  2005/02/23 01:42:08  dechavez
 * display local time correctly
 *
 * Revision 1.1  2005/02/23 00:41:23  dechavez
 * initial release
 *
 */

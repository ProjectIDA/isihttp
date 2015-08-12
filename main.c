#pragma ident "$Id: main.c,v 1.23 2014/01/27 18:19:45 dechavez Exp $"
/*======================================================================
 *
 *  Generate IDA latency web pages
 *
 *====================================================================*/
#include "isi.h"
#include "util.h"

extern char *VersionIdentString;

static time_t now;
static struct {
    struct tm tm;
    char buf[64];
} gm, local;

#ifndef DEFAULT_SERVER
#define DEFAULT_SERVER "idahub.ucsd.edu"
#endif /* DEFAULT_SERVER */

#ifndef DEFAULT_WEBSERVER
#define DEFAULT_WEBSERVER "ida.ucsd.edu"
#endif /* DEFAULT_WEBSERVER */

#ifndef DEFAULT_WEBPATH
#define DEFAULT_WEBPATH "Telemetry/"
#endif /* DEFAULT_WEBPATH */

#ifndef DEFAULT_STATIONDIR
#define DEFAULT_STATIONDIR "Stations/"
#endif /* DEFAULT_STATIONDIR */

#ifndef DEFAULT_HTDOCPATH
#define DEFAULT_HTDOCPATH "/ida/web"
#endif

#ifndef SAMP_PER_PACKET 
#define  SAMP_PER_PACKET 240
#endif /*  SAMP_PER_PACKET */

#define LIVE_LATENCY_THRESHOLD 21600  // 6 hrs

typedef struct {
    int value;
    int percent;
    int bg;
    int fg;
} COLOR_MAP;

#define NumColors 10
#define NumColorsNseg 3

#define PURPLE 0xCC99FF
#define BLUE   0x3399FF
#define GREEN  0x00FF00
#define YELLOW 0xFFFF00
#define TAN    0xFF9966
#define RED    0xFF3333
#define PINK   0xFFCCCC
#define LTGRAY 0xCCCCCC
#define MDGRAY 0x999999
#define DKGRAY 0x666666
#define BLACK  0x000000
#define WHITE  0xffffff

static COLOR_MAP ColorMap[] = {
    { 432000, 0,  DKGRAY, WHITE },   //  5 days
    { 345600, 10, MDGRAY, WHITE },   //  4 days 
    { 259200, 25, LTGRAY, BLACK },   //  3 days 
    { 172800, 37, PINK,   BLACK },   //  2 day 
    {  86499, 50, RED,    BLACK },   //  1 day 
    {  21600, 66, TAN,    BLACK },   //  6 hrs 
    {   7200, 75, YELLOW, BLACK },   //  2 hr
    {   3600, 87, GREEN,  BLACK },   //  1 hr
    {   1800, 100, BLUE,   BLACK },   // 30 min
    {      0, 100, PURPLE, BLACK},
    {     -1, 0, 0, 0}
};

static COLOR_MAP ColorMapNseg[] = {
    {     10, 10,  GREEN,  BLACK},    //  lt 10
    {     50, 50,  YELLOW, BLACK},    //  lt 50
    {     51, 51,  RED,    BLACK}     //  gt 50
};

static int PrintErrorMessageAndExit()
{
   fprintf(stderr, "Problem establishing Color Values \n");
   exit(0);
}

static int GetBg(UINT32 test)
{
int i;
    for (i = 0; ColorMap[i].value >= 0; i++) {
        if (test >= ColorMap[i].value) return ColorMap[i].bg;
    }
    PrintErrorMessageAndExit();
}

static int GetFg(UINT32 test)
{
int i;
    for (i = 0; ColorMap[i].value >= 0; i++) {
        if (test >= ColorMap[i].value) return ColorMap[i].fg;
    }
    PrintErrorMessageAndExit();
}

static int GetPercent(UINT32 test)
{
int i;
    for (i = 0; ColorMap[i].value >= 0; i++) {
        if (test >= ColorMap[i].value) return ColorMap[i].percent;
    }
    PrintErrorMessageAndExit();
}

static int GetPercentbg(UINT32 test)
{
int i;
    for (i = (NumColors-1); i >= 0; i--) {
        if (test >= ColorMap[i].percent) return ColorMap[i].bg;
    }
    PrintErrorMessageAndExit();
}

static int GetPercentfg(UINT32 test)
{
int i;
    for (i = (NumColors-1); i >= 0; i--) {
        if (test >= ColorMap[i].percent) return ColorMap[i].fg;
    }
    PrintErrorMessageAndExit();
}

static int GetNsegbg(UINT32 test)
{
int i, indexval;
    for (i = 0; (i < NumColorsNseg);  i++) {
        if (test < ColorMapNseg[i].percent) return ColorMapNseg[i].bg; 
    }
    return ColorMapNseg[2].bg;
}

static int Getnsegfg(UINT32 test)
{
    if (test < 100) return ColorMapNseg[0].fg;
    return ColorMapNseg[2].fg;
}

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

static void PrintTimeStamp(FILE *fp)
{
    fprintf(fp,"<CENTER>");
    fprintf(fp, "<I>");
    fprintf(fp, "Page last updated %s UTC ", utilLttostr(now, 0, gm.buf));
    //fprintf(fp, "(%s %s)", local.buf, tzname[0]);
    //fprintf(fp, "<BR><FONT SIZE=\"2.25\">isiwww %s", VersionIdentString);
    fprintf(fp, "</I>\n");
    fprintf(fp,"</CENTER>\n");
}

static void MakeTableforSafari(FILE *fp)
{
    fprintf(fp, "<style type=\"text/css\" media=\"all\">\n");
    fprintf(fp, "TABLE {\n");
    fprintf(fp, "color: #fff; \n");
    fprintf(fp, "} \n");
}


static void MakeLinkColorSS(FILE *fp)
{
    fprintf(fp, "<style type=\"text/css\" media=\"all\">\n");
    fprintf(fp, "a.broken:link {\n");
    fprintf(fp, "color: #fff; \n");
    fprintf(fp, "} \n");

    fprintf(fp, "a.broken:visited { \n");
    fprintf(fp, "color: #fff; \n");
    fprintf(fp, "} \n");

    fprintf(fp, "a.working:link {\n");
    fprintf(fp, "color: #000; \n");
    fprintf(fp, "} \n");

    fprintf(fp, "a.working:visited { \n");
    fprintf(fp, "color: #000; \n");
    fprintf(fp, "} \n");
    fprintf(fp, "</style>\n");
}

static void PrintChnPgLegend(FILE *fp)
{
char *phrase[NumColors];
int  percent[NumColors];
int bgcolor[NumColors];
int fgcolor[NumColors];
int  i=0;
int  j=0;

    fprintf(fp, "<CENTER><TABLE HSPACE=\"5\" BORDER=\"1\">\n");
    fprintf(fp, "<TH COLSPAN=\"11\">Latency Indicators (* denotes link latency)</TH>\n");
    fprintf(fp, "<tr>");
    for (i = 0; ColorMap[i].value >= 0; i++) {
      if (GetBg(i) > 0) {
        if (ColorMap[i].value / 432000) { phrase[i] = "gt 5 days";
        } else if (ColorMap[i].value / 345600) { phrase[i] = "gt 4 days";
        } else if (ColorMap[i].value / 259200) { phrase[i] = "gt 3 days";
        } else if (ColorMap[i].value / 172800) { phrase[i] = "gt 2 days";
        } else if (ColorMap[i].value / 86400)  { phrase[i] = "gt 1 day";
        } else if (ColorMap[i].value / 21600)  { phrase[i] = "gt 6 hours";
        } else if (ColorMap[i].value / 7200)   { phrase[i] = "gt 2 hours";
        } else if (ColorMap[i].value / 3600)   { phrase[i] = "gt 1 hour";
        } else if (ColorMap[i].value / 1800)   { phrase[i] = "gt 30 minutes";
        } else {
            phrase[i] = "lt 30 minutes";
        }
        bgcolor[i] = GetBg(ColorMap[i].value);
        fgcolor[i] = GetFg(ColorMap[i].value);
        percent[i] = GetPercent(ColorMap[i].value);
      }
    }
    for (j = (NumColors-1); j >= 0; j--) {
        fprintf(fp, "<td bgcolor=\"%06x\" align=\"center\"><FONT COLOR=\"%06x\"><B>&%s</B></TD>\n",
             bgcolor[j],fgcolor[j],phrase[j]);
    }
    fprintf(fp, "</tr>");
    fprintf(fp, "</TABLE></CENTER>");
}

static void PrintStatusLegend(FILE *fp)
{
char *phrase[NumColors];
char *nsegphrase[NumColors];
int  percent[NumColors];
int bgcolor[NumColors];
int fgcolor[NumColors];
int  i=0;
int  j=0;

    fprintf(fp, "<CENTER><TABLE HSPACE=\"5\" BORDER=\"1\">\n");
    fprintf(fp, "<TH COLSPAN=\"11\">Latency Indicators (* denotes link latency)</TH>\n");
    fprintf(fp, "<tr>");
    for (i = 0; ColorMap[i].value >= 0; i++) {
      if (GetBg(i) > 0) {
        if (ColorMap[i].value / 432000) { 
            phrase[i] = "gt 5 days";
        } else if (ColorMap[i].value / 345600) { 
            phrase[i] = "gt 4 days";
        } else if (ColorMap[i].value / 259200) { 
            phrase[i] = "gt 3 days";
        } else if (ColorMap[i].value / 172800) { 
            phrase[i] = "gt 2 days";
        } else if (ColorMap[i].value / 86400)  { 
            phrase[i] = "gt 1 day";
        } else if (ColorMap[i].value / 21600)  { 
            phrase[i] = "gt 6 hours";
        } else if (ColorMap[i].value / 7200)   { 
            phrase[i] = "gt 2 hours";
        } else if (ColorMap[i].value / 3600)   { 
            phrase[i] = "gt 1 hour";
        } else if (ColorMap[i].value / 1800)   { 
            phrase[i] = "gt 30 minutes";
        } else {
            phrase[i] = "lt 30 minutes";
        }
        bgcolor[i] = GetBg(ColorMap[i].value);
        fgcolor[i] = GetFg(ColorMap[i].value);
        percent[i] = GetPercent(ColorMap[i].value);
      }
    }
    for (j = (NumColors-1); j >= 0; j--) {
        fprintf(fp, "<td bgcolor=\"%06x\" align=\"center\"><FONT COLOR=\"%06x\"><B>&%s</B></TD>\n",
             bgcolor[j],fgcolor[j],phrase[j]);
    }
    fprintf(fp, "</TR><tr><TH colspan=\"11\">Channel Completeness Indicators</TH></TR>\n");
    fprintf(fp, "</tr>");
    for (j = (NumColors-1); j >= 0; j--) {
        if ((j == (NumColors-1)) || (j == 0)) {
           fprintf(fp, "<td bgcolor=\"%06x\" align=\"center\"><font color=\"%06x\"><b>%i%%</b></TD>\n",bgcolor[j],fgcolor[j],percent[j]);
        } else {
        fprintf(fp, "<td bgcolor=\"%06x\" align=\"center\"><font color=\"%06x\"><b> < %i%% </b></TD>\n",bgcolor[j],fgcolor[j],percent[j]);
        }
    }

    for (i = 0; (i < NumColorsNseg); i++) {
         if (ColorMapNseg[i].value <= 10) {
              nsegphrase[i] = "lt 10";
              bgcolor[i] = ColorMapNseg[i].bg;
              fgcolor[i] = ColorMapNseg[i].fg;
         } else if (ColorMapNseg[i].value <= 50) {
              nsegphrase[i] = "lt 50";
              bgcolor[i] = ColorMapNseg[i].bg;
              fgcolor[i] = ColorMapNseg[i].fg;
         } else {
              nsegphrase[i] = "gt 50";
              bgcolor[i] = ColorMapNseg[i].bg;
              fgcolor[i] = ColorMapNseg[i].fg;
         }
    }


    fprintf(fp, "</TR><tr><TH COLSPAN=\"11\">Segment Indicators</TH>\n");
    fprintf(fp, "</tr>");

    for (j = 0; (j < NumColorsNseg-1); j++) {
         fprintf(fp, "<td COLSPAN=\"3\" bgcolor=\"%06x\" align=\"center\"><FONT COLOR=\"%06x\"><B>&%s</B></TD>\n",
              bgcolor[j],fgcolor[j],nsegphrase[j]);
    }
    fprintf(fp, "<td COLSPAN=\"4\" bgcolor=\"%06x\" align=\"center\"><FONT COLOR=\"%06x\"><B>&%s</B></TD>\n", bgcolor[NumColorsNseg-1],fgcolor[NumColorsNseg-1],nsegphrase[NumColorsNseg-1]);
    fprintf(fp, "</tr>");
    fprintf(fp, "</TABLE></CENTER>\n");
}

static void PrintStatusHeader(FILE *fp, char *isiserver, char *htdocpath, char *webstuff)
{
char *parchstr="parchment.jpg";
char parchvar[MAXPATHLEN+1];
parchvar[0]=0;

    fprintf(fp, "<HEAD><META HTTP-EQUIV=\"Refresh\" CONTENT=\"40\">\n");
    fprintf(fp, "<title>IDA/NRTS (%s)</title></HEAD>\n",isiserver);
    sprintf(parchvar,"http://%s/%s",webstuff,parchstr);
    fprintf(fp, "<BODY background=\"%s\">\n",parchvar);
    fprintf(fp, "<CENTER><h2>IDA/NRTS Status at %s</h2></CENTER>\n", isiserver);
    PrintStatusLegend(fp);
    PrintTimeStamp(fp);
    
//    Accommodate multiple color links
    MakeLinkColorSS(fp);

    fprintf(fp, "<BR><CENTER><TABLE border=\"1\">");
    fprintf(fp, "<tr><B>\n");
    fprintf(fp, "<th>STA</th><th>%% CHN</th><th>NSEG</th><th>LAST DATUM</th><th> DATA LATENCY </th><th> LINK LATENCY </TH>\n");

    fprintf(fp, "</B></tr>\n");
}

static void PrintStatusTrailer(FILE *fp)
{
    fprintf(fp, "<TFOOT>");
    fprintf(fp, "<tr><th><B>STA</th><th>%% CHN</th><th>NSEG</th><th>LAST DATUM</th><th> DATA LATENCY </th><th> LINK LATENCY </TH></B></th></tr>\n");
    fprintf(fp, "</table></CENTER>\n");
    fprintf(fp, "</BODY>");
    fprintf(fp, "<BR>");
}

static void PrintStatusResult(FILE *fp, char *sta, char *isiserver, char *webserver, char *webpath, char *stationdir, REAL64 tols, REAL64 tslw, UINT32 livechn, UINT32 nseg)
{
UINT32 latency;
char tbuf[1024];

char *str1 = "index.html";
char *str2 = "chan.html";
char stahomelink[MAXPATHLEN+1];
char chanlink[MAXPATHLEN+1];
int i;
stahomelink[0]=0;
chanlink[0]=0;

    sprintf(stahomelink, "http://%s/?q=station/%s", webserver, util_lcase(sta));
    sprintf(chanlink, "http://%s/%s/%s/%s%s", webserver, webpath, isiserver, util_ucase(sta), str2);

    fprintf(fp, "<tr>");
    if (tols != (REAL64) ISI_UNDEFINED_TIMESTAMP) {
        latency = time(NULL) - (UINT32) tols;
        fprintf(fp, "<td align=\"center\" bgcolor=\"%06x\">",GetBg(latency));
        if (GetBg(latency)) {
            if (stationdir != NULL) {
                fprintf(fp, "\n<A HREF=\"%s\" class=\"visited\">",stahomelink);
            }
            else {
                fprintf(fp,"\n%s",sta);
            }
                fprintf(fp, "<font color=\"%06x\">%s</A>\n<font size=\"+3\" color=\"%06x\"><B>*</B></td>\n", 
                    GetFg(tslw),util_ucase(sta),GetBg(tslw));
        } else {
            if (stationdir != NULL) {
                fprintf(fp, "\n<A HREF=\"%s\" class=\"visited\">",stahomelink);            
            }
            else {
                fprintf(fp,"\n%s",sta);
            }
            fprintf(fp, "<font color=\"%06x\">%s</A>\n<font size=\"+3\" color=\"%06x\"><B>*</B></td>\n",
              WHITE,util_ucase(sta),GetBg(tslw));
        }
        fprintf(fp, "<td align=\"center\" bgcolor=\"%06x\">",GetPercentbg(livechn));
        fprintf(fp, "<A HREF=\"%s\">",chanlink);
        fprintf(fp, "<font color=\"%06x\">%i</A></td>\n",GetPercentfg(livechn),livechn);
        fprintf(fp, "<td style=\"width:5px\" align=\"center\" bgcolor=\"%06x\">\n",GetNsegbg(nseg));
        fprintf(fp, "<font color=\"%06x\">%i</td>\n",Getnsegfg(nseg),nseg);       
        fprintf(fp, "<td align=\"center\" width=150>%s</td>\n",utilLttostr((INT32)tols,0,tbuf));
        fprintf(fp, "<td align=\"center\">%s</td>\n",utilLttostr((INT32)latency,8,tbuf));
        fprintf(fp, "<td align=\"center\">%s</td>\n",utilLttostr((INT32)tslw,8,tbuf));
    } else {
        fprintf(fp, "<td bgcolor=\"%06x\"><font color=\"%06x\">",DKGRAY,WHITE);
        if (stationdir != NULL) {
            fprintf(fp, "\n<A HREF=\"%s\" class=\"broken\">",stahomelink);
            fprintf(fp, "%s</A></td>\n",util_ucase(sta));        }
        else {
            fprintf(fp,"\n%s",sta);
        }
        fprintf(fp, "<td align=\"center\" bgcolor=\"%06x\">\n",DKGRAY);
        fprintf(fp, "<A HREF=\"%s\">",chanlink);
        fprintf(fp, "<font color=\"%06x\">%i</td></A>\n",WHITE,livechn);
        fprintf(fp, "<td align=\"center\">n/a</td>\n");
        fprintf(fp, "<td align=\"center\">n/a</td>\n");
        fprintf(fp, "<td align=\"center\">n/a</td>\n");
        fprintf(fp, "<td align=\"center\">n/a</td>\n");
    }
    fprintf(fp, "</tr>\n");
}

static void PrintStatus(FILE *fp, ISI_SOH_REPORT *soh, ISI_CNF_REPORT *cnf, char *isiserver, char *webserver, char *htdocpath, char *webpath, char *stationdir)
{
int i;
int livechn,totchn;
int sumnseg,totnseg, nsegval;
REAL64 tols;
REAL64 tslw;
UINT32 nseg;
//ISI_NSEG dest;
char *sta;
static char *Blank = "    ";
static BOOL First = TRUE;
char webstuff[MAXPATHLEN+1];
webstuff[0]=0;

    sprintf(webstuff,"%s/%s",webserver,webpath);
    PrintStatusHeader(fp, isiserver, htdocpath, webstuff);
    sta = Blank;
    for (i = 0; i < soh->nentry; i++) {
        if (strcmp(sta, soh->entry[i].name.sta) != 0) {                     //  new station
            if (!First) {
                if (totchn > 0) {
                    livechn = (livechn * 100) / totchn;
                } else {
                    livechn = 0;
                }
                if (totnseg > 0) {
                    nsegval = sumnseg / totnseg;
                } else {
                    nsegval = 0;
                }
                PrintStatusResult(fp, sta, isiserver, webserver, webpath, stationdir, tols, tslw, livechn, nsegval);
            }
            sta = soh->entry[i].name.sta;
            tols = (REAL64) ISI_UNDEFINED_TIMESTAMP;
            tslw = (REAL64) ISI_UNDEFINED_TIMESTAMP;
            nseg = 0;
            livechn = 0;
            totchn = 0;
            totnseg = 0;
            sumnseg = 0;
        }
        if (soh->entry[i].tols.value != (REAL64) ISI_UNDEFINED_TIMESTAMP) {
            if (tols == (REAL64) ISI_UNDEFINED_TIMESTAMP || soh->entry[i].tols.value > tols) {
                tols = soh->entry[i].tols.value;
            }
        }
        nseg = soh->entry[i].nseg;
        sumnseg = sumnseg + nseg;

        if (soh->entry[i].tslw != (REAL64) ISI_UNDEFINED_TIMESTAMP) {
            if (tslw == (REAL64) ISI_UNDEFINED_TIMESTAMP || soh->entry[i].tslw < tslw) {
                tslw = soh->entry[i].tslw;
            }
        }
        First = FALSE;
        if ((soh->entry[i].nrec != 0) && (UINT32) tslw <= LIVE_LATENCY_THRESHOLD) livechn++; // if channel is reporting, count it
        totchn++;    // count total channels
        if (nseg > 0) totnseg++;    // count total nseg if value > 0
    }

    if (totchn > 0) {
         livechn = (livechn * 100) / totchn;
    } else {
         livechn = 0;
    }
    if (totnseg > 0) {
          nsegval = sumnseg / totnseg;
    } else {
         nsegval = 0;
    }

    PrintStatusResult(fp, sta, isiserver, webserver, webpath, stationdir, tols, tslw, livechn, nsegval);
    PrintStatusTrailer(fp);
    PrintStatusLegend(fp);
    PrintTimeStamp(fp);
}

static int FindStaIndex(LNKLST *slist, char *sta)
{
int i;
char *name;

    for (i = 0; i < slist->count; i++) {
        name = (char *) slist->array[i]; 
        if (!strcmp(name,sta)) return i;
    }
    return -1;
}

static void MakeNextButtons(FILE *fp, LNKLST *slist, char *sta, char *isiserver, char *webstuff)
{
int idx=0;
char *str2 = "chan.html";
char stachanfn[MAXPATHLEN+1];
char *prevsta, *nextsta;
stachanfn[0]=0;

    if ((idx = FindStaIndex(slist,sta)) < 0) {
         perror("MakeNextButtons");
         exit(0);
    }
    if (idx == 0) { 
        prevsta = (char *) slist->array[slist->count-1];
    } else {
        prevsta = (char *) slist->array[idx-1];
    }
    sprintf(stachanfn,"http://%s/%s/%s%s",webstuff,isiserver,prevsta,str2);
    fprintf(fp,"<B><A HREF=\"%s\" class=\"working\"><CENTER>PREVIOUS</A>\n",stachanfn);
    if (idx == ((slist->count)-1)) { 
        nextsta = (char *) slist->array[0];
    } else {
        nextsta = (char *) slist->array[idx+1];
    }
    stachanfn[0] = 0;
    sprintf(stachanfn,"http://%s/%s/%s%s",webstuff,isiserver,nextsta,str2);
    fprintf(fp,"<A HREF=\"%s\" class=\"working\">NEXT</BIG></CENTER></A></B>\n",stachanfn);
}

static void MakeChanPageHeader(FILE *fp, LNKLST *slist, char *isiserver, char *htdocpath, char *webpath, char *webstuff, char *sta)
{
char *name;
int i;
char *parchstr="parchment.jpg";
char parchvar[MAXPATHLEN+1];
parchvar[0]=0;
char *str1 = "ack.html";
char *str2 = "chan.html";
char stachanfn[MAXPATHLEN+1];
char weblink[MAXPATHLEN+1];
weblink[0]=0;
stachanfn[0]=0;
char ackpage[MAXPATHLEN+1];
ackpage[0]=0;

    sprintf(weblink, "%s/%s",webstuff,isiserver);
    sprintf(ackpage, "%s/%s/%s",webstuff,isiserver,str1);
    fprintf(fp, "<HEAD><META HTTP-EQUIV=\"Refresh\" CONTENT=\"40\">\n");
    fprintf(fp, "<title>IDA/NRTS (%s)</title></HEAD>\n",isiserver);
    sprintf(parchvar,"http://%s/%s",webstuff,parchstr);
    fprintf(fp, "<BODY background=\"%s\">\n",parchvar);
    fprintf(fp, "<CENTER><h2>IDA/NRTS Status for Station %s at %s</h2></CENTER>\n", sta,isiserver);
    PrintChnPgLegend(fp);
    fprintf(fp,"<BR><A HREF=\"http://%s/index.html \" class=\"working\"><CENTER><BIG>MAIN PAGE</A></CENTER>\n", weblink);    
    MakeNextButtons(fp, slist, sta, isiserver, webstuff);

//    Print links to the other stations' pages here
    fprintf(fp, "<CENTER><TABLE CELLPADDING=\"5\" border=\"1\">");
    for (i=0;  i < slist->count; i++) {
        name = (char *) slist->array[i]; 
        if (!(i % 10)) fprintf(fp,"<TR>");
        sprintf(stachanfn,"http://%s/%s%s",weblink,name,str2);
        fprintf(fp, "<TD><A HREF=\"%s\">%s</A></TD>\n",stachanfn,name);
    }
    fprintf(fp, "</BR></CENTER></TABLE>");

//    Accommodate multiple color links
    MakeLinkColorSS(fp);

    fprintf(fp, "<BR><CENTER><TABLE border=\"1\">");
    fprintf(fp, "<tr><B>\n");
    fprintf(fp, "<th>STA</th><th>CHAN</th><th>RATE</th><th>NSEG</th><th>LAST DATUM</th><th> DATA LATENCY </th><th> LINK LATENCY </TH>\n");
    fprintf(fp, "</B></tr>\n");
}

static void MakeChanPageTrailer(FILE *fp, LNKLST *slist, char *sta, char *isiserver, char *webstuff)
{
    fprintf(fp, "<TFOOT>");
    fprintf(fp, "<B><tr><th>STA</th><th>CHAN</th><th>RATE</th><th>NSEG</th><th>LAST DATUM</th><th>DATA LATENCY</TH><TH>LINK LATENCY</B></th><TH>ACK</TH></tr>\n");

    fprintf(fp, "</table></CENTER><BR>\n");
    PrintChnPgLegend(fp);
    fprintf(fp,"<BR>");
    MakeNextButtons(fp, slist, sta, isiserver, webstuff);
    fprintf(fp,"<A HREF=\"http://%s/%s/index.html \" class=\"working\"><B>MAIN PAGE</B></A>\n",webstuff,isiserver);
    fprintf(fp, "</BODY></CENTER>");
}

 static void MakeChanPageResult(FILE *fp, char *sta, char *chn, char *loc, REAL64 srate, UINT32 nseg, REAL64 tols, REAL64 tslw)
{
UINT32 latency, tslw_latency;
UINT32 colorlatency;
ISI_SITECHANFLAG dest;
char tbuf[1024];
static BOOL TimeStampsOK;
char chanloc[ISI_CHNLOCLEN+1];
chanloc[0]=0;
UINT32 samprate=0;


    TimeStampsOK = TRUE; /* maybe */
    samprate = SAMP_PER_PACKET / srate;
    fprintf(fp, "<tr>");
    sprintf(chanloc,"%s%s",util_ucase(chn),loc);
    if (tols != (REAL64) ISI_UNDEFINED_TIMESTAMP) {
        latency = time(NULL) - (UINT32) tols;
        tslw_latency = (UINT32) tslw;
        if (latency <= samprate) {
            colorlatency = 10;
        } else {
            colorlatency = latency;
        }
        if (GetBg(colorlatency) != DKGRAY) {
            fprintf(fp,"<td bgcolor=\"%06x\"><FONT COLOR=\"%06x\">%s</td>\n",
                GetBg(colorlatency),BLACK,sta);
            fprintf(fp, "<td bgcolor=\"%06x\"><FONT COLOR=\"%06x\">%s</TD>\n",
                GetBg(colorlatency),BLACK,chanloc);
            fprintf(fp, "<td bgcolor=\"%06x\" align=\"center\"><FONT COLOR=\"%06x\">%4.2f</td>\n",
                GetBg(colorlatency),BLACK,srate);
//  ADD CODE FOR NSEG COLOR
/*            fprintf(fp,"<td bgcolor=\"%06x\" align=\"center\"><FONT COLOR=\"%06x\">%i</td>\n",
                GetBg(colorlatency),BLACK,nseg);
*/
            fprintf(fp,"<td colspan=\"1\" bgcolor=\"%06x\" align=\"center\"><FONT COLOR=\"%06x\">%i</td>\n",
                GetNsegbg(nseg),BLACK,nseg);
            if (tols != (REAL64) ISI_UNDEFINED_TIMESTAMP) {
                fprintf(fp,"<td bgcolor=\"%06x\" align=\"center\" width=150><FONT COLOR=\"%06x\">%s</td>\n",
                    GetBg(colorlatency),BLACK,utilLttostr((INT32)tols,0,tbuf));
            } else {
                fprintf(fp, "                   ");
                TimeStampsOK = FALSE;
            }
            if (TimeStampsOK) {
                fprintf(fp,"<td bgcolor=\"%06x\" align=\"center\"><FONT COLOR=\"%06x\">%s</td>\n",
                     GetBg(colorlatency),BLACK,utilLttostr((INT32)latency,8,tbuf));
            } else {
                fprintf(fp, "                   ");
            }
            fprintf(fp,"<td bgcolor=\"%06x\" align=\"center\"><FONT COLOR=\"%06x\">%s</td>\n",
                 GetBg(tslw_latency),BLACK,utilLttostr((INT32)tslw,8,tbuf));
        } else {
            if (tols != (REAL64) ISI_UNDEFINED_TIMESTAMP) {
                fprintf(fp,"<td bgcolor=\"%06x\"><FONT COLOR=\"%06x\">%s</td>\n",
                    GetBg(colorlatency),WHITE,sta);
                fprintf(fp, "<td bgcolor=\"%06x\"><FONT COLOR=\"%06x\">%s</TD>\n",
                    GetBg(colorlatency),WHITE,chanloc);
                fprintf(fp, "<td bgcolor=\"%06x\" align=\"center\"><FONT COLOR=\"%06x\">%4.2f</td>\n",
                    GetBg(colorlatency),WHITE,srate);
//  ADD CODE FOR NSEG COLOR
                fprintf(fp,"<td bgcolor=\"%06x\" align=\"center\"><FONT COLOR=\"%06x\">%i</td>\n",
                    GetBg(colorlatency),WHITE,nseg);
                fprintf(fp, "<td bgcolor=\"%06x\" align=\"center\" width=150><FONT COLOR=\"%06x\">%s</td>\n",
                   GetBg(colorlatency),WHITE,utilLttostr((INT32)tols,0,tbuf));
            } else {
                fprintf(fp, "                   ");
                TimeStampsOK = FALSE;
            }
            if (TimeStampsOK) {
                fprintf(fp, "<td bgcolor=\"%06x\" align=\"center\"><FONT COLOR=\"%06x\">%s</td>\n",
                    GetBg(colorlatency),WHITE,utilLttostr((INT32)latency,8,tbuf));
            } else {
                fprintf(fp, "                   ");
            }
            fprintf(fp, "<td bgcolor=\"%06x\" align=\"center\"><FONT COLOR=\"%06x\">%s</td>\n",
                    GetBg(tslw_latency),WHITE,utilLttostr((INT32)tslw,8,tbuf));
        }
    } else {
        fprintf(fp, "<td bgcolor=\"%06x\"><font color=\"%06x\">%s</TD>",
            DKGRAY,WHITE,sta);
        fprintf(fp, "<td bgcolor=\"%06x\"><font color=\"%06x\">%s</TD>",
            DKGRAY,WHITE,chanloc);
        fprintf(fp, "<td bgcolor=\"%06x\" align=\"center\"><font color=\"%06x\">%4.2f</td>\n",
            DKGRAY,WHITE,srate);
        fprintf(fp, "<td bgcolor=\"%06x\" align=\"center\"><font color=\"%06x\">%i</td>\n",
            DKGRAY,WHITE,nseg);
        fprintf(fp, "<td bgcolor=\"%06x\" align=\"center\" width=150><font color=\"%06x\">n/a</td>\n",
                 DKGRAY,WHITE);
        fprintf(fp, "<td bgcolor=\"%06x\" align=\"center\"><font color=\"%06x\">n/a</td>\n",
                 DKGRAY,WHITE);
        fprintf(fp, "<td bgcolor=\"%06x\" align=\"center\"><font color=\"%06x\">n/a</td>\n",
                 DKGRAY,WHITE);
    }
    fprintf(fp, "<td></td>\n");
    fprintf(fp, "</tr>\n");
}


static void AppendUniquePublicStation(LNKLST *slist, char *name)
{
LNKLST_NODE *crnt;
char *sname;

    crnt = listFirstNode(slist);
    while (crnt != NULL) {
        sname = (char *) crnt->payload;
        if (strcasecmp(name, sname) == 0) return;
        crnt = listNextNode(crnt);
    }

    if (!listAppend(slist, name, strlen(name)+1)) {
        perror("listAppend");
        exit(1);
    }
}

static LNKLST *CreateStationList(ISI_SOH_REPORT *soh)
{
int i;
LNKLST *slist;

    if ((slist = listCreate()) == NULL) {
        perror("listCreate");
        exit(1);
    }
    for (i = 0; i < soh->nentry; i++) AppendUniquePublicStation(slist, soh->entry[i].name.sta);
    if (!listSetArrayView(slist)) {
        perror("listSetArrayView");
        exit(1);
    }

    for (i = 0; i < slist->count; i++) util_ucase((char *) slist->array[i]);

    return slist;
}

static FILE *OpenChanPageOutput(char *isiserver, char *sta, char *htdocpath, char *webpath)
{
char *str2 = "chan.html";
char ofname[MAXPATHLEN+1];
char odirname[MAXPATHLEN+1];
FILE *ofp;

            sprintf(odirname,"%s/%s/%s", htdocpath, webpath, isiserver);
            if (!utilDirectoryExists(odirname)) {
               fprintf(stdout, "FATAL ERROR:   No output directory exists \n");
               exit(1);
            }
            sprintf(ofname, "%s/%s/%s/%s%s", htdocpath, webpath, isiserver, sta, str2);
            if ((ofp = fopen(ofname, "w")) == NULL) {
                perror("OpenChanPageOutput");
            }
            return ofp;
}

static void MakeChanPage(FILE *fp, ISI_SOH_REPORT *soh, ISI_CNF_REPORT *cnf, char *isiserver, char *webserver, char *htdocpath, char *webpath)
{
int i;
REAL64 tols;
REAL64 tslw;
REAL64 ssrate;
char ofname[MAXPATHLEN+1];
char webstuff[MAXPATHLEN+1];
webstuff[0]=0;
FILE *ofp;
static char *PrevSta, *Blank = "    ";
PrevSta = Blank;
ofname[0] = 0;
LNKLST *slist;

    slist = CreateStationList(soh);  // this is a list of upper case public stations

    sprintf(webstuff, "%s/%s", webserver,webpath);
    for (i = 0; i <= soh->nentry; i++) {
        if (strcmp(PrevSta, soh->entry[i].name.sta) != 0) {                     //  new station
            if (i > 0) {
                MakeChanPageTrailer(ofp, slist, PrevSta, isiserver, webstuff);
                PrintTimeStamp(ofp);
                fclose(ofp);
                if (i == soh->nentry) continue;
            } 
            ofp = OpenChanPageOutput(isiserver, soh->entry[i].name.sta, htdocpath, webpath); 
            MakeChanPageHeader(ofp, slist, isiserver, htdocpath, webpath, webstuff, soh->entry[i].name.sta);
            PrevSta = util_ucase(soh->entry[i].name.sta);
        }
        tols = (REAL64) ISI_UNDEFINED_TIMESTAMP;
        tslw = (REAL64) ISI_UNDEFINED_TIMESTAMP;
        if (soh->entry[i].tols.value != (REAL64) ISI_UNDEFINED_TIMESTAMP) {
            if (tols == (REAL64) ISI_UNDEFINED_TIMESTAMP || soh->entry[i].tols.value > tols) {
                tols = soh->entry[i].tols.value;
            }
        }
        if (soh->entry[i].tslw != (REAL64) ISI_UNDEFINED_TIMESTAMP) {
            if (tslw == (REAL64) ISI_UNDEFINED_TIMESTAMP || soh->entry[i].tslw < tslw) {
                tslw = soh->entry[i].tslw;
            }
        }
        ssrate = 1.0 / isiSrateToSint(&cnf->entry[i].srate);
        MakeChanPageResult(ofp, soh->entry[i].name.sta, soh->entry[i].name.chn, soh->entry[i].name.loc, ssrate, soh->entry[i].nseg, soh->entry[i].tols.value, tslw);
    }
    listDestroy(slist);

}
static void help(char *myname)
{
static char *VerboseHelp = 
"The arguments in [ square brackets ] are optional:\n"
"\n"
"isiserver=string  default: idahub.ucsd.edu \n"
"webserver=string  default: ida.ucsd.edu \n"
"webpath=string    default: Telemetry \n"
"htdocpath=string  default: /export/home/apache/htdocs/ida \n"
"stationdir=string default: Stations \n"
;
    fprintf(stderr, "%s %s\n\n", myname, VersionIdentString);
    fprintf(stderr,"usage: %s ", myname);
    fprintf(stderr,"[ isiserver=string webserver=string webpath=string htdocpath=string ] ");
    fprintf(stderr, "\n");
    fprintf(stderr, "%s\n", VerboseHelp);
    exit(1);

}

int main (int argc, char **argv)
{
int i;
ISI_PARAM par;
BOOL verbose = FALSE;
ISI_SOH_REPORT *soh;
ISI_CNF_REPORT *cnf;
char *webpath =   DEFAULT_WEBPATH;
char *htdocpath = DEFAULT_HTDOCPATH;
char *isiserver = DEFAULT_SERVER;
char *webserver = DEFAULT_WEBSERVER;
char *stationdir = DEFAULT_STATIONDIR;
char *base[MAXPATHLEN+1];
char *Tpath;
char ofile[MAXPATHLEN+1];
FILE *ofp;
char *mainpage="index.html";

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
        if (strncmp(argv[i], "webserver=", strlen("webserver=")) == 0) {
            webserver = argv[i] + strlen("webserver=");
        } else if (strncmp(argv[i], "isiserver=", strlen("isiserver=")) == 0) {
            isiserver = argv[i] + strlen("isiserver=");
        } else if (strncmp(argv[i], "webpath=", strlen("webpath=")) == 0) {
            webpath = argv[i] + strlen("webpath=");
        } else if (strncmp(argv[i], "stationdir=", strlen("stationdir=")) == 0) {
            stationdir = argv[i] + strlen("stationdir=");
        } else if (strncmp(argv[i], "htdocpath=", strlen("htdocpath=")) == 0) {
            htdocpath = argv[i] + strlen("htdocpath=");
        } else if (strcmp(argv[i], "-v") == 0) {
            verbose = TRUE;
        } else if (strcmp(argv[i], "-help") == 0) {
            help(argv[0]);
        } else {
           fprintf(stderr, "%s: unexpected argument: '%s'\n", argv[0], argv[i]);
            help(argv[0]);
        }
    }
    if (verbose) fprintf(stdout, "%s %s\n", argv[0], VersionIdentString);

    if ((soh = isiSoh(isiserver, &par)) == NULL) {
        perror("isiSoh");
        exit(1);
    }
    if ((cnf = isiCnf(isiserver, &par)) == NULL) {
        perror("isiCnf");
        exit(1);
    }

    sprintf(ofile, "%s/%s/%s/%s",htdocpath,webpath,isiserver,mainpage);

    if ((ofp = fopen(ofile, "w")) == NULL) {
        fprintf(stdout,"Can\'t open %s/%s/%s/%s - FATAL ERROR \n",htdocpath,webpath,isiserver,mainpage);
        perror("Main");
        exit(1);
    }
    UcaseStationName(soh, cnf);
    SortSoh(soh);
    SortCnf(cnf);

    PrintStatus(ofp, soh, cnf, isiserver, webserver, htdocpath, webpath, stationdir);
    fclose(ofp);
    MakeChanPage(stdout, soh, cnf, isiserver, webserver, htdocpath, webpath);

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

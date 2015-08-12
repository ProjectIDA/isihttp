#pragma ident "$Id: ReleaseNotes.c,v 1.23 2014/01/27 18:19:18 dechavez Exp $"

char *VersionIdentString = "Release 1.5.5";

/* Release notes

1.5.5  01/27/2014
       Changes to accomodate move to new (NetOps) VM server
       Removed some commented debug code.

1.5.0  12/17/2013
       Removed MySQL dependencies, added update timestamp to header

1.4.6  12/11/2007
       Added nseg to main isiwww page

1.4.5  10/03/2007
       Cleaned up help prompts

1.4.4  06/20/2007
       Fixed Safari browser incompatibilities

1.4.3  06/19/2007
       Color for link latency in channel view independent of the other columns

1.4.2  06/19/2007
       Took packet length into account when determining data latency.

1.4.1  06/11/2007
       Replaced page1,2,3 design to station/channel pages only (no more "page 1").  User
       specifies webserver, isi server and htdocs path

1.4.0  06/04/2007
       Added page 3 option (channel pages and nrts_mon style navigation between stations)

1.3.5  02/01/2007
       Fixed parchment display problem

1.3.4  01/25/2007
       Added links to FDSN station pages, used CSS style sheets for link font color

1.3.3(bis) 01/11/2007
       Revert to 1.3.3 code since 1.4.0 was prematurely committed

1.4.0  11/13/2006
       Added db option and used system flags to select which stations to ignore

1.3.3  10/19/2006
       Added server name to title, moved isiwww version down by the timestamp
       at the bottom of the page

1.3.2  10/19/2006
       Added parchment background 

1.3.1  08/16/2006
       Re-title and add refresh every 40 seconds

1.3.0  08/15/2006
       Adopt BUD style color conventions and added data latency and channel counts

1.2.3  10/19/2005 
       Show latencies of more than one hour and less than one day in orange

1.2.2  06/10/2005 
       Rename isiSetLogging() to isiStartLogging()

1.2.1  03/23/2005 
       Print UTC time in YYYY:DDD-HH:MM:SS format

1.2.0  02/23/2005 
       Truncate seconds from page 1 display

1.1.0  02/22/2005 
       Display local time correctly 

1.0.0  02/22/2005
       Initial release
*/

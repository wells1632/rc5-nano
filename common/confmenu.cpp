/*
 * Copyright distributed.net 1997-2012 - All Rights Reserved
 * For use in distributed.net projects only.
 * Any other distribution or use of this source violates copyright.
 *
 * ---------------------------------------------------------------------
 * Real programmers don't bring brown-bag lunches.  If the vending machine
 * doesn't sell it, they don't eat it.  Vending machines don't sell quiche.
 * ---------------------------------------------------------------------
*/
const char *confmenu_cpp(void) {
return "@(#)$Id: confmenu.cpp,v 1.71 2012/06/24 18:26:56 piru Exp $"; }

/* ----------------------------------------------------------------------- */

//#define TRACE
//#define PLAINTEXT_PW

#include "console.h"  // ConOutErr()
#include "projdata.h" // general project data: ids, flags, states; names, ...
#include "client.h"   // client->members, MINCLIENTOPTSTRLEN
#include "baseincs.h" // strlen() etc
#include "logstuff.h" // LogScreenRaw()
#include "selcore.h"  // GetCoreNameFromCoreType()
#include "clicdata.h" // GetContestNameFromID()
#include "util.h"     // projectmap_*(), DNETC_UNUSED_*
#include "triggers.h" // CheckExitRequestTriggerNoIO()
#include "confrwv.h"  // ConfigRead()/ConfigWrite()
#include "confopt.h"  // the option table
#include "confmenu.h" // ourselves

//#define REVEAL_DISABLED /* this is for gregh :) */

/* ----------------------------------------------------------------------- */
static const char *CONFMENU_CAPTION="distributed.net client configuration: %s\n"
"--------------------------------------------------------------------------\n";

static int __is_opt_available_for_project(unsigned int projectid, int menuoption)
{
  u32 flags = ProjectGetFlags(projectid);

  if ((flags & PROJECT_OK) == 0)
    return 0;     /* projectid not supported */

  if (menuoption == CONF_THRESHOLDT &&
      (flags & PROJECTFLAG_TIME_THRESHOLD) == 0)
    return 0;     /* project has no time thresholds */

  if (menuoption == CONF_PREFERREDBLOCKSIZE &&
      (flags & PROJECTFLAG_PREFERRED_BLOCKSIZE) == 0)
    return 0;     /* project has no preferred blocksize */

  return 1;       /* no restrictions found */
}

static int __count_projects_having_flag (u32 flag)
{
  int proj_i;
  int count = 0;
  for (proj_i = 0; proj_i < PROJECT_COUNT; ++proj_i)
  {
    if ((ProjectGetFlags(proj_i) & (flag | PROJECT_OK)) == (flag | PROJECT_OK))
      ++count;
  }
  return count;
}


#if 0
/* one column per contest, max 3 contests */
static int __enumcorenames_wide(const char **corenames,
                                int idx, void *)
{
  char scrline[80];
  unsigned int cont_i, i, colwidth, nextpos;
  int have_xxx_table[CONTEST_COUNT];
  unsigned int colcount = 0;

  /* if you get warnings, uncomment
  ** DNETC_UNUSED(have_xxx_table); */

  for (cont_i = 0; cont_i < CONTEST_COUNT; cont_i++)
  {
    have_xxx_table[cont_i] = 0;
    if (__is_opt_available_for_project(cont_i,CONF_CPUTYPE))
    {             /* HAVE_XXX_CORES defined and more than 1 core */
      have_xxx_table[cont_i] = 1;
      colcount++;
    }
  }

  colwidth = (sizeof(scrline)-2)/(colcount);

  if (idx == 0)
  {
    nextpos = 0;
    for (cont_i = 0; cont_i < CONTEST_COUNT; cont_i++)
    {
      if (have_xxx_table[cont_i])
      {
        const char *xxx = CliGetContestNameFromID(cont_i);
        if (!xxx) xxx = "";
        if ((i = strlen( xxx )) > colwidth)
          i = colwidth;
        strncpy( &scrline[nextpos], xxx, i );
        memset( &scrline[nextpos+i], ' ', colwidth-i );
        nextpos+=colwidth;
        have_xxx_table[cont_i] = 1;
      }
    }
    if (nextpos)
    {
      scrline[nextpos] = '\0';
      LogScreenRaw("%s\n",scrline);
    }
    nextpos = 0;
    for (cont_i = 0; cont_i < CONTEST_COUNT; cont_i++)
    {
      if (have_xxx_table[cont_i])
      {
        memset( &scrline[nextpos], '-', colwidth );
        if (colwidth > 10)
          scrline[nextpos+(colwidth-1)] = scrline[nextpos+(colwidth-2)] = ' ';
        nextpos+=colwidth;
      }
    }
    if (nextpos)
    {
      scrline[nextpos] = '\0';
      LogScreenRaw("%s\n",scrline);
    }
    nextpos = 0;
    for (cont_i = 0; cont_i < CONTEST_COUNT; cont_i++)
    {
      if (have_xxx_table[cont_i])
      {
        const char *xxx = " -1) Auto-select";
        if (!xxx) xxx = "";
        if ((i = strlen( xxx )) > colwidth)
          i = colwidth;
        strncpy( &scrline[nextpos], xxx, i );
        memset( &scrline[nextpos+i], ' ', colwidth-i );
        nextpos+=colwidth;
      }
    }
    if (nextpos)
    {
      scrline[nextpos] = '\0';
      LogScreenRaw("%s\n",scrline);
    }
    nextpos = 0;
  }

  nextpos = 0;
  for (cont_i = 0; cont_i < CONTEST_COUNT; cont_i++)
  {
    if (have_xxx_table[cont_i])
    {
      i = 0;
      if (corenames[cont_i]) /* have a corename at this idx */
      {
        char xxx[4+32]; char iname[8];
        strcpy(iname,"n/a");
        if (selcoreValidateCoreIndex(cont_i,idx) == idx)
          sprintf(iname, "%3d", idx);
        i = sprintf(xxx,"%s) %-29.29s", iname, corenames[cont_i] );
        if (i > colwidth)
          i = colwidth;
        strncpy( &scrline[nextpos], xxx, i );
      }
      memset( &scrline[nextpos+i], ' ', colwidth-i );
      nextpos+=colwidth;
    }
  }
  if (nextpos)
  {
    scrline[nextpos] = '\0';
    LogScreenRaw("%s\n",scrline);
  }

  return +1; /* keep going */
}
#endif

struct enumcoredata
{
  int linepos;
  unsigned int cont_i;
};


/* N rows per contest, upto 3 corenames per row */
static int __enumcorenames(unsigned int cont_i, const char *corename,
                           int idx, void *arg, Client *client)
{
  if (__is_opt_available_for_project(cont_i, CONF_CPUTYPE))
  {
    struct enumcoredata *ecd = (struct enumcoredata *)arg;
    int which = ((ecd->cont_i == cont_i)?(1):(0));

    for (; which < 2; which++)
    {
      char label[80];
      char contnamepad[32];
      unsigned int len;
      int need_pre_lf;

      if (which == 0)
        strcpy( label, "-1) Auto select" );
      else
      {
        len = 0;
        if (selcoreValidateCoreIndex(cont_i, idx, client) == idx)
          len = sprintf(label, "%2d) ", idx);
        else
          len = strlen(strcpy(label, "n/a "));
        strncpy( &label[len], corename, sizeof(label)-len );
        label[sizeof(label)-1] = '\0';
      }

      len = strlen(label);
      {
        unsigned int maxlen = len;
        if (maxlen > 72)
          len = maxlen = 72;
        else if (maxlen > 48)
          maxlen = 72;
        else if (maxlen > 24)
          maxlen = 48;
        else
          maxlen = 24;
        for (;len < maxlen; len++)
          label[len] = ' ';
        len = maxlen;
        label[len] = '\0';
      }

      need_pre_lf = 0;
      if (ecd->linepos != 0)
      {
        if (ecd->cont_i != cont_i ||
            (ecd->linepos + len) > 72)
        {
          need_pre_lf = 1;
          ecd->linepos = 0;
        }
      }

      contnamepad[0] = '\0';
      if (ecd->linepos == 0)
      {
        if (ecd->cont_i != cont_i)
          sprintf(contnamepad, "%-6.6s:", CliGetContestNameFromID(cont_i));
        else
          strcpy(contnamepad,"       ");
      }

      LogScreenRaw( "%s%s%s", ((need_pre_lf)?("\n"):("")),
                                contnamepad, label );
      ecd->linepos += len;
      ecd->cont_i = cont_i;
    } /* for for (; which < 2; which++) */
  }
  return +1;
}


/* ----------------------------------------------------------------------- */

static void __strip_project_from_alist(char *alist, unsigned int cont_i)
{
  const char *projname = CliGetContestNameFromID(cont_i);
  if (projname)
  {
    unsigned int namelen = 0;
    char *nextpos = alist;
    char *startpos = alist;
    char pbuf[16]; pbuf[0] = 0;

    while (*nextpos && (*nextpos==',' || *nextpos== ';' || isspace(*nextpos)))
      nextpos++;

    while (*nextpos)
    {
      alist = nextpos;
      while (*nextpos && *nextpos!=',' && *nextpos!= ';')
        nextpos++;
      if (*nextpos==',' || *nextpos== ';')
      {
        nextpos++;
        while (*nextpos && isspace(*nextpos))
          nextpos++;
      }
      if (pbuf[0] == 0)
      {
        namelen = 0;
        while (projname[namelen])
        {
          pbuf[namelen] = (char)tolower(projname[namelen]);
          namelen++;
        }
        pbuf[namelen] = 0;
        if (namelen == 0)
          break;
      }
      //printf("alist = '%s', nextpos = '%s'\n", alist, nextpos );
      if (((unsigned int)(nextpos - alist)) >= namelen)
      {
        unsigned int pos = 0;
        while (pos < namelen && pbuf[pos] == (char)tolower(*alist))
        {
          alist++;
          pos++;
        }
        if (pos == namelen &&
           (!*alist || *alist == ':' || *alist == '=' || *alist == ',' ||
            *alist == ';' || isspace(*alist)))
        {
          memmove( alist-namelen, nextpos, strlen(nextpos)+1 );
          alist = startpos;
          pos = strlen(alist);
          while (pos > 0)
          {
            pos--;
            if (alist[pos]!=':' && alist[pos]!='=' && alist[pos]!=',' &&
                alist[pos]!=';' && !isspace(alist[pos]))
              break;
            alist[pos] = '\0';
          }
          break;
        }
      }
    }
  }
  return;
}

static void __strip_inappropriates_from_alist( char *alist, int menuoption )
{
  unsigned int cont_i;
  for (cont_i = 0; cont_i < CONTEST_COUNT; cont_i++)
  {
    if (!__is_opt_available_for_project(cont_i,menuoption))
    {
      __strip_project_from_alist( alist, cont_i  );
    }
  }
  return;
}

/* ----------------------------------------------------------------------- */

static int __configure( Client *client ) /* returns >0==success, <0==cancelled */
{
  struct __userpass {
    char username[MINCLIENTOPTSTRLEN*2];
    char password[MINCLIENTOPTSTRLEN*2];
  } userpass;
  unsigned int cont_i;
  char loadorder[MINCLIENTOPTSTRLEN];
  int inthreshold[CONTEST_COUNT];
  #if !defined(NO_OUTBUFFER_THRESHOLDS)
  int outthreshold[CONTEST_COUNT];
  #endif
  int preferred_blocksize[CONTEST_COUNT];
  int pauseifbattery;

  // ---- Set all stuff that doesn't change during config ----
  // note that some options rely on others, so watch the init order

  /* ------------------- CONF_MENU_MISC ------------------ */

  conf_options[CONF_ID].thevariable=&(client->id[0]);
  conf_options[CONF_COUNT].thevariable=&(client->blockcount);
  conf_options[CONF_HOURS].thevariable=&(client->minutes);
  conf_options[CONF_PAUSEFILE].thevariable=&(client->pausefile[0]);
  conf_options[CONF_EXITFILE].thevariable=&(client->exitflagfile[0]);
  conf_options[CONF_RESTARTONINICHANGE].thevariable=&(client->restartoninichange);
  conf_options[CONF_PAUSEPLIST].thevariable=&(client->pauseplist[0]);
  conf_options[CONF_PAUSEIFCPUTEMPHIGH].thevariable=&(client->watchcputempthresh);
  conf_options[CONF_CPUTEMPTHRESHOLDS].thevariable=&(client->cputempthresh[0]);
  pauseifbattery = !client->nopauseifnomainspower;
  conf_options[CONF_PAUSEIFBATTERY].thevariable=&(pauseifbattery);
  conf_options[CONF_QUIETMODE].thevariable=&(client->quietmode);
  conf_options[CONF_CRUNCHMETER].thevariable=&(client->crunchmeter);
  conf_options[CONF_QUIETMODE].thevariable=&(client->quietmode);
  conf_options[CONF_COMPLETIONSOUNDON].thevariable=NULL; /* not available yet */

  /* ------------------- CONF_MENU_BUFF ------------------ */

  conf_options[CONF_NODISK].thevariable=&(client->nodiskbuffers);
  conf_options[CONF_LOADORDER].thevariable =
       strcpy(loadorder, projectmap_expand( client->project_order_map, client->project_state ) );
  conf_options[CONF_INBUFFERBASENAME].thevariable=&(client->in_buffer_basename[0]);
  conf_options[CONF_OUTBUFFERBASENAME].thevariable=&(client->out_buffer_basename[0]);
  conf_options[CONF_CHECKPOINT].thevariable=&(client->checkpoint_file[0]);
  conf_options[CONF_OFFLINEMODE].thevariable=&(client->offlinemode);
  conf_options[CONF_REMOTEUPDATEDISABLED].thevariable=&(client->noupdatefromfile);
  conf_options[CONF_REMOTEUPDATEDIR].thevariable=&(client->remote_update_dir[0]);
  conf_options[CONF_FREQUENT].thevariable=&(client->connectoften);
  conf_options[CONF_FREQUENT_FREQUENCY].thevariable=&(client->max_buffupd_interval);
  conf_options[CONF_FREQUENT_RETRY_FREQUENCY].thevariable=&(client->max_buffupd_retry_interval);
  for (cont_i = 0; cont_i < CONTEST_COUNT; cont_i++)
  {
    preferred_blocksize[cont_i] = client->preferred_blocksize[cont_i];
    if (preferred_blocksize[cont_i] < 1)
      preferred_blocksize[cont_i] = -1; /* (auto) */
    else if (preferred_blocksize[cont_i] < PREFERREDBLOCKSIZE_MIN)
      preferred_blocksize[cont_i] = PREFERREDBLOCKSIZE_MIN;
    else if (preferred_blocksize[cont_i] > PREFERREDBLOCKSIZE_MAX)
      preferred_blocksize[cont_i] = PREFERREDBLOCKSIZE_DEFAULT; /*yes, default*/
    inthreshold[cont_i] = client->inthreshold[cont_i];
    if (inthreshold[cont_i] < 1)
      inthreshold[cont_i] = 0;
    #if !defined(NO_OUTBUFFER_THRESHOLDS)
    outthreshold[cont_i] = client->outthreshold[cont_i];
    #endif
  }
  conf_options[CONF_PREFERREDBLOCKSIZE].thevariable=&(preferred_blocksize[0]);
  conf_options[CONF_THRESHOLDI].thevariable=&(inthreshold[0]);
  conf_options[CONF_THRESHOLDT].thevariable=&(client->timethreshold[0]);

  /* ------------------- CONF_MENU_LOG  ------------------ */

  static const char *logtypes[] = {"none","no limit","restart","fifo","rotate"};
  char logkblimit[sizeof(client->logfilelimit)], logrotlimit[sizeof(client->logfilelimit)];
  int logtype = LOGFILETYPE_NOLIMIT;
  logkblimit[0] = logrotlimit[0] = '\0';

  if ( strcmp( client->logfiletype, "rotate" ) == 0)
  {
    logtype = LOGFILETYPE_ROTATE;
    strcpy( logrotlimit, (client->logfilelimit) );
  }
  else
  {
    strcpy( logkblimit, client->logfilelimit );
    if ( client->logname[0] == '\0' || strcmp( client->logfiletype, "none" ) == 0 )
      logtype = LOGFILETYPE_NONE;
    else if (strcmp( client->logfiletype, "restart" ) == 0)
      logtype = LOGFILETYPE_RESTART;
    else if (strcmp( client->logfiletype, "fifo" ) == 0)
      logtype = LOGFILETYPE_FIFO;
  }

  conf_options[CONF_LOGTYPE].thevariable=&logtype;
  conf_options[CONF_LOGTYPE].choicelist=&logtypes[0];
  conf_options[CONF_LOGTYPE].choicemax=(int)((sizeof(logtypes)/sizeof(logtypes[0]))-1);
  conf_options[CONF_LOGNAME].thevariable=&(client->logname[0]);
  conf_options[CONF_LOGLIMIT].thevariable=&logkblimit[0];
  conf_options[CONF_LOGROTATETIME].thevariable=&(client->logrotateUTC);
  conf_options[CONF_MESSAGELEN].thevariable=&(client->messagelen);
  conf_options[CONF_SMTPSRVR].thevariable=&(client->smtpsrvr[0]);
  conf_options[CONF_SMTPFROM].thevariable=&(client->smtpfrom[0]);
  conf_options[CONF_SMTPDEST].thevariable=&(client->smtpdest[0]);
  conf_options[CONF_SMTPFROM].defaultsetting=(char *)conf_options[CONF_ID].thevariable;
  conf_options[CONF_SMTPDEST].defaultsetting=(char *)conf_options[CONF_ID].thevariable;

  /* ------------------- CONF_MENU_NET  ------------------ */

  conf_options[CONF_NETTIMEOUT].thevariable=&(client->nettimeout);
  conf_options[CONF_AUTOFINDKS].thevariable=&(client->autofindkeyserver);
  conf_options[CONF_KEYSERVNAME].thevariable=&(client->keyproxy[0]);
  conf_options[CONF_KEYSERVPORT].thevariable=&(client->keyport);
  conf_options[CONF_NOFALLBACK].thevariable=&(client->nofallback);

  #define UUEHTTPMODE_UUE      1
  #define UUEHTTPMODE_HTTP     2
  #define UUEHTTPMODE_UUEHTTP  3
  #define UUEHTTPMODE_SOCKS4   4
  #define UUEHTTPMODE_SOCKS5   5
  static const char *fwall_types[] = { "none/transparent/mapped",
                                       "HTTP", "SOCKS4", "SOCKS5" };
  #define FWALL_TYPE_NONE      0
  #define FWALL_TYPE_HTTP      1
  #define FWALL_TYPE_SOCKS4    2
  #define FWALL_TYPE_SOCKS5    3
  int fwall_type = FWALL_TYPE_NONE;
  int use_http_regardless = (( client->uuehttpmode == UUEHTTPMODE_HTTP
                            || client->uuehttpmode == UUEHTTPMODE_UUEHTTP)
                            && client->httpproxy[0] == '\0');
  int use_uue_regardless =  (  client->uuehttpmode == UUEHTTPMODE_UUE
                            || client->uuehttpmode == UUEHTTPMODE_UUEHTTP);
  if (client->httpproxy[0])
  {
    if (client->uuehttpmode == UUEHTTPMODE_SOCKS4)
      fwall_type = FWALL_TYPE_SOCKS4;
    else if (client->uuehttpmode == UUEHTTPMODE_SOCKS5)
      fwall_type = FWALL_TYPE_SOCKS5;
    else if (client->uuehttpmode==UUEHTTPMODE_HTTP ||
             client->uuehttpmode==UUEHTTPMODE_UUEHTTP)
      fwall_type = FWALL_TYPE_HTTP;
  }
  conf_options[CONF_FORCEHTTP].thevariable=&use_http_regardless;
  conf_options[CONF_FORCEUUE].thevariable=&use_uue_regardless;
  conf_options[CONF_FWALLTYPE].thevariable=&fwall_type;
  conf_options[CONF_FWALLTYPE].choicelist=&fwall_types[0];
  conf_options[CONF_FWALLTYPE].choicemax=(int)((sizeof(fwall_types)/sizeof(fwall_types[0]))-1);

  conf_options[CONF_FWALLHOSTNAME].thevariable=&(client->httpproxy[0]);
  userpass.username[0] = userpass.password[0] = 0;

  if (client->httpid[0])
  {
    strcpy( userpass.username, client->httpid );
    char *p = strchr( userpass.username,':');
    if (p)
    {
      *p++ = 0;
      strcpy( userpass.password, p );
    }
  }
  conf_options[CONF_FWALLUSERNAME].thevariable = (&userpass.username[0]);
  conf_options[CONF_FWALLPASSWORD].thevariable = (&userpass.password[0]);

  conf_options[CONF_LURKMODE].thevariable=
  conf_options[CONF_CONNIFACEMASK].thevariable=
  conf_options[CONF_DIALWHENNEEDED].thevariable=
  conf_options[CONF_CONNPROFILE].thevariable=
  conf_options[CONF_CONNSTARTCMD].thevariable=
  conf_options[CONF_CONNSTOPCMD].thevariable=NULL;

  #if defined(LURK)
  int dupcap = LurkGetCapabilityFlags();
  if ((dupcap & (CONNECT_LURK|CONNECT_LURKONLY))!=0)
  {
    conf_options[CONF_LURKMODE].thevariable=&(client->lurk_conf.lurkmode);
  }
  if ((dupcap & CONNECT_IFACEMASK)!=0)
  {
    conf_options[CONF_CONNIFACEMASK].thevariable=&(client->lurk_conf.connifacemask[0]);
  }
  if ((dupcap & CONNECT_DOD)!=0)
  {
    conf_options[CONF_DIALWHENNEEDED].thevariable=&(client->lurk_conf.dialwhenneeded);
    if ((dupcap & CONNECT_DODBYSCRIPT)!=0)
    {
      conf_options[CONF_CONNSTARTCMD].thevariable=&(client->lurk_conf.connstartcmd[0]);
      conf_options[CONF_CONNSTOPCMD].thevariable=&(client->lurk_conf.connstopcmd[0]);
    }
    if ((dupcap & CONNECT_DODBYPROFILE)!=0)
    {
      const char **connectnames = LurkGetConnectionProfileList();
      conf_options[CONF_CONNPROFILE].thevariable=&(client->lurk_conf.connprofile[0]);
      conf_options[CONF_CONNPROFILE].choicemin =
      conf_options[CONF_CONNPROFILE].choicemax = 0;
      if (connectnames)
      {
        unsigned int maxconn = 0;
        while (connectnames[maxconn])
          maxconn++;
        if (maxconn > 1) /* the first option is "", ie default */
        {
          connectnames[0] = "<Use Control Panel Setting>";
          conf_options[CONF_CONNPROFILE].choicemax = (int)(maxconn-1);
          conf_options[CONF_CONNPROFILE].choicelist = connectnames;
        }
      }
    }
  }
  #endif // if(LURK)

  /* ------------------- CONF_MENU_PERF ------------------ */

  conf_options[CONF_CPUTYPE].thevariable=NULL; /* assume not avail */
  for (cont_i = 0; cont_i < CONTEST_COUNT; cont_i++)
  {
    if (__is_opt_available_for_project(cont_i, CONF_CPUTYPE))
    {
      client->coretypes[cont_i] = selcoreValidateCoreIndex(cont_i, client->coretypes[cont_i], client);
      conf_options[CONF_CPUTYPE].thevariable = &(client->coretypes[0]);
    }
  }
  conf_options[CONF_NICENESS].thevariable = &(client->priority);
  conf_options[CONF_NUMCPU].thevariable = &(client->numcpu);
  conf_options[CONF_DEVICENUM].thevariable = &(client->devicenum);

  /* --------------------------------------------------------- */

  int returnvalue = 0;
  int editthis = -1; /* in a menu */
  int whichmenu = CONF_MENU_MAIN; /* main menu */
  const char *menuname = "";
  while (returnvalue == 0)
  {
    // there are two ways to deal with the keyport validation "problematik".
    // (Port redirection/mapping per datapipe or whatever is dealt with
    //  by the client as if the target host were a personal proxy,
    //  so they are NOT an issue from the client's perspective.)
    //
    // - either we totally uncouple firewall and keyserver parameters
    //   and don't do any validation of the keyport based on the
    //   firewall method,
    // - or we bind them tightly under the assumption that anyone using
    //   behind a firewall is not going to be connecting to a personal
    //   proxy outside the firewall, ie keyproxy is _always_ a dnet host.
    //
    // I opted for the former:
    //   a) The network layer will use a default port # if the port # is
    //      zero.  So.... why not leave it at zero?
    //   b) Validation should not be a config issue. Anything can be
    //      modified in the ini itself and subsystems do their own
    //      validation anyway. If users want to play, let them.
    //   c) If implementing a forced d.net host is preferred, it should
    //      not be done here. Network::Open is better suited for that.
    //                                                            - cyp

    /* --------------- drop/pickup menu options ---------------- */

    if (whichmenu == CONF_MENU_MISC)
    {
      conf_options[CONF_CPUTEMPTHRESHOLDS].disabledtext=
                  ((client->watchcputempthresh)?(NULL):("n/a"));
      #if (CLIENT_OS != OS_MACOSX) && (CLIENT_OS != OS_DEC_UNIX) && (CLIENT_CPU != CPU_ATI_STREAM) && (CLIENT_OS != OS_MORPHOS)
      conf_options[CONF_PAUSEIFCPUTEMPHIGH].disabledtext=
      conf_options[CONF_CPUTEMPTHRESHOLDS].disabledtext=
                  "n/a [only supported on MacOS/PPC]";
      #endif
    }
    else if (whichmenu == CONF_MENU_BUFF)
    {
      const char *na = "n/a [no net & no remote dir]";
      int noremotedir = 0;

      conf_options[CONF_INBUFFERBASENAME].disabledtext=
      conf_options[CONF_OUTBUFFERBASENAME].disabledtext=
      conf_options[CONF_CHECKPOINT].disabledtext=
                  ((!client->nodiskbuffers)?(NULL):
                  ("n/a [disk buffers are disabled]"));

      noremotedir = (client->noupdatefromfile ||
                     client->remote_update_dir[0]=='\0');

      conf_options[CONF_MENU_NET_PLACEHOLDER].disabledtext =
                  (client->offlinemode ? " ==> n/a [no net & no remote dir]" : NULL );
      conf_options[CONF_REMOTEUPDATEDIR].disabledtext =
                  (client->noupdatefromfile ? na : NULL );
      conf_options[CONF_FREQUENT].disabledtext=
                  (client->offlinemode && noremotedir ? na : NULL );
      conf_options[CONF_FREQUENT_FREQUENCY].disabledtext=
                  ((client->offlinemode && noremotedir) ? na : NULL );
      conf_options[CONF_FREQUENT_RETRY_FREQUENCY].disabledtext=
                  ((client->offlinemode && noremotedir) ? na : NULL );
      conf_options[CONF_PREFERREDBLOCKSIZE].disabledtext=
                  (client->offlinemode && noremotedir ? na : NULL );
      conf_options[CONF_THRESHOLDI].disabledtext=
                  (client->offlinemode && noremotedir ? na : NULL );
      conf_options[CONF_THRESHOLDT].disabledtext=
                  (client->offlinemode && noremotedir ? na : NULL );

      if (client->max_buffupd_interval == 0)
        conf_options[CONF_FREQUENT_RETRY_FREQUENCY].disabledtext=
                    "n/a [buffer check interval is default]";
      if (!client->connectoften)
        conf_options[CONF_FREQUENT_RETRY_FREQUENCY].disabledtext=
                    "n/a [need additional buffer level checking]";
      if (__count_projects_having_flag(PROJECTFLAG_PREFERRED_BLOCKSIZE) == 0)
        conf_options[CONF_PREFERREDBLOCKSIZE].disabledtext=
                     "n/a [not needed by a supported project]";
      if (__count_projects_having_flag(PROJECTFLAG_TIME_THRESHOLD) == 0)
        conf_options[CONF_THRESHOLDT].disabledtext=
                     "n/a [not needed by a supported project]";
    }
    else if (whichmenu == CONF_MENU_LOG)
    {
      conf_options[CONF_LOGLIMIT].thevariable=(&logkblimit[0]);
      if (logtype == LOGFILETYPE_ROTATE)
        conf_options[CONF_LOGLIMIT].thevariable=(&logrotlimit[0]);
      conf_options[CONF_LOGNAME].disabledtext=
                  ((logtype != LOGFILETYPE_NONE) ? (NULL) :
                  ("n/a [file log disabled]"));
      conf_options[CONF_LOGLIMIT].disabledtext=
                  ((logtype != LOGFILETYPE_NONE &&
                    logtype != LOGFILETYPE_NOLIMIT) ? (NULL) :
                  ("n/a [inappropriate for log type]"));
      conf_options[CONF_LOGROTATETIME].disabledtext=
                  ((logtype == LOGFILETYPE_ROTATE) ? (NULL) :
                  ("n/a [inappropriate for log type]"));
      conf_options[CONF_SMTPSRVR].disabledtext=
      conf_options[CONF_SMTPDEST].disabledtext=
      conf_options[CONF_SMTPFROM].disabledtext=
                  ((client->messagelen > 0)?(NULL):
                  ("n/a [mail log disabled]"));
    }
    else if (whichmenu == CONF_MENU_NET)
    {
      unsigned int x;
      for (x=0; x < CONF_OPTION_COUNT; x++)
      {
        if (conf_options[x].optionscreen == CONF_MENU_NET)
          conf_options[x].disabledtext= NULL;
      }
      if (fwall_type == FWALL_TYPE_NONE)
      {
        conf_options[CONF_FWALLHOSTNAME].disabledtext=
        conf_options[CONF_FWALLUSERNAME].disabledtext=
        conf_options[CONF_FWALLPASSWORD].disabledtext=
              "n/a [firewall support disabled]";
      }
      else
      {
        conf_options[CONF_FORCEHTTP].disabledtext= "n/a";
        if ( fwall_type != FWALL_TYPE_HTTP )
        {
          conf_options[CONF_FORCEUUE].disabledtext=
                "n/a [not available for this proxy method]";
        }
        if (client->httpproxy[0] == 0)
        {
          conf_options[CONF_FWALLUSERNAME].disabledtext=
          conf_options[CONF_FWALLPASSWORD].disabledtext=
                "n/a [firewall hostname missing]";
        }
        else if (fwall_type!=FWALL_TYPE_HTTP && fwall_type!=FWALL_TYPE_SOCKS5)
        {
          conf_options[CONF_FWALLPASSWORD].disabledtext=
                "n/a [proxy method does not support passwords]";
        }
      }

      if (client->autofindkeyserver)
      {
        conf_options[CONF_NOFALLBACK].disabledtext= "n/a"; //can't fallback to self
        conf_options[CONF_KEYSERVNAME].disabledtext = "n/a [autoselected]";
      }
      #ifdef LURK
      {
        conf_options[CONF_CONNIFACEMASK].disabledtext=
                                         "n/a [Requires Lurk|lurkony or DOD]";
        if ((client->lurk_conf.lurkmode)==CONNECT_LURK || (client->lurk_conf.lurkmode)==CONNECT_LURKONLY)
          conf_options[CONF_CONNIFACEMASK].disabledtext= NULL;
        #if (CLIENT_OS == OS_WIN16) || (CLIENT_OS == OS_WIN32)
        else //win16 and win32 dialwhenneeded depends on lurk being available
          conf_options[CONF_DIALWHENNEEDED].disabledtext=
                             "n/a [Dialup detection is off]";
        #endif
        if (client->lurk_conf.dialwhenneeded &&
            conf_options[CONF_DIALWHENNEEDED].thevariable &&
            conf_options[CONF_DIALWHENNEEDED].disabledtext==NULL)
          conf_options[CONF_CONNIFACEMASK].disabledtext= NULL;
        else
        {
          conf_options[CONF_CONNPROFILE].disabledtext=
          conf_options[CONF_CONNSTARTCMD].disabledtext=
          conf_options[CONF_CONNSTOPCMD].disabledtext=
          "n/a [Dialup detection is off]";
        }
      }
      #endif
    }
    #if defined(SINGLE_CRUNCHER_ONLY) // prevent "empty if statement" warnings
    else if (whichmenu == CONF_MENU_PERF)
    {
      #if defined(SINGLE_CRUNCHER_ONLY)
      conf_options[CONF_NUMCPU].disabledtext = "n/a [SINGLE_CRUNCHER_ONLY]";
      #endif
    }
    #endif

    /* -------------------- display menu -------------------------- */

    if (editthis < 0) /* menu */
    {
      int optionlist[18]; /*18==maxperpage*/
      unsigned int optioncount = 0;
      int menuoption;

      if (whichmenu == CONF_MENU_MAIN) /* top level */
      {
        // we handle the main menu separately because of the ID prompt
        // and because we want to enforce a definitive selection.
        // Other than that, its a simplified version of a non-main menu.

        int id_menu = (int)conf_options[CONF_ID].optionscreen;
        const char *id_menuname = NULL;
        menuname = "mainmenu";
        optioncount = 0;

        for (menuoption=0; menuoption < CONF_OPTION_COUNT; menuoption++)
        {
          if (conf_options[menuoption].disabledtext != NULL)
          {
            /* ignore it */
          }
          else if (conf_options[menuoption].type==CONF_TYPE_MENU &&
              ((int)conf_options[menuoption].optionscreen) == whichmenu)
          {
            optionlist[optioncount++] = menuoption;
            if (id_menu == conf_options[menuoption].choicemin)
              id_menuname = conf_options[menuoption].description;
            if (optioncount == 8) /* max 8 submenus on main menu */
              break;
          }
        }

        if (optioncount == 0)
          returnvalue = -1;

        while (whichmenu == CONF_MENU_MAIN && returnvalue == 0)
        {
          char chbuf[6];
          ConClear();
          LogScreenRaw(CONFMENU_CAPTION, "");

          for (menuoption=0; menuoption < ((int)(optioncount)); menuoption++)
            LogScreenRaw(" %u) %s\n", menuoption+1,
                       conf_options[optionlist[menuoption]].description);
          LogScreenRaw("\n 9) Discard settings and exit"
                       "\n 0) Save settings and exit\n\n");
          if (id_menuname && client->id[0] == '\0')
            LogScreenRaw("Note: You have not yet provided a distributed.net ID.\n"
                         "      Please go to the '%s' and set it.\n", id_menuname);
          else if (id_menuname && !utilIsUserIDValid(client->id))
            LogScreenRaw("Note: The distributed.net ID you provided is invalid.\n"
                         "      Please go to the '%s' and correct it.\n", id_menuname);
          LogScreenRaw("\nChoice --> " );
          ConInStr(chbuf, 2, 0);
          menuoption = ((strlen(chbuf)==1 && isdigit(chbuf[0]))?(atoi(chbuf)):(-1));
          if (CheckExitRequestTriggerNoIO() || menuoption==9)
          {
            whichmenu = -1;
            returnvalue = -1; //Breaks and tells it NOT to save
          }
          else if (menuoption == 0)
          {
            whichmenu = -1;
            returnvalue = 1; //Breaks and tells it to save
          }
          else if (menuoption>0 && menuoption<=((int)(optioncount)))
          {
            menuoption = optionlist[menuoption-1];
            whichmenu = conf_options[menuoption].choicemin;
            menuname = conf_options[menuoption].description;
          }
          ConClear();
        }
      }
      else /* non-main menu */
      {
        editthis = -1;
        while (editthis == -1)
        {
          int parentmenu = CONF_MENU_MAIN;
          const char *parentmenuname = "main menu";
          optioncount = 0;

          for (menuoption=0; menuoption < CONF_OPTION_COUNT; menuoption++)
          {
            if (conf_options[menuoption].type==CONF_TYPE_MENU &&
                ((int)conf_options[menuoption].choicemin) == whichmenu)
            {                     /* we are in the sub-menu of this menu */
              unsigned parpar;
              parentmenu = conf_options[menuoption].optionscreen;
              parentmenuname = "main menu"; //conf_options[menuoption].description;
              for (parpar=0; parpar < CONF_OPTION_COUNT; parpar++)
              {
                if (conf_options[parpar].type==CONF_TYPE_MENU &&
                  ((int)conf_options[parpar].choicemin) == parentmenu)
                {
                  parentmenuname = conf_options[parpar].description;
                  break;
                }
              }
            }
            else if (conf_options[menuoption].optionscreen == whichmenu &&
                   optioncount<((sizeof(optionlist)/sizeof(optionlist[0])-1)))
            {
              char parm[128];
              const char *descr = NULL;

              if (conf_options[menuoption].disabledtext != NULL)
              {
                #ifdef REVEAL_DISABLED /* this is only for greg! :) */
                descr = (const char *)conf_options[menuoption].disabledtext;
                #endif /* othewise ignore it */
              }
              else if (conf_options[menuoption].type==CONF_TYPE_MENU)
              {
                descr = "";
              }
              else if (conf_options[menuoption].thevariable == NULL)
              {
                #ifdef REVEAL_DISABLED /* this is only for greg! :) */
                descr = "n/a [not available on this platform]";
                #endif /* othewise ignore it */
              }
              else if (conf_options[menuoption].type==CONF_TYPE_IARRAY)
              {
                int *vectb = NULL;
                #if !defined(NO_OUTBUFFER_THRESHOLDS)
                if ( menuoption == CONF_THRESHOLDI )  // don't have a
                  vectb = &(outthreshold[0]); // THRESHOLDO any more
                #endif
                utilGatherOptionArraysToList( parm, sizeof(parm),
                    (int *)conf_options[menuoption].thevariable, vectb );
                __strip_inappropriates_from_alist( parm, menuoption );
                descr = parm;
              }
              else if (conf_options[menuoption].type==CONF_TYPE_ASCIIZ
#ifdef PLAINTEXT_PW
                || conf_options[menuoption].type==CONF_TYPE_PASSWORD
#endif
              )
              {
                descr = (const char *)conf_options[menuoption].thevariable;
                if (((conf_options[menuoption].index==CONF_INBUFFERBASENAME)
                  || (conf_options[menuoption].index==CONF_OUTBUFFERBASENAME)) && (!*descr))
                  descr = (const char *)conf_options[menuoption].defaultsetting;
              }
              else if (conf_options[menuoption].type==CONF_TYPE_PASSWORD)
              {
                int i = strlen((char *)conf_options[menuoption].thevariable);
                memset(parm, '*', i);
                parm[i] = 0;
                descr = parm;
              }
              else if (conf_options[menuoption].type==CONF_TYPE_TIMESTR)
              {
                int t = *((int *)conf_options[menuoption].thevariable);
                sprintf(parm, "%d:%02u", (t/60),
                               (unsigned int)(((t<0)?(-t):(t))%60) );
                descr = parm;
                if (*conf_options[menuoption].defaultsetting)
                {
                  t = strlen(parm);
                  if (((unsigned int)t) <
                      strlen(conf_options[menuoption].defaultsetting) &&
                      conf_options[menuoption].defaultsetting[t]==' ' &&
                      memcmp(parm,conf_options[menuoption].defaultsetting,t)==0)
                  {
                    descr = conf_options[menuoption].defaultsetting;
                  }
                }
              }
              else if (conf_options[menuoption].type==CONF_TYPE_INT)
              {
                int thevar = *((int *)conf_options[menuoption].thevariable);
                if ((conf_options[menuoption].choicelist != NULL) &&
                     (thevar >= conf_options[menuoption].choicemin) &&
                     (thevar <= conf_options[menuoption].choicemax) )
                {
                  descr = (const char *)conf_options[menuoption].choicelist[thevar];
                }
                else if (thevar == atoi(conf_options[menuoption].defaultsetting))
                {
                  descr = (const char *)conf_options[menuoption].defaultsetting;
                }
                else
                {
                  sprintf(parm, "%d", thevar );
                  descr = parm;
                }
              }
              else if (conf_options[menuoption].type == CONF_TYPE_BOOL)
              {
                descr = "no";
                if (*((int *)conf_options[menuoption].thevariable))
                  descr = "yes";
              }

              if (descr)
              {
                char parm2[128];
                unsigned int optlen;
                optionlist[optioncount++] = menuoption;
                optlen = sprintf(parm2, "%2u) %s%s", optioncount,
                     conf_options[menuoption].description,
                     (conf_options[menuoption].type == CONF_TYPE_MENU ? "" :
                                                       " ==> " ));
                if (descr)
                {
                  strncpy( &parm2[optlen], descr, (80-optlen) );
                  parm2[79]=0;
                }
                if (optioncount == 1)
                {
                  ConClear();
                  LogScreenRaw(CONFMENU_CAPTION, menuname);
                }
                LogScreenRaw( "%s\n", parm2 );
              }
            }
          }

          menuoption = 0;
          if (optioncount > 0)
          {
            char chbuf[sizeof(long)*3];
            menuoption = 0; //-1;
            LogScreenRaw("\n 0) Return to %s\n\nChoice --> ",parentmenuname);
            if (ConInStr( chbuf, sprintf(chbuf,"%d",optioncount)+1, 0 )!=0)
            {
              if (!CheckExitRequestTriggerNoIO())
              {
                menuoption = atoi( chbuf );
                if (menuoption<0 || menuoption>((int)(optioncount)))
                  menuoption = -1;
                else if (menuoption != 0)
                {
                  menuoption = optionlist[menuoption-1];
                  if ((conf_options[menuoption].disabledtext != NULL) ||
                    ((conf_options[menuoption].type != CONF_TYPE_MENU) &&
                    conf_options[menuoption].thevariable == NULL))
                  {
                    menuoption = -1;
                  }
                }
              }
            }
          }

          if (CheckExitRequestTriggerNoIO())
          {
            returnvalue = -1;
            editthis = -3;
            whichmenu = -2;
          }
          else if (menuoption == 0)
          {
            whichmenu = parentmenu;
            menuname = parentmenuname;
            editthis = ((parentmenu == CONF_MENU_MAIN)?(-2):(-1));
          }
          else if (menuoption > 0)
          {
            if (conf_options[menuoption].disabledtext != NULL)
            {
              editthis = -1;
            }
            else if (conf_options[menuoption].type == CONF_TYPE_MENU)
            {
              parentmenu = whichmenu;
              parentmenuname = menuname;
              whichmenu = conf_options[menuoption].choicemin;
              menuname = conf_options[menuoption].description;
              editthis = -2; //-1;
            }
            else if (conf_options[menuoption].thevariable != NULL)
            {
              editthis = menuoption;
            }
          }
        } /* while (editthis == -1) */
      } /* non-main menu */
    }  /* editthis < 0 */
    else
    {
      int newval_isok = 0;
      long newval_d = 0;
      char parm[128];

      /* -- display user selection in detail and get new value --- */
      while ( editthis >= 0 && !newval_isok)
      {
        const char *p;

        ConClear();
        LogScreenRaw(CONFMENU_CAPTION, menuname);
        LogScreenRaw("\n%s:\n\n", conf_options[editthis].description );

        newval_isok = 1;
        if ((p = (const char *)conf_options[editthis].comments) != NULL)
        {
          while (strlen(p) > (sizeof(parm)-1))
          {
            strncpy(parm, p, sizeof(parm));
            parm[(sizeof(parm)-1)] = 0;
            LogScreenRaw("%s", parm);
            p += (sizeof(parm)-1);
          }
          LogScreenRaw("%s\n",p);
        }


        if ( conf_options[editthis].type == CONF_TYPE_ASCIIZ ||
             conf_options[editthis].type == CONF_TYPE_INT ||
             conf_options[editthis].type == CONF_TYPE_PASSWORD ||
             conf_options[editthis].type == CONF_TYPE_IARRAY )
        {
          p = "";
          char defaultbuff[30];
          int coninstrmode = CONINSTR_BYEXAMPLE;

          if (editthis == CONF_CPUTYPE) /* ugh! */
          {
            #if 1 /* N rows per contest */
            struct enumcoredata ecd;
            ecd.linepos = 0;
            ecd.cont_i = ((unsigned int)-1);
            selcoreEnumerate( __enumcorenames, &ecd, client );
            LogScreenRaw("\n\n");
            #else /* one column per contest */
            selcoreEnumerateWide( __enumcorenames_wide, NULL );
            LogScreenRaw("\n");
            #endif
          }
          if (conf_options[editthis].choicelist !=NULL)
          {
            const char *ppp;
            long selmin = (long)(conf_options[editthis].choicemin);
            long selmax = (long)(conf_options[editthis].choicemax);
            sprintf(defaultbuff,"%ld) ", selmax );
            ppp = strstr( conf_options[editthis].comments, defaultbuff );
            if (ppp != NULL)
            {
              sprintf(defaultbuff,"%ld) ", selmin );
              ppp = strstr( conf_options[editthis].comments, defaultbuff );
            }
            if (ppp == NULL)
            {
              long listpos;
              for ( listpos = selmin; listpos <= selmax; listpos++)
                LogScreenRaw("  %2ld) %s\n", listpos,
                      conf_options[editthis].choicelist[listpos]);
            }
          }

          if (conf_options[editthis].type==CONF_TYPE_ASCIIZ
#ifdef PLAINTEXT_PW
           || conf_options[editthis].type==CONF_TYPE_PASSWORD
#endif
          )
          {
            strcpy(parm, (char *)conf_options[editthis].thevariable);
            if (((conf_options[editthis].index==CONF_INBUFFERBASENAME)
              || (conf_options[editthis].index==CONF_OUTBUFFERBASENAME)) && (!*parm))
                  strcpy(parm, (const char *)conf_options[editthis].defaultsetting);
            p = (const char *)(conf_options[editthis].defaultsetting);
          }
          else if (conf_options[editthis].type==CONF_TYPE_PASSWORD)
          {
            int i = strlen((char *)conf_options[editthis].thevariable);
            memset(parm, '*', i);
            parm[i] = 0;
            coninstrmode = CONINSTR_ASPASSWORD;
          }
          else if (conf_options[editthis].type==CONF_TYPE_IARRAY)
          {
            int *vectb = NULL;
            #if !defined(NO_OUTBUFFER_THRESHOLDS)
            if ( editthis == CONF_THRESHOLDI )  // don't have a
              vectb = &(outthreshold[0]); // THRESHOLDO any more
            #endif
            utilGatherOptionArraysToList( parm, sizeof(parm),
                    (int *)conf_options[editthis].thevariable, vectb );
            __strip_inappropriates_from_alist( parm, editthis );
            p = (const char *)(conf_options[editthis].defaultsetting);
          }
          else //if (conf_options[editthis].type==CONF_TYPE_INT)
          {
            sprintf(parm, "%d", *((int *)conf_options[editthis].thevariable) );
            sprintf(defaultbuff, "%d", atoi(conf_options[editthis].defaultsetting));
            p = defaultbuff;
          }
          LogScreenRaw("Default Setting: %s\n"
                       "Current Setting: %s\n"
                       "New Setting --> ", p, parm );

          ConInStr( parm, 64 /*sizeof(parm)*/, coninstrmode );

          if (CheckExitRequestTriggerNoIO())
          {
            editthis = -2;
            returnvalue = -1;
          }
          else if (conf_options[editthis].type==CONF_TYPE_INT)
          {
            p = parm;
            int i = 0;
            while (isspace(*p))
              p++;
            while (*p)
            {
              if ((i==0 && (*p=='-' || *p == '+')) || isdigit(*p))
                parm[i++]=*p++;
              else
              {
                newval_isok = 0;
                break;
              }
            }
            parm[i] = 0;
            if (newval_isok)
            {
              newval_d = atol(parm);
              long selmin = (long)conf_options[editthis].choicemin;
              long selmax = (long)conf_options[editthis].choicemax;
              if ((selmin != 0 || selmax != 0) &&
                (newval_d < selmin || newval_d > selmax))
              newval_isok = 0;
            }
          }
          else if (conf_options[editthis].type == CONF_TYPE_IARRAY)
          {
            newval_isok = 1;
            if (editthis == CONF_CPUTYPE)
            {
              /* merge with previous is done later - this is just to check
              ** that the numbers that the user _does_ provide are ok.
              ** (there is no min/max range adjustment done for cputype)
              */
              int iarray[CONTEST_COUNT];
              for (cont_i = 0; cont_i < CONTEST_COUNT; cont_i++)
                iarray[cont_i] = -1; /* default for validation purposes */
              utilScatterOptionListToArraysEx(parm, &iarray[0], NULL,NULL, NULL );
              for (cont_i = 0; cont_i < CONTEST_COUNT; cont_i++)
              {
                if (__is_opt_available_for_project(cont_i, editthis) &&
                   iarray[cont_i] != selcoreValidateCoreIndex(cont_i, iarray[cont_i], client))
                {
                  newval_isok = 0; /* reject it */
                  break;
                }
              }
            }
          }
          else //if (conf_options[editthis].type==CONF_TYPE_ASCIIZ)
          {
            if (parm[0]!=0)
            {
              char *wp = &parm[strlen(parm)-1];
              while (wp >= &parm[0] && isspace(*wp))
                *wp-- = 0;
              p = parm;
              while (*p && isspace(*p))
                p++;
              if (p > &parm[0])
                strcpy( parm, p );
            }
            newval_isok = 1;

            if (parm[0] != 0 && conf_options[editthis].choicemax != 0 &&
                conf_options[editthis].choicelist) /* int *and* asciiz */
            {
              newval_d = atol(parm);
              if ( ((newval_d > 0) || (parm[0] == '0')) &&
                 (newval_d <= ((long)conf_options[editthis].choicemax)) )
              {
                strncpy(parm, conf_options[editthis].choicelist[newval_d], sizeof(parm));
                parm[sizeof(parm)-1]=0;
                if (newval_d == 0 && editthis == CONF_CONNPROFILE)
                  parm[0]=0;
              }
            }

            if (editthis == CONF_ID)
            {
              if (newval_isok)
                newval_isok = utilIsUserIDValid(parm);
            }
          }
        }
        else if (conf_options[editthis].type == CONF_TYPE_TIMESTR)
        {
          int t = *((int *)conf_options[editthis].thevariable);
          sprintf(parm,"%d:%02u", (t/60),
                           (unsigned int)(((t<0)?(-t):(t))%60) );
          LogScreenRaw("Default Setting: %s\n"
                       "Current Setting: %s\n"
                       "New Setting --> ",
                       conf_options[editthis].defaultsetting, parm );

          ConInStr( parm, 10, CONINSTR_BYEXAMPLE );

          if (CheckExitRequestTriggerNoIO())
          {
            editthis = -2;
            returnvalue = -1;
          }
          else
          {
            if (parm[0]!=0)
            {
              char *wp = &parm[strlen(parm)-1];
              while (wp >= &parm[0] && isspace(*wp))
                *wp-- = 0;
              p = parm;
              while (*p && isspace(*p))
                p++;
              if (p > &parm[0])
                strcpy( parm, p );
            }
            if (parm[0]!=0)
            {
              int h=0, m=0, pos, isok = 0, dotpos=0;
              if (isdigit(parm[0]))
              {
                isok = 1;
                for (pos = 0; parm[pos] != 0; pos++)
                {
                  if (!isdigit(parm[pos]))
                  {
                    if (dotpos != 0 || (parm[pos] != ':' && parm[pos] != '.'))
                    {
                      isok = 0;
                      break;
                    }
                    dotpos = pos;
                  }
                }
                if (isok)
                {
                  if ((h = atoi( parm )) < 0)
                    isok = 0;
                  //else if (h > 23)
                  //  isok = 0;
                  else if (dotpos == 0)
                    isok = 0;
                  else if (strlen(&parm[dotpos+1]) != 2)
                    isok = 0;
                  else if (((m = atoi(&parm[dotpos+1])) > 59))
                    isok = 0;
                }
              } //if (isdigit(parm[0]))
              if (isok)
                newval_d = ((h*60)+m);
              else
                newval_isok = 0;
            } //if (parm[0]!=0)
          } //if (CheckExitRequestTriggerNoIO()) else ...
        }
        else if (conf_options[editthis].type==CONF_TYPE_BOOL)
        {
          strcpy(parm, (*((int *)conf_options[editthis].thevariable))?"yes":"no");
          LogScreenRaw("Default Setting: %s\n"
                       "Current Setting: %s\n"
                       "New Setting --> ",
                       conf_options[editthis].defaultsetting, parm );
          parm[1] = 0;
          ConInStr( parm, 2, CONINSTR_BYEXAMPLE|CONINSTR_ASBOOLEAN );
          if (CheckExitRequestTriggerNoIO())
          {
            editthis = -2;
            returnvalue = -1;
          }
          else if (parm[0]=='y' || parm[0]=='Y')
            newval_d = 1;
          else if (parm[0]=='n' || parm[0]=='N')
            newval_d = 0;
          else
            newval_isok = 0;
        }
        else
        {
          editthis = -1;
        }
        if (editthis >= 0 && !newval_isok)
        {
          ConBeep();
        }
      }

      /* --------------- have modified value, so assign -------------- */

      if (editthis >= 0 && newval_isok)
      {
        // DO NOT TOUCH ANY VARIABLE EXCEPT THE SELECTED ONE
        // (unless those variables are not menu options)
        // DO IT AFTER ALL MENU DRIVEN CONFIG IS FINISHED (see end)

        if (editthis == CONF_ID || editthis == CONF_KEYSERVNAME ||
          editthis == CONF_SMTPFROM || editthis == CONF_SMTPSRVR ||
          editthis == CONF_FWALLHOSTNAME)
        {
          char *opos, *ipos;
          ipos = opos = &parm[0];
          while ( *ipos )
          {
            if ( !isspace( *ipos ) )
              *opos++ = *ipos;
            ipos++;
          }
          *opos = '\0';
          if ( strcmp( parm, "none" ) == 0 )
            parm[0]='\0';
        }
        if (conf_options[editthis].type==CONF_TYPE_ASCIIZ ||
            conf_options[editthis].type==CONF_TYPE_PASSWORD)
        {
          strncpy( (char *)conf_options[editthis].thevariable, parm,
                   MINCLIENTOPTSTRLEN );
          ((char *)conf_options[editthis].thevariable)[MINCLIENTOPTSTRLEN-1]=0;
          if (editthis == CONF_LOADORDER)
          {
            projectmap_build(client->project_order_map, client->project_state, loadorder );
            conf_options[CONF_LOADORDER].thevariable =
              strcpy(loadorder, projectmap_expand( client->project_order_map, client->project_state ) );
          }
        }
        else if ( conf_options[editthis].type == CONF_TYPE_IARRAY)
        {
          int *vecta = NULL, *vectb = NULL;
          int iarray_a[CONTEST_COUNT], iarray_b[CONTEST_COUNT];
          iarray_a[0] = iarray_b[0] = 0;
          vecta = (int *)conf_options[editthis].thevariable;
          #if !defined(NO_OUTBUFFER_THRESHOLDS)
          if ( editthis == CONF_THRESHOLDI )  // don't have a
            vectb = &(outthreshold[0]); // THRESHOLDO any more
          #endif
          for (cont_i = 0; cont_i < CONTEST_COUNT; cont_i++)
          {
            iarray_a[cont_i] = vecta[cont_i];
            if (vectb)
             iarray_b[cont_i] = vectb[cont_i];
          }
          vecta = &iarray_a[0];
          if (vectb)
            vectb = &iarray_b[0];
          utilScatterOptionListToArraysEx(parm,vecta, vectb,NULL, NULL );
          vecta = (int *)conf_options[editthis].thevariable;
          #if !defined(NO_OUTBUFFER_THRESHOLDS)
          if ( editthis == CONF_THRESHOLDI )  // don't have a
            vectb = &(outthreshold[0]); // THRESHOLDO any more
          #endif
          if (editthis == CONF_CPUTYPE)
          {
            for (cont_i = 0; cont_i < CONTEST_COUNT; cont_i++)
            {
              vecta[cont_i] = -1; /* autosel */
              if (__is_opt_available_for_project(cont_i, editthis))
                vecta[cont_i] = selcoreValidateCoreIndex(cont_i, iarray_a[cont_i], client);
            }
          }
          else
          {
            int mmin = conf_options[editthis].choicemin;
            int mmax = conf_options[editthis].choicemax;
            int mdef = atoi(conf_options[editthis].defaultsetting);
            if (mmin || mmax)
            {
              for (cont_i = 0; cont_i < CONTEST_COUNT; cont_i++)
              {
                if (__is_opt_available_for_project(cont_i, editthis))
                {
                  int v = iarray_a[cont_i];
                  if (v == mdef)      v = mdef;
                  else if (v < mmin)  v = mmin;
                  else if (v > mmax)  v = mmax;
                  vecta[cont_i] = v;
                  if (vectb)
                  {
                    v = iarray_b[cont_i];
                    if (v == mdef)    v = mdef;
                    else if (editthis == CONF_THRESHOLDI)
                    {
                      /* outthresh must only be < inthresh and can be <=0 */
                      if (v > iarray_a[cont_i])
                        v = iarray_a[cont_i];
                      else if (v < 0)
                        v = -1;
                    }
                    else if (v < mmin) v = mmin;
                    else if (v > mmax) v = mmax;
                    vectb[cont_i] = v;
                  }
                }
              }
            }
          }
        }
        else //bool or int types
        {
          *((int *)conf_options[editthis].thevariable) = (int)newval_d;
          if ( editthis == CONF_COUNT && newval_d < 0)
            *((int *)conf_options[editthis].thevariable) = -1;
          else if (editthis == CONF_NETTIMEOUT)
            *((int *)conf_options[editthis].thevariable) = ((newval_d<0)?(-1):((newval_d<5)?(5):(newval_d)));
        }
      } /* if (editthis >= 0 && newval_isok) */
      editthis = -1; /* no longer an editable option */
    } /* not a menu */
  } /* while (returnvalue == 0) */

  if (CheckExitRequestTriggerNoIO())
  {
    LogScreenRaw("\n");
    returnvalue = -1;
  }

  /* -- massage mapped options and dependencies back into place -- */

  if (returnvalue != -1)
  {
    client->nopauseifnomainspower = !pauseifbattery;
    for (cont_i = 0; cont_i < CONTEST_COUNT; cont_i++)
    {
      if (preferred_blocksize[cont_i] < 1) /* "auto" */
        preferred_blocksize[cont_i] = 0;
      client->preferred_blocksize[cont_i] = preferred_blocksize[cont_i];
      if (inthreshold[cont_i] < 1) /* "auto" */
        inthreshold[cont_i] = 0;
      client->inthreshold[cont_i] = inthreshold[cont_i];
      #if !defined(NO_OUTBUFFER_THRESHOLDS)
      client->outthreshold[cont_i] = outthreshold[cont_i];
      if (client->outthreshold[cont_i] > inthreshold[cont_i])
        client->outthreshold[cont_i] = inthreshold[cont_i];
      #endif
    }
    if (logtype >=0 && logtype < (int)(sizeof(logtypes)/sizeof(logtypes[0])))
    {
      if (logtype == LOGFILETYPE_ROTATE)
        strcpy( client->logfilelimit, logrotlimit );
      else
      {
        if (client->logname[0] == '\0')
          logtype = LOGFILETYPE_NONE;
        strcpy( client->logfilelimit, logkblimit );
      }
      strcpy( client->logfiletype, logtypes[logtype] );
    }

    if (client->nettimeout < 0)
      client->nettimeout = -1;
    else if (client->nettimeout < 5)
      client->nettimeout = 5;

    client->uuehttpmode = 0;
    if (fwall_type == FWALL_TYPE_SOCKS4)
      client->uuehttpmode = UUEHTTPMODE_SOCKS4;
    else if (fwall_type == FWALL_TYPE_SOCKS5)
      client->uuehttpmode = UUEHTTPMODE_SOCKS5;
    else if (fwall_type == FWALL_TYPE_HTTP || use_http_regardless)
      client->uuehttpmode = (use_uue_regardless?UUEHTTPMODE_UUEHTTP:UUEHTTPMODE_HTTP);
    else if (use_uue_regardless)
      client->uuehttpmode = UUEHTTPMODE_UUE;

    TRACE_OUT((0,"precomp: u:p=\"%s:%s\"\n",userpass.username,userpass.password));
    client->httpid[0] = 0;
    if (strlen(userpass.username) || strlen(userpass.password))
    {
      if (strlen(userpass.password))
        strcat(strcat(userpass.username, ":"), userpass.password);
      strncpy( client->httpid, userpass.username, sizeof( client->httpid ));
      client->httpid[sizeof( client->httpid )-1] = 0;
    }
    TRACE_OUT((0,"postcomp: u:p=\"%s\"\n",client->httpid));
  }

  //fini

  return returnvalue; /* <0=exit+nosave, >0=exit+save */
}


/* returns <0=error, 0=success+nosave, >0=success+save */
int Configure( Client *sample_client, int nottycheck )
{
  int rc = -1; /* assume error */
  if (!nottycheck && !ConIsScreen())
  {
    ConOutErr("Screen output is redirected/not available. Please use --config\n");
  }
  else
  {
    Client *newclient = (Client *)malloc(sizeof(Client));
    if (!newclient)
    {
      ConOutErr("Unable to configure. (Insufficient memory)");
    }
    else
    {
      ResetClientData(newclient);
      strcpy(newclient->inifilename, sample_client->inifilename );
      if (ConfigRead(newclient) < 0) /* <0=fatalerror, 0=no error, >0=inimissing */
      {
        ConOutErr("Unable to configure. (Fatal error reading/parsing .ini)");
      }
      else if ( __configure(newclient) < 0 ) /* nosave */
      {
        rc = 0; /* success+nosave */
      }
      else if (ConfigWrite(newclient) < 0)
      {
        ConOutErr("Unable to save one or more settings to the .ini");
      }
      else
      {
        rc = +1; /* success+saved */
      }
      free((void *)newclient);
    }
  }
  return rc;
}

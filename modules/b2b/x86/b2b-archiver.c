/*******************************************************************************************
 *  b2b-archiver.c
 *
 *  created : 2021
 *  author  : Dietrich Beck, GSI-Darmstadt
 *  version : 02-Mar-2023
 *
 * archives set and get values to data files
 *
 * ------------------------------------------------------------------------------------------
 * License Agreement for this software:
 *
 * Copyright (C) 2013  Dietrich Beck
 * GSI Helmholtzzentrum für Schwerionenforschung GmbH
 * Planckstraße 1
 * D-64291 Darmstadt
 * Germany
 *
 * Contact: d.beck@gsi.de
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 3 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *  
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * For all questions and ideas contact: d.beck@gsi.de
 * Last update: 15-April-2019
 *********************************************************************************************/
#define B2B_ARCHIVER_VERSION 0x000500

// standard includes 
#include <unistd.h> // getopt
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include <time.h>

// dim
#include <dic.h>
#include <dis.h>

// b2b
#include <common-lib.h>                    // COMMON
#include <b2blib.h>                        // API
#include <b2b.h>                           // FW

const char* program;

#define TDIAGOBS    20000000               // observation time for diagnostic [ns]
#define DDSSTEP     0.046566129            // min frequency step of gDDS

// dim stuff
#define    DIMCHARSIZE 32                  // standard size for char services
#define    DIMMAXSIZE  1024                // max size for service names

uint32_t   no_link_32    = 0xdeadbeef;
uint64_t   no_link_64    = 0xdeadbeefce420651;
char       no_link_str[] = "NO_LINK";

setval_t   dicSetval[B2B_NSID]; 
getval_t   dicGetval[B2B_NSID];
nueMeas_t  dicNueMeasExt[B2B_NSID];
char       dicPName[B2B_NSID][DIMMAXSIZE];

uint32_t   dicSetvalId[B2B_NSID];
uint32_t   dicGetvalId[B2B_NSID];
uint32_t   dicNueMeasExtId[B2B_NSID];
uint32_t   dicPNameId[B2B_NSID];

// global variables
int        flagSetValid[B2B_NSID];          // flag: received set value
int        flagGetValid[B2B_NSID];          // flag: received get value

time_t     utc_secs[B2B_NSID];              // time of CBS in UTC
uint32_t   utc_msecs[B2B_NSID];             // time of CBS in UTC

char       filename[B2B_NSID][DIMMAXSIZE];  // file names


static void help(void) {
  fprintf(stderr, "Usage: %s [OPTION] <PREFIX> \n", program);
  fprintf(stderr, "\n");
  fprintf(stderr, "  -h                  display this help and exit\n");
  fprintf(stderr, "  -e                  display version\n");
  fprintf(stderr, "  -f<fprefix>         sets a prefix for the file names\n");
  fprintf(stderr, "  -n                  create new files, erases existing files\n");
  fprintf(stderr, "\n");
  fprintf(stderr, "Use this tool to archive data of the B2B system\n");
  fprintf(stderr, "Example1: '%s pro_sis18 -ftest\n", program);
  fprintf(stderr, "\n");
  fprintf(stderr, "Report software bugs to <d.beck@gsi.de>\n");
  fprintf(stderr, "Version %s. Licensed under the LGPL v3.\n", b2b_version_text(B2B_ARCHIVER_VERSION));
} //help


// header String for file
char * headerString()
{
  return "patternName; time_CBS_UTC; sid; mode; valid; ext_T [as]; valid; ext_h; valid; ext_cTrig; valid; inj_T; valid; inj_h; valid; inj_cTrig; valid; cPhase; valid; ext_phase; ext_phaseFract; ext_phaseErr; valid; ext_dKickMon; valid; ext_dKickProb; valid; ext_diagPhase [as]; valid; ext_diag_Match; valid; inj_phase; inj_phaseFract; inj_phaseErr; valid; inj_dKickMon; valid; inj_dKickProb; valid; inj_diagPhase; valid; inj_diagMatch; received PME; PMI; PRE; PRI; KTE; KTI; KDE; KDI; PDE; PDI; error PME; PMI; PRE; PRI; KTE; KTI; KDE; KDI; PDE; PDI; late PME; PMI; PRE; PRI; KTE; KTI; KDE; KDI; PDE; PDI; fin-CBS; prr-CBS; t0E-CBS; t0I-CBS; kte-CBS; kti-CBS; ext_nueMeas; ext_dNueMeas";
} // headerString

// receive get values
void recGetvalue(long *tag, diagval_t *address, int *size)
{
#define STRMAXLEN 2048
  uint32_t  sid;
  uint32_t  mode;
  double    cor;
  double    act;
  char      tCBS[256];;

  char      strSetval[STRMAXLEN];
  char      strGetval[STRMAXLEN];
  char      strNueval[STRMAXLEN];
  char      *new;

  int       i;

  FILE      *dataFile;                       // file for data

  sid = *tag;
  if ((sid < 0) || (sid >= B2B_NSID)) return;
  if (!flagSetValid[sid])             return;
  flagGetValid[sid] = (*size != sizeof(uint32_t));
  mode = dicSetval[sid].mode;
  //if (mode <  1) return;                                    // b2b 'off', no need to write data; but maybe it is interesting to see when facility was executed withouot b2b

  strftime(tCBS, 52, "%d-%b-%Y_%H:%M:%S", gmtime(&(utc_secs[sid])));

  // set values
  new  = strSetval;
  new += sprintf(new, "%s.%03d; %d; %d", tCBS, utc_msecs[sid], sid, mode);
  new += sprintf(new, "; %d; %lu"   , !((dicSetval[sid].flag_nok >> 1) & 0x1), dicSetval[sid].ext_T);
  new += sprintf(new, "; %d; %d"    , !((dicSetval[sid].flag_nok >> 2) & 0x1), dicSetval[sid].ext_h);
  new += sprintf(new, "; %d; %8.3f" , !((dicSetval[sid].flag_nok >> 3) & 0x1), dicSetval[sid].ext_cTrig);
  new += sprintf(new, "; %d; %lu"   , !((dicSetval[sid].flag_nok >> 4) & 0x1), dicSetval[sid].inj_T);
  new += sprintf(new, "; %d; %d"    , !((dicSetval[sid].flag_nok >> 5) & 0x1), dicSetval[sid].inj_h);
  new += sprintf(new, "; %d; %8.3f" , !((dicSetval[sid].flag_nok >> 6) & 0x1), dicSetval[sid].inj_cTrig);
  new += sprintf(new, "; %d; %8.3f" , !((dicSetval[sid].flag_nok >> 7) & 0x1), dicSetval[sid].cPhase);

  // get values
  new  = strGetval;
  new += sprintf(new, "; %d; %lu"  ,  !((dicGetval[sid].flag_nok     ) & 0x1), dicGetval[sid].ext_phase);
  new += sprintf(new, "; %7.3f"    ,  (double)dicGetval[sid].ext_phaseFract_ps / 1000.0);
  new += sprintf(new, "; %7.3f"    ,  (double)dicGetval[sid].ext_phaseErr_ps   / 1000.0);  
  new += sprintf(new, "; %d; %d"   ,  !((dicGetval[sid].flag_nok >> 1) & 0x1), dicGetval[sid].ext_dKickMon);
  new += sprintf(new, "; %d; %d"   ,  !((dicGetval[sid].flag_nok >> 2) & 0x1), dicGetval[sid].ext_dKickProb);

  cor  = 0;
  act  = b2b_fixTS(dicGetval[sid].ext_diagPhase, cor, dicSetval[sid].ext_T) - cor;
  new += sprintf(new, "; %d; %8.3f",  !((dicGetval[sid].flag_nok >> 3) & 0x1), act);

  cor  = dicSetval[sid].ext_cTrig;
  act  = b2b_fixTS(dicGetval[sid].ext_diagMatch, cor, dicSetval[sid].ext_T) - cor;
  new += sprintf(new, "; %d; %8.3f",  !((dicGetval[sid].flag_nok >> 4) & 0x1), act);
  
  new += sprintf(new, "; %d; %lu",    !((dicGetval[sid].flag_nok >> 5) & 0x1), dicGetval[sid].inj_phase);
  new += sprintf(new, "; %7.3f  ",    (double)dicGetval[sid].inj_phaseFract_ps / 1000.0);
  new += sprintf(new, "; %7.3f"  ,    (double)dicGetval[sid].inj_phaseErr_ps   / 1000.0);  
  new += sprintf(new, "; %d; %d" ,    !((dicGetval[sid].flag_nok >> 6) & 0x1), dicGetval[sid].inj_dKickMon);
  new += sprintf(new, "; %d; %d" ,    !((dicGetval[sid].flag_nok >> 7) & 0x1), dicGetval[sid].inj_dKickProb);

  cor  = 0;
  act  = b2b_fixTS(dicGetval[sid].inj_diagPhase, cor, dicSetval[sid].inj_T) - cor;
  new += sprintf(new, "; %d; %8.3f",  !((dicGetval[sid].flag_nok >> 8) & 0x1), act);

  cor = dicSetval[sid].inj_cTrig - dicSetval[sid].cPhase;
  act = b2b_fixTS(dicGetval[sid].inj_diagMatch, cor, dicSetval[sid].inj_T) - cor;
  new += sprintf(new, "; %d; %8.3f",  !((dicGetval[sid].flag_nok >> 9) & 0x1), act);

  for (i=0; i<10; i++) new += sprintf(new, "; %d", ((dicGetval[sid].flagEvtRec  >> i) & 0x1));
  for (i=0; i<10; i++) new += sprintf(new, "; %d", ((dicGetval[sid].flagEvtErr  >> i) & 0x1));
  for (i=0; i<10; i++) new += sprintf(new, "; %d", ((dicGetval[sid].flagEvtLate >> i) & 0x1));
  new += sprintf(new, "; %d; %d; %d; %d; %d; %d", dicGetval[sid].finOff, dicGetval[sid].prrOff, dicGetval[sid].preOff, dicGetval[sid].priOff, dicGetval[sid].kteOff, dicGetval[sid].ktiOff);

  // frequency values; chk: in principle we should check the timestammp of the service too?
  new = strNueval;
  if (*(uint32_t *)&(dicNueMeasExt[sid]) == no_link_32)
    new += sprintf(new, "; NOLINK; NOLINK");
  else 
    new += sprintf(new, "; %13.6f; %13.6f", dicNueMeasExt[sid].nueGet, dicNueMeasExt[sid].nueErr);

  if (!(dataFile = fopen(filename[sid], "a"))) return;
  fprintf(dataFile, "%s; %s%s%s\n", dicPName[sid], strSetval, strGetval, strNueval);
  fclose(dataFile); 
} // recGetvalue
  
// receive set values
void recSetvalue(long *tag, setval_t *address, int *size)
{
  uint32_t sid;
  uint32_t secs, msecs;

  sid = *tag;
  if ((sid < 0) || (sid >= B2B_NSID)) return;

  dic_get_timestamp(0, &secs, &msecs);
  utc_secs[sid]     = (time_t)(secs);
  utc_msecs[sid]    = msecs;
  flagSetValid[sid] = (*size != sizeof(uint32_t));
} // recSetValue
  

// add all dim services
void dicSubscribeServices(char *prefix)
{
  char name[DIMMAXSIZE];
  int  i;

  for (i=0; i<B2B_NSID; i++) {
    sprintf(name, "%s-raw_sid%02d_setval", prefix, i);
    //printf("name %s\n", name);
    dicSetvalId[i]     = dic_info_service_stamped(name, MONITORED, 0, &(dicSetval[i]), sizeof(setval_t), recSetvalue, i, &no_link_32, sizeof(uint32_t));

    sprintf(name,"%s-pname_sid%02d", prefix, i);
    //printf("name %s\n", name);
    dicPNameId[i]      = dic_info_service_stamped(name, MONITORED, 0, &(dicPName[i]), DIMMAXSIZE, 0 , 0, &no_link_str, sizeof(no_link_str));

    sprintf(name, "%s-other-rf_sid%02d_ext", prefix, i);
    /* printf("name %s\n", name); */
    dicNueMeasExtId[i] = dic_info_service_stamped(name, MONITORED, 0, &(dicNueMeasExt[i]), sizeof(nueMeas_t), 0 , 0, &no_link_32, sizeof(uint32_t));

    sleep (2);  // data is taken upon callback of set-values; wait a bit until the other services have connected to their servers

    sprintf(name, "%s-raw_sid%02d_getval", prefix, i);
    //printf("name %s\n", name);
    dicGetvalId[i]     = dic_info_service_stamped(name, MONITORED, 0, &(dicGetval[i]), sizeof(getval_t), recGetvalue, i, &no_link_32, sizeof(uint32_t));
  } // for i
} // dicSubscribeServices


int main(int argc, char** argv) {
  int opt, error = 0;
  int exitCode   = 0;
  char *tmp;

  int      getVersion;

  char     sprefix[DIMMAXSIZE];             // prefix for system like 'pro'
  char     fprefix[DIMMAXSIZE];             // prefix for file like 'test'
  int      i;
  FILE     *dataFile;                       // file for data
  char     fileMode[10];                    // mode for file


  program       = argv[0];
  getVersion    = 0;
  sprintf(fprefix, "%s", "");
  sprintf(fileMode, "a");

  while ((opt = getopt(argc, argv, "f:ehn")) != -1) {
    switch (opt) {
      case 'e':
        getVersion = 1;
        break;
      case 'h':
        help();
        return 0;
        error = 1;
        break;
      case 'n' :
        sprintf(fileMode, "w");
        break;
      case 'f' :
        tmp = strtok(optarg, " ");
        if (strlen(tmp) == 0) {
          fprintf(stderr, "Specify a proper name, not '%s'!\n", optarg);
          exit(1);
        } // if strlen
        sprintf(fprefix, "%s", tmp);
        break;
      default:
        fprintf(stderr, "%s: bad getopt result\n", program);
        return 1;
    } // switch opt
  } //  while opt

  if (error) {
    help();
    return 1;
  } // if error

  // no parameters, no command: just display help and exit
  if ((optind == 1) && (argc == 1)) {
    help();
    return 0;
  } // if optind

  if (optind < argc) sprintf(sprefix, "b2b_%s", argv[optind]);
  else {
    fprintf(stderr, "%s: missing non optional argument <system prefix>\n", program);
    exit(1);
  } // else optind

  if (getVersion) printf("%s: version %s\n", program, b2b_version_text(B2B_ARCHIVER_VERSION));

  for (i=0; i<B2B_NSID; i++) {
    sprintf(filename[i], "%s_%s_sid%02d.dat", fprefix, sprefix, i);
    printf("open data file %s\n", filename[i]);
    if (!(dataFile = fopen(filename[i], fileMode))) {
      fprintf(stderr, "%s: can't open file %s\n", program, filename[i]);
      exit (1);
    } // if !file
    fprintf(dataFile, "%s\n", headerString());
    fclose(dataFile);
  } // for i

  dicSubscribeServices(sprefix);
    
  while (1) sleep(1);

  // hm... maybe we should close the files cleanly
  
  return exitCode;
}

/********************************************************************************************
 *  b2b-pm.c
 *
 *  created : 2019
 *  author  : Dietrich Beck, GSI-Darmstadt
 *  version : 19-Aug-2024
 *
 *  firmware required for measuring the h=1 phase for ring machine
 *  
 *  - when receiving B2B_ECADO_PRXX or B2B_ECADO_DIAGXXX, the phase is measured as a timestamp for an 
 *    arbitraty period
 *  - the phase timestamp is then sent as a timing message to the network
 *  units of time are
 *  in case of no suffix         : ns
 *  in case of suffix such as _as: as
 *  in case of suffix _t         : b2bt type
 *  
 * -------------------------------------------------------------------------------------------
 * License Agreement for this software:
 *
 * Copyright (C) 2018  Dietrich Beck
 * GSI Helmholtzzentrum fuer Schwerionenforschung GmbH
 * Planckstrasse 1
 * D-64291 Darmstadt
 * Germany
 *
 * Contact: d.beck@gsi.de
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either

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
 ********************************************************************************************/
#define B2BPM_FW_VERSION      0x000704                                  // make this consistent with makefile

// standard includes
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <stdint.h>

// includes specific for bel_projects
#include "dbg.h"                                                        // debug outputs
#include <stack.h>                                                      // stack check
#include "ebm.h"                                                        // EB master
#include "pp-printf.h"                                                  // print
#include "mini_sdb.h"                                                   // sdb stuff
#include "aux.h"                                                        // cpu and IRQ
#include "uart.h"                                                       // WR console

// includes for this project 
#include <common-defs.h>                                                // common defs for firmware
#include <common-fwlib.h>                                               // common routines for firmware
#include <b2b.h>                                                        // specific defs for b2b
#include <b2bpm_shared_mmap.h>                                          // autogenerated upon building firmware

// stuff required for environment
extern uint32_t* _startshared[];
unsigned int     cpuId, cpuQty;
#define  SHARED  __attribute__((section(".shared")))
uint64_t SHARED  dummy = 0;

// global variables 
volatile uint32_t *pShared;             // pointer to begin of shared memory region
volatile uint32_t *pSharedGetGid;       // pointer to a "user defined" u32 register; here: group ID of extraction machine
volatile uint32_t *pSharedGetSid;       // pointer to a "user defined" u32 register; here: sequence ID of extraction machine
volatile uint32_t *pSharedGetTH1Hi;     // pointer to a "user defined" u32 register; here: period of h=1, high bits
volatile uint32_t *pSharedGetTH1Lo;     // pointer to a "user defined" u32 register; here: period of h=1, low bits
volatile uint32_t *pSharedGetNH;        // pointer to a "user defined" u32 register; here: harmonic number

uint32_t *cpuRamExternal;               // external address (seen from host bridge) of this CPU's RAM            

uint64_t statusArray;                   // all status infos are ORed bit-wise into statusArray, statusArray is then published
uint32_t nTransfer;                     // # of transfers
uint32_t transStat;                     // status of transfer, here: meanDelta of 'poor mans fit'
int32_t  comLatency;                    // latency for messages received via ECA [ns]
int32_t  offsDone;                      // offset deadline WR message to time when we are done [ns]
int32_t  maxComLatency;
uint32_t maxOffsDone;
uint32_t nLate;                         // # of late messages


// for phase measurement
uint64_t tStamp[B2B_NSAMPLES];          // timestamp samples

// debug 
uint64_t t1, t2;
int32_t  tmp1;

void init() // typical init for lm32
{
  discoverPeriphery();        // mini-sdb ...
  uart_init_hw();             // needed by WR console   
  cpuId = getCpuIdx();
} // init


// determine address and clear shared mem
void initSharedMem(uint32_t *reqState, uint32_t *sharedSize)
{
  uint32_t idx;
  uint32_t *pSharedTemp;
  int      i; 
  const uint32_t c_Max_Rams = 10;
  sdb_location found_sdb[c_Max_Rams];
  sdb_location found_clu;
  
  // get pointer to shared memory
  pShared                 = (uint32_t *)_startshared;

  // get address to data
  pSharedGetGid           = (uint32_t *)(pShared + (B2B_SHARED_GET_GID        >> 2));
  pSharedGetSid           = (uint32_t *)(pShared + (B2B_SHARED_GET_SID        >> 2));
  pSharedGetTH1Hi         = (uint32_t *)(pShared + (B2B_SHARED_GET_TH1EXTHI   >> 2));   // for simplicity: use 'EXT' for data
  pSharedGetTH1Lo         = (uint32_t *)(pShared + (B2B_SHARED_GET_TH1EXTLO   >> 2));
  pSharedGetNH            = (uint32_t *)(pShared + (B2B_SHARED_GET_NHEXT      >> 2));

  // find address of CPU from external perspective
  idx = 0;
  find_device_multi(&found_clu, &idx, 1, GSI, LM32_CB_CLUSTER);
  if (idx == 0) {
    *reqState = COMMON_STATE_FATAL;
    DBPRINT1("b2b-pm: fatal error - did not find LM32-CB-CLUSTER!\n");
  } // if idx
  idx = 0;
  find_device_multi_in_subtree(&found_clu, &found_sdb[0], &idx, c_Max_Rams, GSI, LM32_RAM_USER);
  if (idx == 0) {
    *reqState = COMMON_STATE_FATAL;
    DBPRINT1("b2b-pm: fatal error - did not find THIS CPU!\n");
  } // if idx
  else cpuRamExternal = (uint32_t *)(getSdbAdr(&found_sdb[cpuId]) & 0x7FFFFFFF); // CPU sees the 'world' under 0x8..., remove that bit to get host bridge perspective

  DBPRINT2("b2b-pm: CPU RAM external 0x%8x, shared offset 0x%08x\n", cpuRamExternal, SHARED_OFFS);
  DBPRINT2("b2b-pm: fw common shared begin   0x%08x\n", pShared);
  DBPRINT2("b2b-pm: fw common shared end     0x%08x\n", pShared + (COMMON_SHARED_END >> 2));

  // clear shared mem
  i = 0;
  pSharedTemp        = (uint32_t *)(pShared + (COMMON_SHARED_END >> 2 ) + 1);
  DBPRINT2("b2b-pm: fw specific shared begin 0x%08x\n", pSharedTemp);
  while (pSharedTemp < (uint32_t *)(pShared + (B2B_SHARED_END >> 2 ))) {
    *pSharedTemp = 0x0;
    pSharedTemp++;
    i++;
  } // while pSharedTemp
  DBPRINT2("b2b-pm: fw specific shared end   0x%08x\n", pSharedTemp);

  *sharedSize        = (uint32_t)(pSharedTemp - pShared) << 2;

  // basic info to wr console
  DBPRINT1("\n");
  DBPRINT1("b2b-pm: initSharedMem, shared size [bytes]: %d\n", *sharedSize);
  DBPRINT1("\n");
} // initSharedMem 


// clear project specific diagnostics
void extern_clearDiag()
{
  statusArray   = 0x0; 
  nTransfer     = 0;
  transStat     = 0;
  nLate         = 0x0;
  comLatency    = 0x0;
  maxComLatency = 0x0;
  offsDone      = 0x0;
  maxOffsDone   = 0x0;
} // extern_clearDiag
  

// entry action 'configured' state
uint32_t extern_entryActionConfigured()
{
  uint32_t status = COMMON_STATUS_OK;

  // disable input gate 
  fwlib_ioCtrlSetGate(0, 2);

  // configure EB master (SRC and DST MAC/IP are set from host)
  if ((status = fwlib_ebmInit(2000, 0xffffffffffff, 0xffffffff, EBM_NOREPLY)) != COMMON_STATUS_OK) {
    DBPRINT1("b2b-pm: ERROR - init of EB master failed! %u\n", (unsigned int)status);
    return status;
  } 

  // get and publish NIC data
  fwlib_publishNICData();

  return status;
} // extern_entryActionConfigured


// entry action 'operation' state
uint32_t extern_entryActionOperation()
{
  int      i;
  uint64_t tDummy;
  uint64_t eDummy;
  uint64_t pDummy;
  uint32_t fDummy;
  uint32_t flagDummy1, flagDummy2, flagDummy3, flagDummy4;

  // clear diagnostics
  fwlib_clearDiag();             

  // flush ECA queue for lm32
  i = 0;
  while (fwlib_wait4ECAEvent(1000, &tDummy, &eDummy, &pDummy, &fDummy, &flagDummy1, &flagDummy2, &flagDummy3, &flagDummy4) !=  COMMON_ECADO_TIMEOUT) {i++;}
  DBPRINT1("b2b-pm: ECA queue flushed - removed %d pending entries from ECA queue\n", i);

  // init get values
  *pSharedGetTH1Hi       = 0x0;
  *pSharedGetTH1Lo       = 0x0;
  *pSharedGetNH          = 0x0;
  *pSharedGetGid         = 0x0; 
  *pSharedGetSid         = 0x0;

  nLate                  = 0x0;
  comLatency             = 0x0;
  maxComLatency          = 0x0;
  offsDone               = 0x0;
  maxOffsDone            = 0x0;

  return COMMON_STATUS_OK;
} // extern_entryActionOperation


uint32_t extern_exitActionOperation()
{
  return COMMON_STATUS_OK;
} // extern_exitActionOperation


// sort timestamps as they might be unordered
// for 11/31/51/61/101 timestamps, this is below 10,16,27,31,51 us
void insertionSort(uint64_t *stamps, int n) {
  int      i, j;
  uint64_t tmp;
  for (i=1; i<n; i++) {
    tmp = stamps[i];
    j   = i;
    while ((--j >= 0) && (stamps[j] > tmp)) stamps[j+1] = stamps[j];
    stamps[j+1] = tmp;
  } // for i
} // insertionSort


// perform fit of phase with sub-ns
int32_t phaseFitAverage(uint64_t TH1_as, uint32_t nSamples, b2bt_t *phase_t) {
  int64_t  one_ns_as   = 1000000000;        // conversion ns to as
  int64_t  max_diff_as = TH1_as >> 2;       // maximum difference shall be a quarter of the rf-period
  
  int      i;
  uint64_t tFirst_ns;                       // first timestamp [ns]
  //uint64_t nPeriods;                      // number of rf periods
  uint64_t diff_stamp_as;                   // difference between actual and first timestamp [as]
  int64_t  sum_rfperiods_as;                // sum of all rf-periods [as] 
  int64_t  deviation_as;                    // deviation between measured and projected timestamp [as]
  int64_t  abs_deviation_as;                // absolute value of deviation [as]
  int64_t  sum_deviation_as;                // sum of all deviations [as]
  int64_t  ave_deviation_as;                // average of all deviations [as]
  int64_t  max_deviation_as;                // maximum of all deviations [as]
  int64_t  min_deviation_as;                // minimum of all deviations [as]
  int64_t  subnsfit_dev_as;                 // sub-ns-fit deviation [as]
  uint32_t window_as;                       // window given by max and min deviation
  b2bt_t   ts_t;                            // timestamp [ps]
  int      nGood;                           // number of good timestamps

  // The idea is similar to the native sub-ns fit. As the main difference, the fractional part is 
  // not calculated by the _two_ extremes only, but by using the average of _all_ samples.
  // The algorithm is as follows
  // - always start from the 1st 'good' timestamp (which is the tStamp[1], tStamp[0] might be bad)
  // - for tStamp[i], add (i-1)*rf-period to the first timestamp and calc the difference to tStamp[i]
  // - average all the differences -> one obtains the mean value of all differences
  // - use the mean values as fractional part and add this to the value of tStamp[1]
  // Be aware: timestamps are sorted, but maybe incomplete. The algorithm stops at the first missing timestamp.

  if (TH1_as==0)    return B2B_STATUS_PHASEFAILED; // rf period must be known
  if (nSamples < 3) return B2B_STATUS_PHASEFAILED; // need at least three measurements

  // init stuff
  tFirst_ns = tStamp[1];              // don't use 1st timestamp tStamp[0]: start with 2nd timestamp tStamp[1]
  sum_deviation_as = 0;
  sum_rfperiods_as = 0;
  nGood            = 0;
  max_deviation_as = 0;
  min_deviation_as = 0;

  // calc sum deviation of all timestamps
  for (i=1; i<nSamples; i++) {        // include 1st timestamp too (important for little statistics)
    // for (i=1; i<27; i++) {  // HACK     // include 1st timestamp too (important for little statistics)
    
    // this multiplication is expensive
    diff_stamp_as = (tStamp[i] - tFirst_ns) * one_ns_as;

    // we use ther number of iterations as the number of rf-periods; thus, the algorithm will
    // stop at the first missing timestamp
    deviation_as  = diff_stamp_as - sum_rfperiods_as;

    if (deviation_as < 0) abs_deviation_as = -deviation_as;
    else                  abs_deviation_as =  deviation_as;
    // do a simple consistency check - don't accept differences larger than max_diff
    if (abs_deviation_as < max_diff_as) {
      sum_deviation_as += deviation_as;
      nGood++;
      // simple statistics
      if (deviation_as > max_deviation_as) max_deviation_as = deviation_as;
      if (deviation_as < min_deviation_as) min_deviation_as = deviation_as;
    } // if abs_deviation in range

    // increment rf period for next iteration
    sum_rfperiods_as += TH1_as;
  } // for i

  // if result invalid, return with error
  if (nGood < 1) {
    (*phase_t).ns  = 0x7fffffffffffffff;
    (*phase_t).ps  = 0x7fffffff;
    (*phase_t).dps = 0x7fffffff;
    return B2B_STATUS_PHASEFAILED;
  } // if nGood < 1

  // calculate average and max-min window
  ave_deviation_as = sum_deviation_as / nGood;
  subnsfit_dev_as  = (max_deviation_as + min_deviation_as) >> 1; 
  window_as        = (uint32_t)(max_deviation_as - min_deviation_as);
  // calculate a phasmax_deviation_as + me value and convert to ps
  ts_t.ns          = tFirst_ns;
  if (B2B_FW_USESUBNSFIT) ts_t.ps = subnsfit_dev_as  / 1000000;  // sub-ns fit
  else                    ts_t.ps = ave_deviation_as / 1000000;  // average fit
  ts_t.dps         = window_as >> 20;                // cheap division by 1000000
  *phase_t         = fwlib_cleanB2bt(ts_t);

  //pp_printf("nSamples %d, nGood %d, correction [ps] %d, dt [ps] %d\n", nSamples, nGood, ts_t.ps, ts_t.dps);
  return COMMON_STATUS_OK;
} //phaseFitSubNs


// 'fit' phase value
// this takes about 38/54/72/115us per 11/31/51/101 samples
uint32_t phaseFit(uint64_t period_as, uint32_t nSamples, b2bt_t *phase_t)
{
  int      i;
  int      usedIdx;         // index of used timestamp
  int32_t  diff;            // difference of two neighboring timestamps [ns]
  int32_t  delta;           // difference from expected period [ns]
  int32_t  dt;              // helper variable
  int32_t  periodNs;        // period [ns]
  uint32_t maxDelta;        // max deviation of measured period [ns]
  
  uint64_t phaseTmp;        // intermediate value;
  uint64_t tmp;             // helper variable

  int64_t  dummy;           // value ignored
  int32_t  status;

  if (period_as == 0)         return B2B_STATUS_PHASEFAILED;           // this does not make sense
  if (nSamples   < 3)         return B2B_STATUS_PHASEFAILED;           // have at least three samples

  // select timestamp in the middle (never use the first TS)
  usedIdx  = nSamples >> 1;                                            // this is safe as we have at least three samples

  // check period
  periodNs = (int32_t)(period_as/1000000000);                          // convert to ns chk: consider right-shift by 30 bits instead
  maxDelta = periodNs / 10;                                            // allow 10% deviation chk: consider right-shift by 3 bits instead
  diff     = (int32_t)(tStamp[usedIdx+1] - tStamp[usedIdx]);           // time difference of selected timestamps
  delta    = diff % periodNs;                                          // difference is multiple of period? (missing timestamp possible)!
  if (delta > (periodNs >> 1)) dt = periodNs - delta;
  else                         dt = delta;
  if (dt > maxDelta) {
    //pp_printf("ohps,  delta %d, usedIDX %d\n", delta, usedIdx);
    return B2B_STATUS_PHASEFAILED;
  } // if dt

  // phase fit
  status = phaseFitAverage(period_as, nSamples, phase_t);
  
  //status = phaseFitSubNs(period_as, nSamples, phase_ns, phase_125ps, &dummy, confidence_as);
  //dummy  = dummy/1000000;

  // if phase fit failed: plan B
  if (status != COMMON_STATUS_OK) {
    (*phase_t).ns  = tStamp[usedIdx];                                  // just grab a timestamp in the middle
    (*phase_t).ps  = 0;
    (*phase_t).dps = 2000;                                             // conservative estimate of uncertainty
    status         = COMMON_STATUS_OK;                                 // assume this is ok ...
  } // if status

  return status;
} //phaseFit


// aquires a series of timestamps from an IO; returns '0' on success
// takes about 
// - 4.0 us per timestamp
// - + 'interval'
int acquireTimestamps(uint64_t *ts,                           // array of timestamps [ns]
                      uint32_t nReq,                          // number of requsted timestamps
                      uint32_t *nRec,                         // number of received timestamps
                      uint32_t interval,                      // interval for receiving timestamps [us]
                      uint32_t io,                            // number of IO 0..n
                      uint32_t tag                            // expected event tag
                      )
{
  uint32_t ecaAction;                                         // action triggered by ECA
  uint64_t recDeadline;                                       // received deadline
  uint64_t recEvtId;                                          // received EvtId
  uint64_t recParam;                                          // received Parameter
  uint32_t recTEF;                                            // received TEF
  uint32_t flagIsLate;                                        // flag indicating that we received a 'late' event from ECA
  uint32_t flagIsEarly;                                       // flag 'early'
  uint32_t flagIsConflict;                                    // flag 'conflict'
  uint32_t flagIsDelayed;                                     // flag 'delayed'

  *nRec = 0;
  
  fwlib_ioCtrlSetGate(1, io);                                 // open input gate
  uwait(interval);
  fwlib_ioCtrlSetGate(0, io);                                 // disable input gate 
  
  // reading an action from ECA
  while (*nRec < nReq) {
    //t1 = getSysTime();
    ecaAction = fwlib_wait4ECAEvent(1, &recDeadline, &recEvtId, &recParam, &recTEF, &flagIsLate, &flagIsEarly, &flagIsConflict, &flagIsDelayed);
    //t2 = getSysTime();
    //tmp1 = (int32_t)(t2 - t1);
    //pp_printf("dtns %ld\n", tmp1);
    if (ecaAction == tag) {ts[*nRec] = recDeadline; (*nRec)++;}
    if (ecaAction == B2B_ECADO_TIMEOUT) break; 
  } // while nInput

  //pp_printf("intervalus %u, nRec %u \n", interval, *nRec);

  return COMMON_STATUS_OK;
} // acquireTimestamps                  


// do action of state operation: This is THE central code of this firmware
uint32_t doActionOperation(uint64_t *tAct,                    // actual time
                           uint32_t actStatus)                // actual status of firmware
{
  uint32_t status;                                            // status returned by routines
  uint32_t flagIsLate;                                        // flag indicating that we received a 'late' event from ECA
  uint32_t flagIsEarly;                                       // flag 'early'
  uint32_t flagIsConflict;                                    // flag 'conflict'
  uint32_t flagIsDelayed;                                     // flag 'delayed'
  uint32_t ecaAction;                                         // action triggered by event received from ECA
  uint64_t recDeadline;                                       // deadline received from ECA
  uint64_t reqDeadline;                                       // deadline requested by sender
  uint64_t recEvtId;                                          // evt ID received
  uint64_t recParam;                                          // param received
  uint32_t recTEF;                                            // TEF received
  uint32_t recGid;                                            // GID received
  uint32_t recSid;                                            // SID received
  uint32_t recBpid;                                           // BPID received
  uint64_t sendDeadline;                                      // deadline to send
  uint64_t sendEvtId;                                         // evtid to send
  uint64_t sendParam;                                         // parameter to send
  uint32_t sendTEF;                                           // TEF to send
  uint32_t sendEvtNo;                                         // EvtNo to send
  uint64_t sysTime; 
  
  // phase measurement
  uint32_t nInput;                                            // # of timestamps
  static uint64_t TH1_as;                                     // h=1 period [as]
  static b2bt_t   tH1_t;                                      // h=1 timestamp of phase ( = 'phase') [ps]
  uint32_t dt_pss;                                            // uncertainty of h=1 timestamp [ps]
  static uint32_t flagPMError;                                // error flag phase measurement

  // diagnostic PM; phase (rf) and match (trigger)
  static uint32_t flagMatchDone;                              // flag: match measurement done
  static uint32_t flagPhaseDone;                              // flag: phase meausrement done
  b2bt_t   tH1Match_t;                                        // h=1 timestamp of match diagnostic [ps]
  b2bt_t   tH1Phase_t;                                        // h=1 timestamp of phase diagnostic [ps]
  uint64_t remainder;                                         // remainder for modulo operations
  static int64_t dtMatch_as;                                  // deviation of trigger from expected timestamp [as]
  int64_t  dtPhase_as;                                        // deviation of phase from expected timestamp [as]

  int      i;
  static uint32_t nSamples;                                   // # of samples for measurement
  static uint64_t TMeas;                                      // measurement window for timestamps [ns]
  static uint32_t TMeas_us;                                   // measurement window [us]
  int64_t  TWait;                                             // time till measurement start [ns]
  int32_t  TWait_us;                                          // time till measurement start [us]

  fdat_t   tmp;                                               // for copying of data
  
  status    = actStatus;
  sendEvtNo = 0x0;

  ecaAction = fwlib_wait4ECAEvent(COMMON_ECATIMEOUT * 1000, &recDeadline, &recEvtId, &recParam, &recTEF, &flagIsLate, &flagIsEarly, &flagIsConflict, &flagIsDelayed);

  //if (ecaAction != B2B_ECADO_TIMEOUT) comLatency = (int32_t)(getSysTime() - recDeadline);

  switch (ecaAction) {
    // the following two cases handle h=1 group DDS phase measurement
    case B2B_ECADO_B2B_PMEXT :                                        // this is an OR, no 'break' on purpose
      sendEvtNo   = B2B_ECADO_B2B_PREXT;
    case B2B_ECADO_B2B_PMINJ :
      if (!sendEvtNo) sendEvtNo = B2B_ECADO_B2B_PRINJ;
      comLatency = (int32_t)(getSysTime() - recDeadline);

      //t1 = getSysTime();
      *pSharedGetTH1Hi = (uint32_t)((recParam >> 32) & 0x000fffff);   // lower 52 bit used as period
      *pSharedGetTH1Lo = (uint32_t)( recParam        & 0xffffffff);
      *pSharedGetNH    = (uint32_t)((recParam>> 56)  & 0xff      );   // upper 8 bit used as harmonic number
      TH1_as           = recParam & 0x000fffffffffffff;
      recGid           = (uint32_t)((recEvtId >> 48) & 0xfff     );
      recSid           = (uint32_t)((recEvtId >> 20) & 0xfff     );
      recBpid          = (uint32_t)((recEvtId >>  6) & 0x3fff    );
      *pSharedGetGid   = recGid;
      *pSharedGetSid   = recSid;
      flagMatchDone    = 0;
      flagPhaseDone    = 0;
      flagPMError      = 0x0;
      tH1_t.ns         = 0x6fffffffffffffff;                          // bogus number, might help for debugging chk

      nSamples                              = B2B_NSAMPLES;               
      if (TH1_as >  2500000000000) nSamples = B2B_NSAMPLES >> 1;      // use only 1/2 for nue < 400 kHz: 80us@400kHz and B2B_NSAMPLES=32
      if (TH1_as >  5000000000000) nSamples = B2B_NSAMPLES >> 2;      // use only 1/4 for nue < 200 kHz: 80us@200kHz and B2B_NSAMPLES=32
      if (TH1_as > 10000000000000) nSamples = B2B_NSAMPLES >> 3;      // use only 1/8 for nue < 100 kHz: 80us@100kHz and B2B_NSAMPLES=32
      if (TH1_as > 20000000000000) nSamples = 3;                      // use minimum for  nue <  50 kHz: 80us@ 27kHz and B2B_NSAMPLES=32

      TMeas           = (uint64_t)(nSamples)*(TH1_as >> 30);          // window for acquiring timestamps ~[ns]; use bitshift as division
      TMeas_us        = (int32_t)(TMeas >> 10) + 16;                  // ~[ns] -> ~[us] add 16us to correct for too short window
      
      nInput = 0;
      acquireTimestamps(tStamp, nSamples, &nInput, TMeas_us, 2, B2B_ECADO_TLUINPUT3);

      if (nInput > 2) insertionSort(tStamp, nInput);                  
      if (phaseFit(TH1_as, nInput, &tH1_t) != COMMON_STATUS_OK) {
        if (sendEvtNo ==  B2B_ECADO_B2B_PREXT) flagPMError = B2B_ERRFLAG_PMEXT;
        else                                   flagPMError = B2B_ERRFLAG_PMINJ;
        if (nInput < 3) status = B2B_STATUS_NORF;
        else            status = B2B_STATUS_PHASEFAILED;
      } // if some error occured

      // send command: transmit measured phase value to the network
      sendEvtId    = fwlib_buildEvtidV1(recGid, sendEvtNo, 0, recSid, recBpid, flagPMError);
      sendParam    = tH1_t.ns;
      sendTEF      = (uint32_t)( (int16_t)(tH1_t.ps)  & 0xffff);
      sendTEF     |= (uint32_t)((uint16_t)(tH1_t.dps) & 0xffff) << 16; 
      sendDeadline = getSysTime() + (uint64_t)B2B_AHEADT;             // use a more aggressive deadline < COMMON_AHEADT
      fwlib_ebmWriteTM(sendDeadline, sendEvtId, sendParam, sendTEF, 0);
      //t2 = getSysTime();
      // send something to ECA (for monitoring purposes) chk do something useful here
      sysTime      = getSysTime();
      sendEvtId    = fwlib_buildEvtidV1(0xfff, ecaAction, 0, recSid, recBpid, 0x0);
      sendParam    = 0xdeadbeef;
      sendDeadline = sysTime;                                         // produces a late action but allows explicit monitoring of processing time   
      fwlib_ecaWriteTM(sendDeadline, sendEvtId, sendParam, 0, 1);     // force late message

      offsDone     = sysTime - recDeadline;
      transStat    = tH1_t.dps;
      nTransfer++;

      //tmp1 = (int32_t)(t2-t1);
      //pp_printf("pmns %ld\n", tmp1); 
      
      //flagIsLate = 0; /* chk */      
      break; // case B2B_ECADO_B2B_PMEXT

    // the following two cases handle phase matching diagnostic and measure the skew between kicker trigger and h=1 group DDS signals
    case B2B_ECADO_B2B_TRIGGEREXT :                                   // this is an OR, no 'break' on purpose
    case B2B_ECADO_B2B_TRIGGERINJ :                                   // this case only makes sense if cases B2B_ECADO_B2B_PMEXT/INJ succeeded
      if (!flagPMError) {
        comLatency = (int32_t)(getSysTime() - recDeadline);
        reqDeadline = recDeadline + (uint64_t)B2B_PRETRIGGERTR;       // ECA is configured to pre-trigger ahead of time!!!
        nInput   = 0;
        TWait    = (int64_t)((reqDeadline - (TMeas >> 1)) - getSysTime());  // time how long we should wait before starting the measurement
        TWait_us = (TWait / 1000 - 10);                               // the '-10' is a fudge thing
        if (TWait_us > 0) uwait(TWait_us);
        acquireTimestamps(tStamp, nSamples, &nInput, TMeas_us, 2, B2B_ECADO_TLUINPUT3);
        //pp_printf("TMeas %u, TMeasUs %u\n", (uint32_t)TMeas, (uint32_t)TMeasUs);

        // find closest timestamp
        if (nInput > 2) {
          insertionSort(tStamp, nInput);                              // need at least two timestamps
          if (phaseFit(TH1_as, nInput, &tH1Match_t) == COMMON_STATUS_OK) {
            dtMatch_as  = (reqDeadline - tH1Match_t.ns);              // ns
            //pp_printf("match ns %4d, ps %4d, ", (int32_t)dtMatch_as, (int32_t)tH1Match_t.ps);
            dtMatch_as  = dtMatch_as * 1000 - tH1Match_t.ps;          // ps
            //pp_printf("match ps %d\n", (int32_t)dtMatch_as);
            dtMatch_as *= 1000000;                                    // difference [as]
            /* prior 'remainder' one would need to subtract the trigger offset - don't do this here
            remainder   =  Dt % TH1_as;                               // remainder [as]
            if (remainder > (TH1_as >> 1)) dtMatch_as = remainder - TH1_as;
            else                           dtMatch_as = remainder;*/
            // hack
            flagMatchDone = 1;
            // hack for testing
            //tH1_t = tH1Match_t;
            // tmp1 = (int32_t)(dtMatch_as / 125000000); pp_printf("match2 [125 ps] %08d\n", tmp1);
          } // if phasefit
        } // if nInput
        
        // send something to ECA (for monitoring purposes), chk do something useful here
        // sendEvtId    = fwlib_buildEvtidV1(0xfff, ecaAction, 0, recSid, recBpid, 0x0);
        // sendParam    = 0xdeadbeef;
        // sendDeadline = getSysTime();                                  // produces a late action but allows explicit monitoring of processing time
        // fwlib_ecaWriteTM(sendDeadline, sendEvtId, sendParam, 0, 1);   // force late message
      } // if not pm error
      //flagIsLate = 0; /* chk */
      
      break; // case  B2B_ECADO_B2B_TRIGGEREXT/INJ

    // the following two cases handle frequency diagnostic and measure the skew between expected and h=1 group DDS signals
    case B2B_ECADO_B2B_PDEXT :                                        // this is an OR, no 'break' on purpose
      sendEvtNo   = B2B_ECADO_B2B_DIAGEXT;
    case B2B_ECADO_B2B_PDINJ :
      if (!sendEvtNo) sendEvtNo = B2B_ECADO_B2B_DIAGINJ;
      
      comLatency = (int32_t)(getSysTime() - recDeadline);
      
      recGid          = (uint32_t)((recEvtId >> 48) & 0xfff     );
      recSid          = (uint32_t)((recEvtId >> 20) & 0xfff     );
      recBpid         = (uint32_t)((recEvtId >>  6) & 0x3fff    );
      
      nInput          = 0;

      acquireTimestamps(tStamp, nSamples, &nInput, TMeas_us, 2, B2B_ECADO_TLUINPUT3);
      //pp_printf("nInput %d \n", nInput);
      // find closest timestamp
      if (nInput > 2) {
        insertionSort(tStamp, nInput);                                // need at least two timestamps
        if (phaseFit(TH1_as, nInput, &tH1Phase_t) == COMMON_STATUS_OK) {
          dtPhase_as   = (tH1Phase_t.ns - tH1_t.ns) * 1000000000;     // difference [as]
          dtPhase_as  += (tH1Phase_t.ps - tH1_t.ps) * 1000000;
          remainder   =  dtPhase_as % TH1_as;                         // remainder [as]
          if (remainder > (TH1_as >> 1)) dtPhase_as = remainder - TH1_as;
          else                           dtPhase_as = remainder;
          flagPhaseDone = 1;          
        } // if phasefit
      } // if nInput

      // send command: transmit diagnostic information to the network
      sendEvtId    = fwlib_buildEvtidV1(recGid, sendEvtNo, 0, recSid, recBpid, 0);
      if (flagPhaseDone) tmp.f = (float)dtPhase_as / 1000000000.0;    // convert to float [ns]
      else               tmp.data = 0x7fffffff;                       // mark as invalid
      sendParam    = (uint64_t)(tmp.data & 0xffffffff) << 32;         // high word; phase diagnostic
      if (flagMatchDone) tmp.f = (float)dtMatch_as / 1000000000.0;    // convert to float [ns]
      else               tmp.data = 0x7fffffff;                       // mark as invalid

      //tmp1 = (int32_t)(dtMatch_as / 1000000); pp_printf("match3 [ps] %08d\n", tmp1); //pp_printf("match3 [hex float ns] %08x\n", tmp.data);
      
      sendParam   |= (uint64_t)(tmp.data & 0xffffffff);               // low word; match diagnostic
      sendDeadline = getSysTime() + (uint64_t)COMMON_AHEADT;          // use a more conservative deadline
      fwlib_ebmWriteTM(sendDeadline, sendEvtId, sendParam, 0, 0);
      
      // send something to ECA (for monitoring purposes) chk do something useful here
      sendEvtId    = fwlib_buildEvtidV1(0xfff, ecaAction, 0, recSid, recBpid, 0x0);
      sendParam    = 0xdeadbeef;
      sendDeadline = getSysTime();                                    // produces a late action but allows explicit monitoring of processing time
      fwlib_ecaWriteTM(sendDeadline, sendEvtId, sendParam, 0, 1);     // force late message
      //flagIsLate = 0; /* chk */
      break; // case  B2B_ECADO_B2B_PDEXT/INJ
      
    default :                                                         // flush ECA queue
      flagIsLate = 0;                                                 // ignore late events in this case
  } // switch ecaAction
 
  // check for late event
  if ((status == COMMON_STATUS_OK) && flagIsLate) {
    status = B2B_STATUS_LATEMESSAGE;
    nLate++;
  } // if status
  
  // check WR sync state
  if (fwlib_wrCheckSyncState() == COMMON_STATUS_WRBADSYNC) return COMMON_STATUS_WRBADSYNC;
  else                                                     return status;
} // doActionOperation


int main(void) {
  uint64_t tActCycle;                           // time of actual UNILAC cycle
  uint32_t status;                              // (error) status
  uint32_t actState;                            // actual FSM state
  uint32_t pubState;                            // value of published state
  uint32_t reqState;                            // requested FSM state
  uint32_t dummy1;                              // dummy parameter
  uint32_t sharedSize;                          // size of shared memory
  uint32_t *buildID;                            // build ID of lm32 firmware
 
  // init local variables
  buildID        = (uint32_t *)(INT_BASE_ADR + BUILDID_OFFS);                 // required for 'stack check'  

  reqState       = COMMON_STATE_S0;
  actState       = COMMON_STATE_UNKNOWN;
  pubState       = COMMON_STATE_UNKNOWN;
  status         = COMMON_STATUS_OK;
  nTransfer      = 0;

  init();                                                                     // initialize stuff for lm32
  initSharedMem(&reqState, &sharedSize);                                      // initialize shared memory
  fwlib_init((uint32_t *)_startshared, cpuRamExternal, SHARED_OFFS, sharedSize, "b2b-pm", B2BPM_FW_VERSION); // init common stuff
  fwlib_clearDiag();                                                          // clear common diagnostics data
  
  while (1) {
    check_stack_fwid(buildID);                                                // check stack status
    fwlib_cmdHandler(&reqState, &dummy1);                                     // check for commands and possibly request state changes
    status = COMMON_STATUS_OK;                                                // reset status for each iteration

    // state machine
    status = fwlib_changeState(&actState, &reqState, status);                 // handle requested state changes
    switch(actState) {                                                        // state specific do actions
      case COMMON_STATE_OPREADY :
        status = doActionOperation(&tActCycle, status);
        if (status == COMMON_STATUS_WRBADSYNC)      reqState = COMMON_STATE_ERROR;
        if (status == COMMON_STATUS_ERROR)          reqState = COMMON_STATE_ERROR;
        break;
      default :                                                               // avoid flooding WB bus with unnecessary activity
        status = fwlib_doActionState(&reqState, actState, status);            // other 'do actions' are handled here
        break;
    } // switch
    
    switch (status) {
      case COMMON_STATUS_OK :                                                 // status OK
        statusArray = statusArray |  (0x1 << COMMON_STATUS_OK);               // set OK bit
        break;
      default :                                                               // status not OK
        if ((statusArray >> COMMON_STATUS_OK) & 0x1) fwlib_incBadStatusCnt(); // changing status from OK to 'not OK': increase 'bad status count'
        statusArray = statusArray & ~((uint64_t)0x1 << COMMON_STATUS_OK);     // clear OK bit
        statusArray = statusArray |  ((uint64_t)0x1 << status);               // set status bit and remember other bits set
        break;
    } // switch status
    
    if ((pubState == COMMON_STATE_OPREADY) && (actState  != COMMON_STATE_OPREADY)) fwlib_incBadStateCnt();
    fwlib_publishStatusArray(statusArray);
    pubState = actState;
    fwlib_publishState(pubState);
    if (comLatency > maxComLatency) maxComLatency = comLatency;
    if (offsDone   > maxOffsDone)   maxOffsDone   = offsDone;
    fwlib_publishTransferStatus(nTransfer, 0x0, transStat, nLate, maxOffsDone, maxComLatency); 
  } // while

  return(1); // this should never happen ...
} // main

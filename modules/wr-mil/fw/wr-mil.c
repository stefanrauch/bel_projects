/********************************************************************************************
 *  wr-mil.c
 *
 *  created : 2024
 *  author  : Dietrich Beck, Micheal Reese, Mathias Kreider GSI-Darmstadt
 *  version : 30-Sep-2024
 *
 *  firmware required for the White Rabbit -> MIL Gateways
 *  
 *  This firmware is a refactured fork of the original 'wr-mil-gw' developed for SIS18 and ESR
 *  by Michael Reese and others. The fork was triggered by the need of a wr-mil-gateway at UNILAC. 
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
#define WRMIL_FW_VERSION      0x000012    // make this consistent with makefile

#define RESET_INHIBIT_COUNTER    10000    // count so many main ECA timemouts, prior sending fill event
//#define WR_MIL_GATEWAY_LATENCY 70650    // additional latency in units of nanoseconds
                                          // this value was determined by measuring the time difference
                                          // of the MIL event rising edge and the ECA output rising edge (no offset)
                                          // and tuning this time difference to 100.0(5)us

// standard includes
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <stdint.h>

// includes specific for bel_projects
#include "dbg.h"                                                        // debug outputs
#include <stack.h>                                                      // stack check
#include "pp-printf.h"                                                  // print
#include "mini_sdb.h"                                                   // sdb stuff
#include "aux.h"                                                        // cpu and IRQ
#include "uart.h"                                                       // WR console
#include "../../../top/gsi_scu/scu_mil.h"                               // register layout of 'MIL macro'

// includes for this project 
#include <common-defs.h>                                                // common defs for firmware
#include <common-fwlib.h>                                               // common routines for firmware
#include <wr-mil.h>                                                     // specific defs for wr-mil
#include <wrmil_shared_mmap.h>                                          // autogenerated upon building firmware

// stuff required for environment
extern uint32_t* _startshared[];
unsigned int     cpuId, cpuQty;
#define  SHARED  __attribute__((section(".shared")))
uint64_t SHARED  dummy = 0;

// global variables 
volatile uint32_t *pShared;                // pointer to begin of shared memory region
volatile uint32_t *pSharedSetUtcTrigger;   // pointer to a "user defined" u32 register; here: the MIL event that triggers the generation of UTC events
volatile uint32_t *pSharedSetUtcUtcDelay;  // pointer to a "user defined" u32 register; here: delay [us] between the 5 generated UTC MIL events
volatile uint32_t *pSharedSetTrigUtcDelay; // pointer to a "user defined" u32 register; here: delay [us] between the trigger event and the first UTC (and other) generated events
volatile uint32_t *pSharedSetGid;          // pointer to a "user defined" u32 register; here: GID the gateway will be using
volatile int32_t  *pSharedSetLatency;      // pointer to a "user defined" u32 register; here: MIL event is generated xxx us+latency after the WR event. The value of latency can be negative
volatile uint32_t *pSharedSetUtcOffsHi;    // pointer to a "user defined" u32 register; here: delay [ms] between the TAI and the MIL-UTC, high word
volatile uint32_t *pSharedSetUtcOffsLo;    // pointer to a "user defined" u32 register; here: delay [ms] between the TAI and the MIL-UTC, low word
volatile uint32_t *pSharedSetReqFillEvt;   // pointer to a "user defined" u32 register; here: if this is written to 1, the gateway will send a fill event as soon as possible
volatile uint32_t *pSharedSetMilDev;       // pointer to a "user defined" u32 register; here: MIL device for sending MIL messages; 0: MIL Piggy; 1..: SIO in slot 1..
volatile uint32_t *pSharedSetMilMon;       // pointer to a "user defined" u32 register; here: 1: monitor MIL events; 0; don't monitor MIL events
volatile uint32_t *pSharedGetNEvtsSndHi;   // pointer to a "user defined" u32 register; here: number of telegrams sent, high word
volatile uint32_t *pSharedGetNEvtsSndLo;   // pointer to a "user defined" u32 register; here: number of telegrams sent, high word
volatile uint32_t *pSharedGetNEvtsRecTHi;  // pointer to a "user defined" u32 register; here: number of telegrams received (TAI), high word
volatile uint32_t *pSharedGetNEvtsRecTLo;  // pointer to a "user defined" u32 register; here: number of telegrams received (TAI), high word
volatile uint32_t *pSharedGetNEvtsRecDHi;  // pointer to a "user defined" u32 register; here: number of telegrams received (data), high word
volatile uint32_t *pSharedGetNEvtsRecDLo;  // pointer to a "user defined" u32 register; here: number of telegrams received (data), high word
volatile uint32_t *pSharedGetNEvtsErr;     // pointer to a "user defined" u32 register; here: number of received MIL telegrams with errors, detected by VHDL manchester decoder
volatile uint32_t *pSharedGetNEvtsBurst;   // pointer to a "user defined" u32 register; here: number of occurences of 'nonsense high frequency bursts' 

uint32_t *cpuRamExternal;               // external address (seen from host bridge) of this CPU's RAM
volatile uint32_t *pMilSend;            // address of MIL device sending timing messages, usually this will be a SIO
volatile uint32_t *pMilRec;             // address of MIL device receiving timing messages, usually this will be a MIL piggy

uint64_t statusArray;                   // all status infos are ORed bit-wise into statusArray, statusArray is then published
uint64_t nEvtsSnd;                      // # of sent MIL telegrams
uint64_t nEvtsRecT;                     // # of received MIL telegrams (TAI)
uint64_t nEvtsRecD;                     // # of received MIL telegrams (data)
uint32_t nEvtsErr;                      // # of late messages with errors
uint32_t nEvtsBurst;                    // # of detected 'high frequency bursts'
uint32_t nEvtsLate;                     // # of late messages
uint32_t offsDone;                      // offset deadline WR message to time when we are done [ns]
int32_t  comLatency;                    // latency for messages received via ECA

int32_t  maxComLatency;
uint32_t maxOffsDone;


uint32_t utc_trigger;
int32_t  utc_utc_delay;
int32_t  trig_utc_delay;
uint64_t utc_offset;
uint32_t request_fill_evt;
int32_t  mil_latency;
uint32_t mil_domain;
uint32_t mil_mon;

uint32_t inhibit_fill_events = 0;       // this is a counter to block any sending of fill events for some cycles after a real event was sent

// constants (as variables to have a defined type)
uint64_t  one_us_ns = 1000;

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
  sdb_location   found_sdb[c_Max_Rams];
  sdb_location   found_clu;
  
  // get pointer to shared memory
  pShared                    = (uint32_t *)_startshared;

  // get address to data
  pSharedSetUtcTrigger       = (uint32_t *)(pShared + (WRMIL_SHARED_SET_UTC_TRIGGER       >> 2));
  pSharedSetUtcUtcDelay      = (uint32_t *)(pShared + (WRMIL_SHARED_SET_UTC_UTC_DELAY     >> 2));
  pSharedSetTrigUtcDelay     = (uint32_t *)(pShared + (WRMIL_SHARED_SET_TRIG_UTC_DELAY    >> 2));
  pSharedSetGid              = (uint32_t *)(pShared + (WRMIL_SHARED_SET_GID               >> 2));
  pSharedSetLatency          = (uint32_t *)(pShared + (WRMIL_SHARED_SET_LATENCY           >> 2));
  pSharedSetUtcOffsHi        = (uint32_t *)(pShared + (WRMIL_SHARED_SET_UTC_OFFSET_HI     >> 2));
  pSharedSetUtcOffsLo        = (uint32_t *)(pShared + (WRMIL_SHARED_SET_UTC_OFFSET_LO     >> 2));
  pSharedSetReqFillEvt       = (uint32_t *)(pShared + (WRMIL_SHARED_SET_REQUEST_FILL_EVT  >> 2));
  pSharedSetMilDev           = (uint32_t *)(pShared + (WRMIL_SHARED_SET_MIL_DEV           >> 2));
  pSharedSetMilMon           = (uint32_t *)(pShared + (WRMIL_SHARED_SET_MIL_MON           >> 2));
  pSharedGetNEvtsSndHi       = (uint32_t *)(pShared + (WRMIL_SHARED_GET_N_EVTS_SND_HI     >> 2));
  pSharedGetNEvtsSndLo       = (uint32_t *)(pShared + (WRMIL_SHARED_GET_N_EVTS_SND_LO     >> 2));
  pSharedGetNEvtsRecTHi      = (uint32_t *)(pShared + (WRMIL_SHARED_GET_N_EVTS_RECT_HI    >> 2));
  pSharedGetNEvtsRecTLo      = (uint32_t *)(pShared + (WRMIL_SHARED_GET_N_EVTS_RECT_LO    >> 2));
  pSharedGetNEvtsRecDHi      = (uint32_t *)(pShared + (WRMIL_SHARED_GET_N_EVTS_RECD_HI    >> 2));
  pSharedGetNEvtsRecDLo      = (uint32_t *)(pShared + (WRMIL_SHARED_GET_N_EVTS_RECD_LO    >> 2));
  pSharedGetNEvtsErr         = (uint32_t *)(pShared + (WRMIL_SHARED_GET_N_EVTS_ERR        >> 2));
  pSharedGetNEvtsBurst       = (uint32_t *)(pShared + (WRMIL_SHARED_GET_N_EVTS_BURST      >> 2));

  // find address of CPU from external perspective
  idx = 0;
  find_device_multi(&found_clu, &idx, 1, GSI, LM32_CB_CLUSTER);
  if (idx == 0) {
    *reqState = COMMON_STATE_FATAL;
    DBPRINT1("wr-mil: fatal error - did not find LM32-CB-CLUSTER!\n");
  } // if idx
  idx = 0;
  find_device_multi_in_subtree(&found_clu, &found_sdb[0], &idx, c_Max_Rams, GSI, LM32_RAM_USER);
  if (idx == 0) {
    *reqState = COMMON_STATE_FATAL;
    DBPRINT1("wr-mil: fatal error - did not find THIS CPU!\n");
  } // if idx
  else cpuRamExternal = (uint32_t *)(getSdbAdr(&found_sdb[cpuId]) & 0x7FFFFFFF); // CPU sees the 'world' under 0x8..., remove that bit to get host bridge perspective

  DBPRINT2("wr-mil: CPU RAM external 0x%8x, shared offset 0x%08x\n", cpuRamExternal, SHARED_OFFS);
  DBPRINT2("wr-mil: fw common shared begin   0x%08x\n", pShared);
  DBPRINT2("wr-mil: fw common shared end     0x%08x\n", pShared + (COMMON_SHARED_END >> 2));

  // clear shared mem
  i = 0;
  pSharedTemp        = (uint32_t *)(pShared + (COMMON_SHARED_END >> 2 ) + 1);
  DBPRINT2("wr-mil: fw specific shared begin 0x%08x\n", pSharedTemp);
  while (pSharedTemp < (uint32_t *)(pShared + (WRMIL_SHARED_END >> 2 ))) {
    *pSharedTemp = 0x0;
    pSharedTemp++;
    i++;
  } // while pSharedTemp
  DBPRINT2("wr-mil: fw specific shared end   0x%08x\n", pSharedTemp);

  *sharedSize        = (uint32_t)(pSharedTemp - pShared) << 2;

  // basic info to wr console
  DBPRINT1("\n");
  DBPRINT1("wr-mil: initSharedMem, shared size [bytes]: %d\n", *sharedSize);
  DBPRINT1("\n");
} // initSharedMem 


// clear project specific diagnostics
void extern_clearDiag()
{
  statusArray   = 0x0;
  nEvtsSnd      = 0x0;
  nEvtsRecT     = 0x0;
  nEvtsRecD     = 0x0;
  nEvtsErr      = 0x0;
  nEvtsBurst    = 0x0;
  nEvtsLate     = 0x0;
  offsDone      = 0x0;
  comLatency    = 0x0;
  maxOffsDone   = 0x0;
  maxComLatency = 0x0;
  resetEventErrCntMil(pMilRec, 0);
} // extern_clearDiag 


// configure SoC to receive events via MIL bus
uint32_t configMILEvents(int enable_fifo)
{
  uint32_t i,j;
  uint32_t fifo_mask;

  // initialize status and command register with initial values; disable event filtering; clear filter RAM
  if (writeCtrlStatRegEvtMil(pMilRec, 0, MIL_CTRL_STAT_ENDECODER_FPGA | MIL_CTRL_STAT_INTR_DEB_ON) != MIL_STAT_OK) return COMMON_STATUS_ERROR;

  // clean up 
  if (disableLemoEvtMil(pMilRec, 0, 1) != MIL_STAT_OK) return COMMON_STATUS_ERROR;
  if (disableLemoEvtMil(pMilRec, 0, 2) != MIL_STAT_OK) return COMMON_STATUS_ERROR;
  if (disableFilterEvtMil(pMilRec, 0)  != MIL_STAT_OK) return COMMON_STATUS_ERROR; 
  if (clearFilterEvtMil(pMilRec, 0)    != MIL_STAT_OK) return COMMON_STATUS_ERROR;

  if (enable_fifo) fifo_mask = MIL_FILTER_EV_TO_FIFO;
  else             fifo_mask = 0x0;

  for (i=0; i<(0xf+1); i++) { 
    for (j=0; j<(0xff+1); j++) {
      if (setFilterEvtMil(pMilRec, 0, j, i, fifo_mask | MIL_FILTER_EV_PULS1_S) != MIL_STAT_OK) return COMMON_STATUS_ERROR;
    } // for j
  } // for i

  // configure LEMO1 for pulse generation
  if (configLemoPulseEvtMil(pMilRec, 0, 1) != MIL_STAT_OK) return COMMON_STATUS_ERROR;

  return COMMON_STATUS_OK;
} // configMILEvents


// entry action 'configured' state
uint32_t extern_entryActionConfigured()
{
  uint32_t status = COMMON_STATUS_OK;

  // get and publish NIC data
  fwlib_publishNICData(); 

  // get address of MIL device sending MIL telegrams; 0 is MIL piggy
  if (*pSharedSetMilDev == 0){
    pMilSend = fwlib_getMilPiggy();
    if (!pMilSend) {
      DBPRINT1("wr-mil: ERROR - can't find MIL device; sender\n");
      return COMMON_STATUS_OUTOFRANGE;
    } // if !pMilSend
  } // if SetMilDev
  else {
    // SCU slaves have offsets 0x20000, 0x40000... for slots 1, 2 ...
    pMilSend = fwlib_getSbMaster();
    if (!pMilSend) {
      DBPRINT1("wr-mil: ERROR - can't find MIL device; sender\n");
      return COMMON_STATUS_OUTOFRANGE;
    } // if !pMilSend
    else pMilSend += *pSharedSetMilDev * 0x20000;
  } // else SetMilDev

  // reset MIL sender and wait
  if ((status = resetDevMil(pMilSend, 0))  != MIL_STAT_OK) {
    DBPRINT1("wr-mil: ERROR - can't reset MIL device; sender\n");
    return WRMIL_STATUS_MIL;
  }  // if reset

  // get address of MIL device receiving MIL telegrams; only piggy is supported
  //  if (*pSharedSetMilMon){
  if (1) {
    pMilRec = fwlib_getMilPiggy();
    if (!pMilRec) {
      DBPRINT1("wr-mil: ERROR - can't find MIL device; receiver\n");
      return COMMON_STATUS_OUTOFRANGE;
    } // if !pMilRec

    // reset MIL receiver and wait
    if ((status = resetDevMil(pMilRec, 0))  != MIL_STAT_OK) {
      DBPRINT1("wr-mil: ERROR - can't reset MIL device; receiver\n");
      return WRMIL_STATUS_MIL;
    }  // if reset
  } // if receiver

  // if everything is ok, we must return with COMMON_STATUS_OK
  if (status == MIL_STAT_OK) status = COMMON_STATUS_OK;

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
  int      enable_fifo;

  // clear diagnostics
  fwlib_clearDiag();

  // flush ECA queue for lm32
  i = 0;
  while (fwlib_wait4ECAEvent(1000, &tDummy, &eDummy, &pDummy, &fDummy, &flagDummy1, &flagDummy2, &flagDummy3, &flagDummy4) !=  COMMON_ECADO_TIMEOUT) {i++;}
  DBPRINT1("wr-mil: ECA queue flushed - removed %d pending entries from ECA queue\n", i);
  // init set values
    
  // init get values
  *pSharedGetNEvtsSndHi     = 0x0;
  *pSharedGetNEvtsSndLo     = 0x0;
  *pSharedGetNEvtsRecTHi    = 0x0;
  *pSharedGetNEvtsRecTLo    = 0x0;
  *pSharedGetNEvtsRecDHi    = 0x0;
  *pSharedGetNEvtsRecDLo    = 0x0;
  *pSharedGetNEvtsErr       = 0x0;
  *pSharedGetNEvtsBurst     = 0x0;

  // init set values
  utc_trigger          = *pSharedSetUtcTrigger;
  utc_utc_delay        = *pSharedSetUtcUtcDelay;
  trig_utc_delay       = *pSharedSetTrigUtcDelay;
  utc_offset           = (uint64_t)(*pSharedSetUtcOffsHi) << 32;
  utc_offset          |= (uint64_t)(*pSharedSetUtcOffsLo);
  request_fill_evt     = *pSharedSetReqFillEvt;
  inhibit_fill_events  = RESET_INHIBIT_COUNTER;
  mil_latency          = *pSharedSetLatency;
  pp_printf("latency %d\n", mil_latency);
  mil_domain           = *pSharedSetGid;
  mil_mon              = *pSharedSetMilMon;

  nEvtsSnd             = 0;
  nEvtsRecT            = 0;
  nEvtsRecD            = 0;
  nEvtsErr             = 0;
  nEvtsBurst           = 0;
  nEvtsLate            = 0;
  offsDone             = 0;
  comLatency           = 0;
  maxOffsDone          = 0;
  maxComLatency        = 0;

  // configure MIL receiver for timing events for all 16 virtual accelerators
  // if mil_mon == 2, the FIFO for event data monitoring must be enabled
  if (mil_mon) {
    if (mil_mon == 2) enable_fifo = 1;
    else              enable_fifo = 0;
    if (configMILEvents(enable_fifo) != COMMON_STATUS_OK) {
      pp_printf("config\n");
      DBPRINT1("wr-mil: ERROR - failed to configure MIL piggy for receiving timing events!\n");
    }  // if configMILevents

    //---- arm MIL Piggy 
    enableFilterEvtMil(pMilRec, 0);                                          // enable filter @ MIL piggy
    clearFifoEvtMil(pMilRec, 0);                                             // get rid of junk in FIFO @ MIL piggy
  } // if mil_mon

  return COMMON_STATUS_OK;
} // extern_entryActionOperation


uint32_t extern_exitActionOperation()
{ 
  return COMMON_STATUS_OK;
} // extern_exitActionOperation


// convert 64-bit TAI from WR into an array of five MIL events (EVT_UTC_1/2/3/4/5 events with evtNr 0xE0 - 0xE4)
// arguments:
//   TAI:     a 64-bit WR-TAI value
//   EVT_UTC: points to an array of 5 uint32_t and will be filled 
//            with valid special MIL events:
//                            EVT_UTC_1 = EVT_UTC[0] =  ms[ 9: 2]          , code = 0xE0
//                            EVT_UTC_2 = EVT_UTC[1] =  ms[ 1: 0] s[30:25] , code = 0xE1
//                            EVT_UTC_3 = EVT_UTC[2] =   s[24:16]          , code = 0xE2
//                            EVT_UTC_4 = EVT_UTC[3] =   s[15: 8]          , code = 0xE3
//                            EVT_UTC_5 = EVT_UTC[4] =   s[ 7: 0]          , code = 0xE4
//            where s is a 30 bit number (seconds since 2008) and ms is a 10 bit number
//            containing the  milisecond fraction.
void make_mil_timestamp(uint64_t tai, uint32_t *evt_utc, uint64_t UTC_offset_ms)
{
  uint64_t msNow  = tai / UINT64_C(1000000); // conversion from ns to ms (since 1970)
  //uint64_t ms2008 = UINT64_C(1199142000000); // miliseconds at 01/01/2008  (since 1970)
                                             // the number was caluclated using: date --date='01/01/2008' +%s
  uint64_t mil_timestamp_ms = msNow - UTC_offset_ms;//ms2008;
  uint32_t mil_ms           = mil_timestamp_ms % 1000;
  uint32_t mil_sec          = mil_timestamp_ms / 1000;

  // The following converion code for the UTC timestamps is based on 
  // some sample code that was kindly provided by Peter Kainberger.
  union UTCtime_t
  {
    uint8_t bytes[8];
    struct {
      uint32_t timeMs;
      uint32_t timeS;
    } bit;
  } utc_time = { .bit.timeS  =  mil_sec & 0x3fffffff,
                 .bit.timeMs = (mil_ms & 0x3ff) << 6 };

  evt_utc[0] =  utc_time.bytes[2] *256 + WRMIL_DFLT_EVT_UTC_1;
  evt_utc[1] = (utc_time.bytes[3] | 
                utc_time.bytes[4])*256 + WRMIL_DFLT_EVT_UTC_2;
  evt_utc[2] =  utc_time.bytes[5] *256 + WRMIL_DFLT_EVT_UTC_3;
  evt_utc[3] =  utc_time.bytes[6] *256 + WRMIL_DFLT_EVT_UTC_4;
  evt_utc[4] =  utc_time.bytes[7] *256 + WRMIL_DFLT_EVT_UTC_5;
} // make_mil_timestamp


// prepares evtId and param for sending a MIL telegram via the ECA
void prepMilTelegramEca(uint32_t milTelegram, uint64_t *evtId, uint64_t *param)
{
  // evtId
  *evtId    &= 0xf000ffffffffffff;            // remove all bits for GID
  *evtId    |= (uint64_t)LOC_MIL_SEND << 48;  // set bit for GID to 'local message generating a MIL telegram'

  // param
  *param     = ((uint64_t)mil_domain) << 32;
  *param    |= (uint64_t)milTelegram & 0xffffffff;
} // void prepMilTelegramEca


// conversion of WR timing message from Data Master to MIL telegram; original code from wr-mil-gw (Michael Reese, Peter Kainberger)
// note that the mapping has been changed
uint32_t convert_WReventID_to_milTelegram(uint64_t evtId, uint32_t *milTelegram)
{
  // EventID 
  // |---------------evtIdHi---------------|  |---------------evtIdLo---------------|
  // FFFF GGGG GGGG GGGG EEEE EEEE EEEE FFFF  SSSS SSSS SSSS BBBB BBBB BBBB BBRR RRRR
  // till  June 2024          cccc cccc irrr  ssss ssss vvvv
  //                                               xxxx xxxx
  // since June 2024          cccc cccc                 vvvv                     ssss
  //                              
  // F: FID(4)
  // G: GID(12)
  // E: EVTNO(12) = evtNo
  // F: FLAGS(4)
  // S: SID(12)
  // B: BPID(14)
  // R: Reserved(10)
  // s: status bits
  //    see https://www-acc.gsi.de/wiki/Timing/TimingSystemEventMil#Normal_Event
  //    bit 0: reserved
  //    bit 1: high b/rho, rigid beam
  //    bit 2: no beam
  //    bit 3: high current
  // v: virtAcc = virtual accellerator
  // c: evtCode = MIL relevant part of the evtNo (only 0..255)
  // i: InBeam(1)
  // r: reserved(3)
  // x: other content for special events like (code=200..208 command events)
  //    or (code=200 beam status event) 

  uint32_t evtNo;                         // 12 bit evtNo of WR message
  uint32_t evtCode;
  uint32_t tophalf;                       // the top half bits (15..8) of the mil telegram; the meaning depends on the evtCode.
  uint32_t virtAcc;                       // # of virtual accelerator
  uint32_t statusBits;                    // top bits (12..15) the the mil telegram
  uint32_t pzKennung;                     // for beamtime only (SIS18, ESR)
  uint32_t gid;                           // group ID, WR message

  evtNo      = (evtId >> 36)       & 0x00000fff;       // 12 bits
  evtCode    = evtNo               & 0x000000ff;       // mil telegram is limited to 0..255
  statusBits = evtId               & 0x0000000f;       // lower 4 bits of evtIdReserved
  virtAcc    = (evtId >> 20)       & 0x0000000f;       // lower 4 bits of SID
  gid        = (evtId >> 48)       & 0x00000fff;       // GID

  tophalf    = statusBits << 4;
  tophalf   |= virtAcc;

  // Pulszentralenkennung
  switch (gid) {
    case SIS18_RING: pzKennung =  1; break;
    case ESR_RING  : pzKennung =  2; break;
    case PZU_QR    : pzKennung =  9; break;
    case PZU_QL    : pzKennung = 10; break;
    case PZU_QN    : pzKennung = 11; break;
    case PZU_UN    : pzKennung = 12; break;
    case PZU_UH    : pzKennung = 13; break;
    case PZU_AT    : pzKennung = 14; break;
    case PZU_TK    : pzKennung = 15; break;
    default :        pzKennung =  0; break;
  } // switch gid
    
  // commands: top half of the MIL bits (15..8) are extracted from the status bits of EventID
  // however, there are only 4 status bits, but here we need 8 bits
  // thus we can not do s.th. useful and just write a bogus number of 0x0
  if ((evtCode >= 200) && (evtCode <= 208)) {
    tophalf = 0x0;                     
  } // if evtCode
  else if (evtCode == 255) { // command event: top half of the MIL bits (15..8) are ppppvvvv, p is pzKennung
    tophalf = ( pzKennung  << 4 ) | virtAcc;
  } // evtCode 255
  else {                                // all other events: top half of MIL bits (15..8) are ssssvvvv
    tophalf = ( statusBits << 4 ) | virtAcc;
  } // all other event number

  *milTelegram = (tophalf << 8)   | evtCode; 
                                           
  // For MIL events, the upper 4 bits of evtNo are zero
  return (evtNo & 0x00000f00) == 0; 
} // convert_WReventID_to_milTelegram


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
  uint32_t recEvtNo;                                          // event number received
  uint32_t recSid;                                            // SID received
  uint32_t recBpid;                                           // BPID received
  uint32_t recMilEvtData;                                     // event data received from MIL
  uint32_t recMilEvtCode;                                     // event code received from MIL
  uint32_t recMilVAcc;                                        // event virt acc received from MIL
  uint32_t recMilEvts[] = {0xffff};                           // list of MIL events we like to liste to (here: dummy)
  uint64_t sendDeadline;                                      // deadline to send
  uint64_t sendEvtId;                                         // evtid to send
  uint64_t sendParam;                                         // parameter to send
  uint32_t sendTEF;                                           // TEF to send
  static uint64_t previous_time = 0;                          // protect for nonsense high frequency bursts
  uint32_t milTelegram;                                       // telegram to be sent
  uint32_t evt_utc[WRMIL_N_UTC_EVTS];                         // MIL telegrams for distributing UTC

  int      i;
  uint64_t one_us_ns = 1000;
  uint64_t sysTime;
  uint32_t tmp32;
  
  status    = actStatus;

  ecaAction = fwlib_wait4ECAEvent(COMMON_ECATIMEOUT * 1000, &recDeadline, &recEvtId, &recParam, &recTEF, &flagIsLate, &flagIsEarly, &flagIsConflict, &flagIsDelayed);

  switch (ecaAction) {
    // received WR timing message from Data Master that shall be sent as a MIL telegram
    case WRMIL_ECADO_MIL_EVT:
      comLatency   = (int32_t)(getSysTime() - recDeadline);
      recGid       = (uint32_t)((recEvtId >> 48) & 0x00000fff);
      recEvtNo     = (uint32_t)((recEvtId >> 36) & 0x00000fff);
      recSid       = (uint32_t)((recEvtId >> 20) & 0x00000fff);

      if (recGid != mil_domain) return WRMIL_STATUS_BADSETTING;
      if (recSid  > 15)         return COMMON_STATUS_OUTOFRANGE;

      // chk, hack due to different event formats
      // the following lines can be removed once the wr-unipz has been changed to the new format
      // recEvtId &= 0xfffffffffffffff0;
      // recEvtId |= ((recParam >> 32) & 0xf);
      
      convert_WReventID_to_milTelegram(recEvtId, &milTelegram);

      // build timing message and inject into ECA

      // deadline
      sendDeadline  = recDeadline + WRMIL_PRETRIGGER_DM + mil_latency - WRMIL_MILSEND_LATENCY;
       // protect from nonsense hi-frequency bursts
      if (sendDeadline < previous_time + WRMIL_MILSEND_MININTERVAL) {
        sendDeadline = previous_time + WRMIL_MILSEND_MININTERVAL;
        nEvtsBurst++;
      } // if sendDeadline
      previous_time = sendDeadline;

      // evtID + param
      sendEvtId     = recEvtId;
      prepMilTelegramEca(milTelegram, &sendEvtId, &sendParam);

      // clear MIL FIFO and write to ECA
      if (mil_mon) clearFifoEvtMil(pMilRec, 0);
      fwlib_ecaWriteTM(sendDeadline, sendEvtId, sendParam, 0x0, 1);

      nEvtsSnd++;
      sysTime = getSysTime();
      // note: we pretrigger 500us the WR-messages to have time to deliver the message
      // however, some service events will come with an 'official' pretrigger of only 250us;
      // thus, the ECA will report the WR-message as late. For us however, we only need write the
      // MIL-message to the ECA 'in time'. Here, 'in time' means that we write the MIL-message
      // to the ECA sooner than its deadline. Usually this will be the case even if we receive
      // the WR-message late from the ECA. Thus the late info depends on if we manage to
      // write the MIL message in time.
      // note: we add 20us in the comparison taking into account the time it takes to write
      // the message to the ECA and the time it takes the ECA to process that message
      if (sysTime + 20000 > sendDeadline) {
        nEvtsLate++;
        flagIsLate    = 1;
      } // if systime
      else flagIsLate = 0;

      offsDone = sysTime - recDeadline;

      // handle UTC events; here the UTC time (- offset) is distributed as a series of MIL telegrams
      if (recEvtNo == utc_trigger) {
        // send EVT_UTC_1/2/3/4/5 telegrams
        make_mil_timestamp(sendDeadline, evt_utc, utc_offset);
        sendDeadline += trig_utc_delay * one_us_ns;
        for (i=0; i<WRMIL_N_UTC_EVTS; i++){
          sendDeadline += utc_utc_delay * one_us_ns;
          prepMilTelegramEca(evt_utc[i], &sendEvtId, &sendParam);
          // nicer evtId for debugging only
          sendEvtId &= 0xffff000fffffffff;
          sendEvtId |= (uint64_t)(0x0e0 + i) << 36;
          fwlib_ecaWriteTM(sendDeadline, sendEvtId, sendParam, 0x0, 1);
          nEvtsSnd++;
        } // for i
      } // if utc_trigger

      // reset inhibit counter for fill events
      inhibit_fill_events = RESET_INHIBIT_COUNTER;

      break;

    // received timing message from TLU; this indicates a received MIL telegram on the MIL piggy
    case WRMIL_ECADO_MIL_TLU:

      // in case of data monitoring, read message from MIL piggy FIFO and re-send it locally via the ECA
      if (mil_mon==2) {
        if (fwlib_wait4MILEvent(50, &recMilEvtData, &recMilEvtCode, &recMilVAcc, recMilEvts, 0) == COMMON_STATUS_OK) {

          // deadline
          sendDeadline  = recDeadline - WRMIL_POSTTRIGGER_TLU; // time stamp when MIL telegram was decoded at MIL piggy
          sendDeadline += 1000000;                             // advance to 1ms into the future to avoid late messages
          // evtID
          sendEvtId     = fwlib_buildEvtidV1(LOC_MIL_REC, recMilEvtCode, 0x0, recMilVAcc, 0x0, recMilEvtData);
          // param
          sendParam     = ((uint64_t)mil_domain) << 32;

          fwlib_ecaWriteTM(sendDeadline, sendEvtId, sendParam, 0x0, 1);
          nEvtsRecD++;
        } // if wait4Milevent
      } // if mil_mon

      // read number of received 'broken' MIL telegrams
      readEventErrCntMil(pMilRec, 0, &nEvtsErr);

      nEvtsRecT++;
      
      break;

    default :                                                         // flush ECA Queue
      flagIsLate = 0;                                                 // ignore late events
  } // switch ecaAction

  // send fill events if inactive for a long time
  if (request_fill_evt) {
    // decrease inhibit counter
    inhibit_fill_events--;

    // check for fill event
    if (!inhibit_fill_events) {
      // the last mil event is so far in the past that the inhibit counter is zero
      // so lets send the fill event and reset inhibit counter
      // deadline
      sendDeadline  = getSysTime();
      sendDeadline += COMMON_AHEADT;
      // evtID
      sendEvtId     = fwlib_buildEvtidV1(mil_domain, WRMIL_DFLT_MIL_EVT_FILL, 0x0, 0x0, 0x0, 0x0); // chk
      convert_WReventID_to_milTelegram(sendEvtId, &milTelegram);                                   // --> MIL format
      prepMilTelegramEca(milTelegram, &sendEvtId, &sendParam);                                     // --> EvtId for internal use
      fwlib_ecaWriteTM(sendDeadline, sendEvtId, sendParam, 0x0, 0);                                // --> ECA
      nEvtsSnd++;
      inhibit_fill_events = RESET_INHIBIT_COUNTER;
    } // if not inhibit fill events
  } // if request_fill_evt
    
  // check for late event
  if ((status == COMMON_STATUS_OK) && flagIsLate) {
    status = WRMIL_STATUS_LATEMESSAGE;
    nEvtsLate++;
  } // if flagIslate
  
  // check WR sync state
  if (fwlib_wrCheckSyncState() == COMMON_STATUS_WRBADSYNC) return COMMON_STATUS_WRBADSYNC;
  else                                                     return status;
} // doActionOperation


int main(void) {
  uint64_t tActMessage;                         // time of actual message
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
  nEvtsSnd       = 0;
  nEvtsRecT      = 0;
  nEvtsRecD      = 0;

  init();                                                                     // initialize stuff for lm32
  initSharedMem(&reqState, &sharedSize);                                      // initialize shared memory
  fwlib_init((uint32_t *)_startshared, cpuRamExternal, SHARED_OFFS, sharedSize, "wr-mil", WRMIL_FW_VERSION); // init common stuff
  fwlib_clearDiag();                                                          // clear common diagnostics data
  
  while (1) {
    check_stack_fwid(buildID);                                                // check stack status
    fwlib_cmdHandler(&reqState, &dummy1);                                     // check for commands and possibly request state changes
    status = COMMON_STATUS_OK;                                                // reset status for each iteration

    // state machine
    status = fwlib_changeState(&actState, &reqState, status);                 // handle requested state changes
    switch(actState) {                                                        // state specific do actions
      case COMMON_STATE_OPREADY :
        status = doActionOperation(&tActMessage, status);
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
    fwlib_publishTransferStatus(0, 0, 0, nEvtsLate, maxOffsDone, maxComLatency);
    
    *pSharedGetNEvtsSndHi  = (uint32_t)(nEvtsSnd >> 32);
    *pSharedGetNEvtsSndLo  = (uint32_t)(nEvtsSnd & 0xffffffff);
    *pSharedGetNEvtsRecTHi = (uint32_t)(nEvtsRecT >> 32);
    *pSharedGetNEvtsRecTLo = (uint32_t)(nEvtsRecT & 0xffffffff);
    *pSharedGetNEvtsRecDHi = (uint32_t)(nEvtsRecD >> 32);
    *pSharedGetNEvtsRecDLo = (uint32_t)(nEvtsRecD & 0xffffffff);
    *pSharedGetNEvtsErr    = nEvtsErr;
    *pSharedGetNEvtsBurst  = nEvtsBurst;
  } // while

  return(1); // this should never happen ...
} // main

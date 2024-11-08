/********************************************************************************************
 *  uni-chop.c
 *
 *  created : 2024
 *  author  : Dietrich Beck, Tobias Habermann GSI-Darmstadt
 *  version : 07-Nov-2024
 *
 *  firmware required for UNILAC chopper control
 *  
 *  This firmware just takes care of writing the time critical MIL telegrams to the UNILAC
 *  'Choppersteuerung'. The data to be written is received via timing messages
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
#define UNICHOP_FW_VERSION      0x000011  // make this consistent with makefile

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
#include <uni-chop.h>                                                   // specific defs for uni-chop
#include <unichop_shared_mmap.h>                                        // autogenerated upon building firmware

// stuff required for environment
extern uint32_t* _startshared[];
unsigned int     cpuId, cpuQty;
#define  SHARED  __attribute__((section(".shared")))
uint64_t SHARED  dummy = 0;

// global variables 
volatile uint32_t *pShared;                // pointer to begin of shared memory region
volatile uint32_t *pSharedSetMilDev;       // pointer to a "user defined" u32 register; here: MIL device for sending MIL messages; 0: MIL Piggy; 1..: SIO in slot 1..
volatile uint32_t *pSharedGetNMilSndHi;    // pointer to a "user defined" u32 register; here: number of MIL writes, high word
volatile uint32_t *pSharedGetNMilSndLo;    // pointer to a "user defined" u32 register; here: number of MIL writes, low word
volatile uint32_t *pSharedGetNMilSndErr;   // pointer to a "user defined" u32 register; here: number of failed MIL writes
volatile uint32_t *pSharedGetNEvtsRecHi;   // pointer to a "user defined" u32 register; here: number of timing messages received, high word
volatile uint32_t *pSharedGetNEvtsRecLo;   // pointer to a "user defined" u32 register; here: number of timing messages received, low word

uint32_t *cpuRamExternal;               // external address (seen from host bridge) of this CPU's RAM
volatile uint32_t *pMilSend;            // address of MIL device sending timing messages, usually this will be a SIO
uint16_t slotSio;                       // slot of SIO in SCU crate

uint64_t statusArray;                   // all status infos are ORed bit-wise into statusArray, statusArray is then published
uint64_t nMilSnd;                       // # of MIL writes
uint32_t nMilSndErr;                    // # of failed MIL writes                      
uint64_t nEvtsRec;                      // # of received timing messages
uint32_t nEvtsLate;
uint32_t offsDone;                      // offset deadline WR message to time when we are done [ns]
int32_t  comLatency;                    // latency for messages received via ECA

int32_t  maxComLatency;
uint32_t maxOffsDone;

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
  pSharedSetMilDev           = (uint32_t *)(pShared + (UNICHOP_SHARED_SET_MIL_DEV           >> 2));
  pSharedGetNMilSndHi        = (uint32_t *)(pShared + (UNICHOP_SHARED_GET_N_MIL_SND_HI      >> 2));
  pSharedGetNMilSndLo        = (uint32_t *)(pShared + (UNICHOP_SHARED_GET_N_MIL_SND_LO      >> 2));
  pSharedGetNMilSndErr       = (uint32_t *)(pShared + (UNICHOP_SHARED_GET_N_MIL_SND_ERR     >> 2));
  pSharedGetNEvtsRecHi       = (uint32_t *)(pShared + (UNICHOP_SHARED_GET_N_EVTS_REC_HI     >> 2));
  pSharedGetNEvtsRecLo       = (uint32_t *)(pShared + (UNICHOP_SHARED_GET_N_EVTS_REC_LO     >> 2));

  // find address of CPU from external perspective
  idx = 0;
  find_device_multi(&found_clu, &idx, 1, GSI, LM32_CB_CLUSTER);
  if (idx == 0) {
    *reqState = COMMON_STATE_FATAL;
    DBPRINT1("uni-chop: fatal error - did not find LM32-CB-CLUSTER!\n");
  } // if idx
  idx = 0;
  find_device_multi_in_subtree(&found_clu, &found_sdb[0], &idx, c_Max_Rams, GSI, LM32_RAM_USER);
  if (idx == 0) {
    *reqState = COMMON_STATE_FATAL;
    DBPRINT1("uni-chop: fatal error - did not find THIS CPU!\n");
  } // if idx
  else cpuRamExternal = (uint32_t *)(getSdbAdr(&found_sdb[cpuId]) & 0x7FFFFFFF); // CPU sees the 'world' under 0x8..., remove that bit to get host bridge perspective

  DBPRINT2("uni-chop: CPU RAM external 0x%8x, shared offset 0x%08x\n", cpuRamExternal, SHARED_OFFS);
  DBPRINT2("uni-chop: fw common shared begin   0x%08x\n", pShared);
  DBPRINT2("uni-chop: fw common shared end     0x%08x\n", pShared + (COMMON_SHARED_END >> 2));

  // clear shared mem
  i = 0;
  pSharedTemp        = (uint32_t *)(pShared + (COMMON_SHARED_END >> 2 ) + 1);
  DBPRINT2("uni-chop: fw specific shared begin 0x%08x\n", pSharedTemp);
  while (pSharedTemp < (uint32_t *)(pShared + (UNICHOP_SHARED_END >> 2 ))) {
    *pSharedTemp = 0x0;
    pSharedTemp++;
    i++;
  } // while pSharedTemp
  DBPRINT2("uni-chop: fw specific shared end   0x%08x\n", pSharedTemp);

  *sharedSize        = (uint32_t)(pSharedTemp - pShared) << 2;

  // basic info to wr console
  DBPRINT1("\n");
  DBPRINT1("uni-chop: initSharedMem, shared size [bytes]: %d\n", *sharedSize);
  DBPRINT1("\n");
} // initSharedMem


// send MIL diag message to ECAaction
void sendMilDiag(int writeFlag, uint16_t status, uint16_t slotSio, uint16_t ifbAddr, uint16_t modAddr, uint16_t modReg, uint16_t data)
{
  uint64_t sendEvtId;
  uint32_t sendEvtNo;
  uint64_t sendParam;
  uint64_t sendDeadline;

  if (writeFlag) sendEvtNo = UNICHOP_ECADO_MIL_SWRITE;
  else           sendEvtNo = UNICHOP_ECADO_MIL_SREAD;

  sendEvtId    = fwlib_buildEvtidV1(GID_LOCAL_ECPU_FROM, sendEvtNo, 0x0, 0x0, 0x0, 0x0);
  sendParam    = (uint64_t)(status  & 0xffff) << 48;
  sendParam   |= (uint64_t)(slotSio & 0xff  ) << 40;
  sendParam   |= (uint64_t)(ifbAddr & 0xff  ) << 32;
  sendParam   |= (uint64_t)(modAddr & 0xff  ) << 24;
  sendParam   |= (uint64_t)(modReg  & 0xff  ) << 16;
  sendParam   |= (uint64_t)(data    & 0xffff);
  sendDeadline = getSysTime() + COMMON_AHEADT;

  fwlib_ecaWriteTM(sendDeadline, sendEvtId, sendParam, 0x0, 0x0);
} // sendMilDiag


// write to modules connected via MIL
int32_t writeToModuleMil(uint16_t ifbAddr, uint16_t modAddr, uint16_t modReg, uint16_t data)
{
  uint16_t wData     = 0x0;     // data to write
  int16_t  busStatus = 0;       // status of bus operation
  int32_t  status;

  if (modAddr) {
    // select module
    wData     = (modAddr << 8) | modReg;
    
    busStatus = writeDevMil(pMilSend, slotSio, ifbAddr, IFB_FC_ADDR_BUS_W, wData);
    
    if (busStatus == MIL_STAT_OK) {
      // write data word
      wData     = data;
      busStatus = writeDevMil(pMilSend, slotSio, ifbAddr, IFB_FC_DATA_BUS_W, wData);
    } // if ok
  } // if modAddr
  else {
    // module address '0': no module, direct write to MIL slave
    busStatus = writeDevMil(pMilSend, slotSio, ifbAddr, modReg, data);
  } // else (no module)
    
  sendMilDiag(1, busStatus, slotSio, ifbAddr, modAddr, modReg, data);
  
  if (busStatus == MIL_STAT_OK) {
    status = COMMON_STATUS_OK;
  } // if ok
  else {
    DBPRINT1("uni-chop: writeToModuleMil failed, MIL error code %d\n", busStatus);
    status = UNICHOP_STATUS_MIL;
  } // else ok

    
  return status;
} // writeToModuleMil


// read from modules connected via MIL
int32_t readFromModuleMil(uint16_t ifbAddr, uint16_t modAddr, uint16_t modReg, uint16_t *data) 
{
  uint16_t wData      = 0x0;    // data to write
  uint16_t rData      = 0x0;    // data to read
  int16_t  busStatus  = 0;      // status of bus operation
  int32_t  status;

  if (modAddr) {
    // select module
    wData     = (modAddr << 8) | modReg;
    
    busStatus = writeDevMil(pMilSend, slotSio, ifbAddr, IFB_FC_ADDR_BUS_W, wData);
    
    if (busStatus == MIL_STAT_OK) {
      // read data
      busStatus = readDevMil(pMilSend, slotSio, ifbAddr, IFB_FC_DATA_BUS_R, &rData);
    } // if ok
  } // if modAddr
  else {
    // module address '0': no module, direct read from MIL slave
    busStatus = readDevMil(pMilSend, slotSio, ifbAddr, modReg, &rData);
  } // else (no module)
  
  sendMilDiag(0, busStatus, slotSio, ifbAddr, modAddr, modReg, rData);
  
  if (busStatus == MIL_STAT_OK) {
    *data     = rData;
    status    = COMMON_STATUS_OK;
  } // if ok
  else {
    DBPRINT1("uni-chop: readFromModuleMil failed, MIL error code %d\n", busStatus);
    status    = UNICHOP_STATUS_MIL;
  } // else ok

  
  return status;
} // readFromModuleMil 


// clear project specific diagnostics
void extern_clearDiag()
{
  statusArray   = 0x0;  
  nMilSnd       = 0x0;
  nMilSndErr    = 0x0;
  nEvtsRec      = 0x0;
  nEvtsLate     = 0x0;
  offsDone      = 0x0;
  comLatency    = 0x0;
  maxComLatency = 0x0;
  maxOffsDone   = 0x0;
} // extern_clearDiag 


// entry action 'configured' state
uint32_t extern_entryActionConfigured()
{
  uint32_t status = COMMON_STATUS_OK;

  // get and publish NIC data
  fwlib_publishNICData(); 

  // get address of MIL device sending MIL telegrams; 0 is MIL piggy
  if (*pSharedSetMilDev == 0){
    pMilSend = fwlib_getMilPiggy();
    slotSio  = 0;
    if (!pMilSend) {
      DBPRINT1("uni-chop: ERROR - can't find MIL device\n");
      return COMMON_STATUS_OUTOFRANGE;
    } // if !pMilSend
  } // if SetMilDev
  else {
    // SCU slaves have offsets 0x20000, 0x40000... for slots 1, 2 ...
    pMilSend = fwlib_getSbMaster();
    slotSio  = *pSharedSetMilDev;
    if (!pMilSend) {
      DBPRINT1("uni-chop: ERROR - can't find MIL device\n");
      return COMMON_STATUS_OUTOFRANGE;
    } // if !pMilSend
  } // else SetMilDev

  // reset MIL sender and wait
  status = resetDevMil(pMilSend, slotSio);
  if (status != MIL_STAT_OK) {
    DBPRINT1("uni-chop: ERROR - can't reset MIL device\n");
    return UNICHOP_STATUS_MIL;
  }  // if reset

  // check if connection to chopper unit is ok
  status = echoTestDevMil(pMilSend, slotSio, IFB_ADDR_CU, 0x0651);
  if (status != MIL_STAT_OK) {
    DBPRINT1("uni-chop: ERROR - modulbus SIS IFK not available at (ext) base address 0x%08x! Error code is %u\n", (unsigned int)((uint32_t)pMilSend & 0x7FFFFFFF), (unsigned int)status);
    return UNICHOP_STATUS_MIL;
  } // if status

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
  DBPRINT1("uni-chop: ECA queue flushed - removed %d pending entries from ECA queue\n", i);
    
  // init get values
  *pSharedGetNMilSndHi      = 0x0;
  *pSharedGetNMilSndLo      = 0x0;
  *pSharedGetNMilSndErr     = 0x0;
  *pSharedGetNEvtsRecHi     = 0x0;
  *pSharedGetNEvtsRecLo     = 0x0;

  nMilSnd                   = 0;
  nMilSndErr                = 0;
  nEvtsRec                  = 0;
  nEvtsLate                 = 0;
  offsDone                  = 0;
  comLatency                = 0;
  maxComLatency             = 0;
  maxOffsDone               = 0;

  uint16_t data;

  readFromModuleMil(IFB_ADDR_CU, MOD_LOGIC1_ADDR, MOD_LOGIC1_REG_STATUSGLOBAL, &data);
  pp_printf("module version 0x%x\n", data);

  return COMMON_STATUS_OK;
} // extern_entryActionOperation


uint32_t extern_exitActionOperation()
{ 
  return COMMON_STATUS_OK;
} // extern_exitActionOperation


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
  uint64_t recEvtId;                                          // evt ID received
  uint64_t recParam;                                          // param received
  uint32_t recTEF;                                            // TEF received
  uint32_t recGid;                                            // GID received
  uint32_t recEvtNo;                                          // event number received
  uint32_t recSid;                                            // SID received
  uint32_t recBpid;                                           // BPID received
  uint32_t recAttribute;                                      // MIL event data in the lowest 4 bits, https://www-acc.gsi.de/wiki/Timing/TimingSystemEventMil#Mapping_between_Settings_and_MIL_Event_Telegrams
  uint64_t sendDeadline;                                      // deadline to send
  uint64_t sendEvtId;                                         // evtid to send
  uint32_t sendEvtNo;                                         // evtno to send
  uint32_t sendBpid;                                          // bpid to send
  uint64_t sendParam;                                         // parameter to send  

  uint16_t strahlweg_mask;                                    // strahlweg mask
  uint16_t strahlweg_reg;                                     // strahlweg register
  uint16_t anforder_mask;                                     // anforder mask

  uint16_t milData;                                           // MIL data
  uint16_t milIfb;                                            // MIL device bus slave address
  uint16_t milModAddr;                                        // address of module in MIL slave crate
  uint16_t milModReg;                                         // register of module in MIL slave crate

  uint16_t tChopRiseAct;                                      // time of rising edge of chopper readback pulse
  uint16_t tChopFallAct;                                      // time of falling edge of chopper readback pulse
  uint16_t tChopFallCtrl;                                     // time of falling edge of chopper control pulse
  uint16_t lenChopAct;                                        // length of chopper readback pulse
  uint16_t regChopRiseAct;                                    // register of rising edge of chopper readback pulse
  uint16_t regChopFallAct;                                    // register of falling edge of chopper readback pulse
  uint16_t regChopFallCtrl;                                   // register of falling edge of chopper control pulse
  uint32_t rpgGatelen;                                        // measured length of RPG gate
  uint16_t rpgGatelenHi;                                      // measured length of RPG gate, hi word
  uint16_t rpgGatelenLo;                                      // measured length of RPG gate, lo word
  uint16_t rpgAddr;                                           // address for selected RPG

  static uint32_t flagInterlockHLI = 0;                       // chopper control detected slow interlock, HLI
  static uint32_t flagInterlockHSI = 0;                       // chopper control detected slow interlock, HSI
  static uint32_t flagBlockHLI     = 0;                       // chopper control executes with 'no beam', HLI
  static uint32_t flagBlockHSI     = 0;                       // chopper control executes with 'no beam', HSI
  static uint32_t flagCCIRec       = 0;                       // received strahlweg register and mask from chopper control interface
  static uint32_t flagCCILate      = 0;                       // message from chopper control interface was late
  
  int      i;
  uint64_t one_us_ns = 1000;
  uint64_t sysTime;
  uint32_t tmp32;
  
  status          = actStatus;
  sendEvtNo       = 0x0;
  regChopRiseAct  = 0x0;
  regChopFallAct  = 0x0;
  regChopFallCtrl = 0x0;

  ecaAction = fwlib_wait4ECAEvent(COMMON_ECATIMEOUT * 1000, &recDeadline, &recEvtId, &recParam, &recTEF, &flagIsLate, &flagIsEarly, &flagIsConflict, &flagIsDelayed);

  switch (ecaAction) {
    // received timing message containing strahlweg data
    case UNICHOP_ECADO_STRAHLWEG_WRITE:
      comLatency   = (int32_t)(getSysTime() - recDeadline);
      recGid       = (uint32_t)((recEvtId >> 48) & 0x00000fff);
      recEvtNo     = (uint32_t)((recEvtId >> 36) & 0x00000fff);
      recSid       = (uint32_t)((recEvtId >> 20) & 0x00000fff);
      if (recGid != GID_LOCAL_ECPU_TO) return COMMON_STATUS_BADSETTING;
      if (recSid  > 15)                return COMMON_STATUS_OUTOFRANGE;

      flagCCIRec   = 1;
      flagCCILate  = flagIsLate;

      if (!flagIsLate) {
        strahlweg_reg    =  recParam        & 0xffff;
        strahlweg_mask   = (recParam >> 16) & 0xffff;
        anforder_mask    = (recParam >> 32) & 0xffff;

        flagBlockHLI     = (strahlweg_reg >> 7) & 0x1;
        flagBlockHSI     = (strahlweg_reg >> 6) & 0x1;
        flagInterlockHLI = (strahlweg_reg >> 5) & 0x1;
        flagInterlockHSI = (strahlweg_reg >> 4) & 0x1;

        // write strahlweg register
        status = writeToModuleMil(IFB_ADDR_CU, MOD_LOGIC1_ADDR, MOD_LOGIC1_REG_STRAHLWEG_REG, strahlweg_reg);
        if (status == COMMON_STATUS_OK)   nMilSnd++;
        else                              nMilSndErr++;

        // write strahlweg mask
        if (status == COMMON_STATUS_OK) {
          status = writeToModuleMil(IFB_ADDR_CU, MOD_LOGIC1_ADDR, MOD_LOGIC1_REG_STRAHLWEG_MASK, strahlweg_mask);
          if (status == COMMON_STATUS_OK) nMilSnd++;
          else                            nMilSndErr++;
        } // if status ok

        // write anforder mask
        if (status == COMMON_STATUS_OK) {
          status = writeToModuleMil(IFB_ADDR_CU, MOD_LOGIC2_ADDR, MOD_LOGIC2_REG_ANFORDER_MASK, anforder_mask);
          if (status == COMMON_STATUS_OK) nMilSnd++;
          else                            nMilSndErr++;
        } // if status ok

        // enable RPGs for HLI and HSI
        if (status == COMMON_STATUS_OK) {
          writeToModuleMil(IFB_ADDR_CU, MOD_RPG_HLI_ADDR, MOD_RPG_XXX_ENABLE_REG, MOD_RPG_XXX_ENABLE_TRUE);
          writeToModuleMil(IFB_ADDR_CU, MOD_RPG_HSI_ADDR, MOD_RPG_XXX_ENABLE_REG, MOD_RPG_XXX_ENABLE_TRUE);
        } // if status ok
        
      } // if not late
      else {
        // we set all flags to block/interlock as the watchdog of the chopper control should inhibit the chopper
        /* chk, I think its a stupid idea to set everything to '1' 
        flagBlockHLI     = 1;
        flagBlockHSI     = 1;
        flagInterlockHLI = 1;
        flagInterlockHSI = 1; */
      } // else: is late


      offsDone = getSysTime() - recDeadline;
      nEvtsRec++;

      break;

    case UNICHOP_ECADO_STRAHLWEG_READ:
      comLatency   = (int32_t)(getSysTime() - recDeadline);
      recGid       = (uint32_t)((recEvtId >> 48) & 0x00000fff);
      recEvtNo     = (uint32_t)((recEvtId >> 36) & 0x00000fff);
      recSid       = (uint32_t)((recEvtId >> 20) & 0x00000fff);
      if (recGid != GID_LOCAL_ECPU_TO) return COMMON_STATUS_BADSETTING;
      if (recSid  > 15)                return COMMON_STATUS_OUTOFRANGE;

      if (!flagIsLate) {
        // read strahlweg register
        status = readFromModuleMil(IFB_ADDR_CU, MOD_LOGIC1_ADDR, MOD_LOGIC1_REG_STRAHLWEG_REG, &strahlweg_reg);
        if (status == COMMON_STATUS_OK)   nMilSnd++;
        else                              nMilSndErr++;

        // read strahlweg mask
        if (status == COMMON_STATUS_OK) {
          status = readFromModuleMil(IFB_ADDR_CU, MOD_LOGIC1_ADDR, MOD_LOGIC1_REG_STRAHLWEG_MASK, &strahlweg_mask);
          if (status == COMMON_STATUS_OK) nMilSnd++;
          else                            nMilSndErr++;
        } // if status ok

        // read anforder mask
        if (status == COMMON_STATUS_OK) {
          status = readFromModuleMil(IFB_ADDR_CU, MOD_LOGIC2_ADDR, MOD_LOGIC2_REG_ANFORDER_MASK, &anforder_mask);
          if (status == COMMON_STATUS_OK) nMilSnd++;
          else                            nMilSndErr++;
        } // if status ok

        // write result to ECA
        sendEvtId    = fwlib_buildEvtidV1(GID_LOCAL_ECPU_FROM, UNICHOP_ECADO_STRAHLWEG_READ, 0x0, 0x0, 0x0, 0x0);
        sendParam    = (uint64_t)(strahlweg_reg  & 0xffff);
        sendParam   |= (uint64_t)(strahlweg_mask & 0xffff) << 16;
        sendParam   |= (uint64_t)(anforder_mask  & 0xffff) << 24;
        sendDeadline = getSysTime() + COMMON_AHEADT;
        fwlib_ecaWriteTM(sendDeadline, sendEvtId, sendParam, 0x0, 0x0);
      } // if not late

      offsDone = getSysTime() - recDeadline;
      nEvtsRec++;

      break;

    // received timing message requesting a standard MIL write
    case UNICHOP_ECADO_MIL_SWRITE:
      comLatency   = (int32_t)(getSysTime() - recDeadline);
      recGid       = (uint32_t)((recEvtId >> 48) & 0x00000fff);
      recEvtNo     = (uint32_t)((recEvtId >> 36) & 0x00000fff);
      recSid       = (uint32_t)((recEvtId >> 20) & 0x00000fff);
      if (recGid != GID_LOCAL_ECPU_TO) return COMMON_STATUS_BADSETTING;
      if (recSid  > 15)                return COMMON_STATUS_OUTOFRANGE;
      
      if (!flagIsLate) {
        milData     =  recParam        & 0xffff;
        milModReg   = (recParam >> 16) & 0xff;
        milModAddr  = (recParam >> 24) & 0xff; 
        milIfb      = (recParam >> 32) & 0xff;

        // write to HW
        status = writeToModuleMil(milIfb, milModAddr, milModReg, milData);
        if (status == COMMON_STATUS_OK)   nMilSnd++;
        else                              nMilSndErr++;
      } // if not late
      
      offsDone = getSysTime() - recDeadline;
      nEvtsRec++;
      
      break;

    // received a timing message requesting a standard MIL read
    case UNICHOP_ECADO_MIL_SREAD:
      comLatency   = (int32_t)(getSysTime() - recDeadline);
      recGid       = (uint32_t)((recEvtId >> 48) & 0x00000fff);
      recEvtNo     = (uint32_t)((recEvtId >> 36) & 0x00000fff);
      recSid       = (uint32_t)((recEvtId >> 20) & 0x00000fff);
      if (recGid != GID_LOCAL_ECPU_TO) return COMMON_STATUS_BADSETTING;
      if (recSid  > 15)                return COMMON_STATUS_OUTOFRANGE;

      if (!flagIsLate) {
        // read strahlweg register
        milModReg   = (recParam >> 16) & 0xff; 
        milModAddr  = (recParam >> 24) & 0xff;
        milIfb      = (recParam >> 32) & 0xff;

        status = readFromModuleMil(milIfb, milModAddr, milModReg, &milData);
        // result is obtained via MIL diagnostics (gid: 0xff1, evtno: 0xfa9);

        if (status == COMMON_STATUS_OK)   nMilSnd++;
        else                              nMilSndErr++;
      } // if not late

      offsDone = getSysTime() - recDeadline;
      nEvtsRec++;

      break;

     // received a timing message marking the end of an ion source pulse
    case UNICHOP_ECADO_IQSTOP :
      comLatency   = (int32_t)(getSysTime() - recDeadline);
      recGid       = (uint32_t)((recEvtId >> 48) & 0x00000fff);
      recEvtNo     = (uint32_t)((recEvtId >> 36) & 0x00000fff);
      recSid       = (uint32_t)((recEvtId >> 20) & 0x00000fff);

      if (recSid  > 15)       return COMMON_STATUS_OUTOFRANGE;
      
      if (!flagIsLate) {
        if (recGid == GID_PZU_QR) {milModAddr = MOD_RPG_IQR_ADDR; sendEvtNo = UNICHOP_ECADO_QRSTOP;}
        if (recGid == GID_PZU_QL) {milModAddr = MOD_RPG_IQL_ADDR; sendEvtNo = UNICHOP_ECADO_QLSTOP;}

        status                                 = readFromModuleMil(IFB_ADDR_CU, milModAddr, MOD_RPG_XXX_GATELENHI_REG, &rpgGatelenHi);
        if (status == COMMON_STATUS_OK) status = readFromModuleMil(IFB_ADDR_CU, milModAddr, MOD_RPG_XXX_GATELENLO_REG, &rpgGatelenLo);
        
        rpgGatelen  = (uint32_t)(rpgGatelenHi & 0x1ff) << 16;
        rpgGatelen |= (uint32_t)rpgGatelenLo;
        if ((rpgGatelenHi >> 8) && 0x3) {status = COMMON_STATUS_OUTOFRANGE; rpgGatelen = 0xffffffff;}
        if  (rpgGatelenHi == 0)         {                                   rpgGatelen = 0x7fffffff;}

        // write result to ECA
        sendEvtId    = fwlib_buildEvtidV1(GID_LOCAL_ECPU_FROM, sendEvtNo, 0x0, recSid, 0x0, 0x0);
        sendParam    = (uint64_t)(rpgGatelen & 0xffffffff);
        sendDeadline = getSysTime() + COMMON_AHEADT;
        fwlib_ecaWriteTM(sendDeadline, sendEvtId, sendParam, 0x0, 0x0);
      } // if flagIsLate

      offsDone = getSysTime() - recDeadline;
      // nEvtsRec++; don't increase counter - this is just monitoring

      break;

    // received a timing message marking the end of a chopper pulse at HLI or HSI
    case UNICHOP_ECADO_HLISTOP :
      sendEvtNo         = UNICHOP_ECADO_HLISTOP;
      regChopRiseAct    = MOD_LOGIC1_REG_HLI_ACT_POSEDGE_RD;
      regChopFallAct    = MOD_LOGIC1_REG_HLI_ACT_NEGEDGE_RD;
      regChopFallCtrl   = MOD_LOGIC1_REG_HLI_CTRL_NEGEDGE_RD;
      sendBpid          = (flagBlockHLI << 2) | (flagInterlockHLI << 3);
      rpgAddr           = MOD_RPG_HLI_ADDR;

    case UNICHOP_ECADO_HSISTOP :                                    // this is an OR, no 'break' on purpose
      if (!sendEvtNo) {
        sendEvtNo = UNICHOP_ECADO_HSISTOP;
        regChopRiseAct  = MOD_LOGIC1_REG_HSI_ACT_POSEDGE_RD;
        regChopFallAct  = MOD_LOGIC1_REG_HSI_ACT_NEGEDGE_RD;
        regChopFallCtrl = MOD_LOGIC1_REG_HSI_CTRL_NEGEDGE_RD;
        sendBpid        = (flagBlockHSI << 2) | (flagInterlockHSI << 3);
        rpgAddr         = MOD_RPG_HSI_ADDR;
      } // if not sendEveNo

      // lower 2 bit of sendBpid set to 0 for byte alignment and easier debugging using saft-ctl snoop
      sendBpid         |= (flagCCIRec   << 4) | (flagCCILate      << 5);
        
      comLatency   = (int32_t)(getSysTime() - recDeadline);
      recGid       = (uint32_t)((recEvtId >> 48) & 0x00000fff);
      recEvtNo     = (uint32_t)((recEvtId >> 36) & 0x00000fff);
      recSid       = (uint32_t)((recEvtId >> 20) & 0x00000fff);
      recAttribute = (uint32_t)((recEvtId      ) & 0x0000003f);
            
      if (recSid  > 15)       return COMMON_STATUS_OUTOFRANGE;
      
      if (!flagIsLate) {
        if (recGid == GID_PZU_UN) milModAddr = MOD_LOGIC1_ADDR;
        if (recGid == GID_PZU_UH) milModAddr = MOD_LOGIC1_ADDR;

        status                                 = readFromModuleMil(IFB_ADDR_CU, milModAddr, regChopRiseAct , &tChopRiseAct);
        if (status == COMMON_STATUS_OK) status = readFromModuleMil(IFB_ADDR_CU, milModAddr, regChopFallAct , &tChopFallAct);
        if (status == COMMON_STATUS_OK) status = readFromModuleMil(IFB_ADDR_CU, milModAddr, regChopFallCtrl, &tChopFallCtrl);
        
        // test for valid/invalid bits
        // for explanation of bits see https://github.com/GSI-CS-CO/bel_projects/blob/master/top/fg45041x/chopper_m1/chopper_monitoring.vhd
        if (!tChopRiseAct)                           tChopRiseAct  = UNICHOP_U16_NODATA; 
        else if ((tChopRiseAct  & 0xff80) != 0x8000) tChopRiseAct  = UNICHOP_U16_INVALID;
        else                                         tChopRiseAct  = tChopRiseAct  & 0x7f;
        if (!tChopFallAct)                           tChopFallAct  = UNICHOP_U16_NODATA;
        else if ((tChopFallAct  & 0xc000) != 0x8000) tChopFallAct  = UNICHOP_U16_INVALID;
        else                                         tChopFallAct  = tChopFallAct  & 0x3fff;
        if (!tChopFallCtrl)                          tChopFallCtrl = UNICHOP_U16_NODATA;
        else if ((tChopFallCtrl & 0xc000) != 0x8000) tChopFallCtrl = UNICHOP_U16_INVALID;
        else                                         tChopFallCtrl = tChopFallCtrl & 0x3fff;

        if ((tChopFallAct == UNICHOP_U16_INVALID) || (tChopRiseAct == UNICHOP_U16_INVALID))    lenChopAct = UNICHOP_U16_INVALID;
        else if ((tChopFallAct == UNICHOP_U16_NODATA) && (tChopRiseAct == UNICHOP_U16_NODATA)) lenChopAct = UNICHOP_U16_NODATA;
        else                                                                                   lenChopAct = tChopFallAct - tChopRiseAct;

        // write result to ECA
        sendEvtId    = fwlib_buildEvtidV1(GID_LOCAL_ECPU_FROM, sendEvtNo, 0x0, recSid, sendBpid, recAttribute);
        sendParam    = (uint64_t)(tChopFallCtrl) << 48;
        sendParam   |= (uint64_t)(tChopRiseAct)  << 32;
        sendParam   |= (uint64_t)(tChopFallAct)  << 16;
        sendParam   |= (uint64_t)(lenChopAct); 
        sendDeadline = getSysTime() + COMMON_AHEADT;
        fwlib_ecaWriteTM(sendDeadline, sendEvtId, sendParam, 0x0, 0x0);
      } // if not flagIsLate

      // disable relevant RPG
      writeToModuleMil(IFB_ADDR_CU, rpgAddr, MOD_RPG_XXX_ENABLE_REG, MOD_RPG_XXX_ENABLE_FALSE);

      offsDone = getSysTime() - recDeadline;
      // nEvtsRec++; don't increase counter - this is just monitoring

      break;

    // monitoring 'commands' from chopper control; reset with every new cycle for HLI and HSI
    case UNICHOP_ECADO_HLICMD :
    case UNICHOP_ECADO_HSICMD :                                       // this is an OR, no 'break' on purpose
      flagInterlockHSI = 0;
      flagBlockHSI     = 0;
      flagInterlockHLI = 0;
      flagBlockHLI     = 0;
      flagCCILate      = 0;
      flagCCIRec       = 0;
      break;
      
    default :                                                         // flush ECA Queue
      flagIsLate = 0;                                                 // ignore late events
  } // switch ecaAction
 
  // check for late event
  if ((status == COMMON_STATUS_OK) && flagIsLate) {
    status = COMMON_STATUS_LATEMESSAGE;
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

  nMilSnd        = 0;
  nMilSndErr     = 0;
  nEvtsRec       = 0; 
  nEvtsLate      = 0;

  init();                                                                     // initialize stuff for lm32
  initSharedMem(&reqState, &sharedSize);                                      // initialize shared memory
  fwlib_init((uint32_t *)_startshared, cpuRamExternal, SHARED_OFFS, sharedSize, "uni-chop", UNICHOP_FW_VERSION); // init common stuff
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

    *pSharedGetNEvtsRecHi  = (uint32_t)(nEvtsRec >> 32);
    *pSharedGetNEvtsRecLo  = (uint32_t)(nEvtsRec & 0xffffffff);
    *pSharedGetNMilSndHi   = (uint32_t)(nMilSnd >> 32);
    *pSharedGetNMilSndLo   = (uint32_t)(nMilSnd & 0xffffffff);
    *pSharedGetNMilSndErr  = (uint32_t)(nMilSndErr);
  } // while

  return(1); // this should never happen ...
} // main

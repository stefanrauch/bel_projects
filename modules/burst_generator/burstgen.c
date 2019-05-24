/*******************************************************************************
 *  burstgen.c (derived from lm32 example)
 *
 *  created : 2019, GSI Darmstadt
 *  author  : Dietrich Beck, Enkhbold Ochirsuren
 *  version : 27-Mar-2019
 *
 *  This example demonstrates the pulse generation at IO (of SCU) according to
 *  the ECA timing event principle: ECA condition table is configured with
 *  rules for embedded CPU (eCPU) actions and IO actions. The pulses are
 *  generated by IO actions, which are produced with internal timing messages
 *  sent from LM32. The eCPU actions are used to control pulse generation.
 *  They are produced by external timing messages and handled by LM32.
 *
 *  The pulse generation consists of two phases:
 *   1. Configuration - host configures ECA for both eCPU and IO actions and
 *      provides pulse parameters and production cycle to LM32
 *   2. Production - on eCPU action LM32 starts to send timing messages
 *      periodically to ECA event input to produce IO actions (pulses at IO).
 *      The pulse generation is stopped either after a certain time period or
 *      by another dedicated eCPU action.
 *
 *  LM32 uses MSIs to catch host requests and eCPU actions.
 *  The saftlib tools are used to configure the ECA unit with required rules and
 *  inject timing messages for eCPU actions.
 *  The eb tools are used to provide pulse parameters and production cycle
 *  to LM32.
 *
 *  build:  make clean && make TARGET=burstgen
 *  deploy: scp burstgen.bin root@scuxl0304.acc:.
 *  load:   eb-fwload dev/wbm0 u 0x0 burstgen.bin
 *  run:    eb-reset dev/wbm0 cpureset 0 (assume only one LM32 is instantiated)
 *  debug:  eb-console dev/wbm0
 *
 *  A set of commands used to generate continous pulses of 50KHz at the B1 IO
 *  pin of SCU3 is given below as an example. Please consider that the presented
 *  Wishbone addresses might differ!
 *
 *  1. Commands to configure ECA, write pulse parameters and pulse production
 *  cycle for LM32 into the shared memory:
 *
 *  set ECA rules for IO actions (10 entries with 10000 ns offset):
 *    saft-io-ctl tr0 -n B1 -u -c 0x0000FCA000000000 0xFFFFFFF000000000 10000 0xf 1
 *    saft-io-ctl tr0 -n B1 -u -c 0x0000FCA000000000 0xFFFFFFF000000000 20000 0xf 0
 *    ...
 *    saft-io-ctl tr0 -n B1 -u -c 0x0000FCA000000000 0xFFFFFFF000000000 90000 0xf 1
 *    saft-io-ctl tr0 -n B1 -u -c 0x0000FCA000000000 0xFFFFFFF000000000 100000 0xf 0
 *
 *  set ECA rules for eCPU actions:
 *    saft-ecpu-ctl tr0 -d -c 0x0000991000000000 0xFFFFFFF000000000 0 0xb2b2b2b2
 *    saft-ecpu-ctl tr0 -d -c 0x0000990000000000 0xFFFFFFF000000000 0 0xb2b2b2b2
 *
 *  write pulse parameters to shared RAM:
 *  - IO event id
 *    	eb-write dev/wbm0 0x200A0510/4 0x0000FCA0
 *  - delay, currently not used
 *    	eb-write dev/wbm0 0x200A0514/4 0
 *  - number of IO action rules
 *    	eb-write dev/wbm0 0x200A0518/4 10
 *  - each IO action offset
 *    	eb-write dev/wbm0 0x200A051C/4 10000
 *  and instruct LM32 to load pluse parameters (see below in 2.)
 *
 *  write production cycles to shared RAM:
 *  - production cycles, hi32 and lo32 values
 *  	eb-write dev/wbm0 0x200A0510/4 0x0000FCA0
 *    	eb-write dev/wbm0 0x200A0520/4 0x88776655
 *    	eb-write dev/wbm0 0x200A0524/4 0x44332211
 *  and instruct LM32 to load production cycles (see below in 2.)
 *
 *  These commands are also packed in configure_eca.sh and can be invoked as:
 *    ./configure_eca.sh -n 10 -o 10000 -f 0xf -c 0x88776655 -d 0x44332211
 *
 *  2. Host requests to LM32 that instruct the embedded soft CPU to get
 *  the pulse parameters and production cycle from a shared memory:
 *
 *  - show actual pulse parameters
 *    	eb-write dev/wbm0 0x808/4 1
 *  - load pulse parameters from shared RAM
 *    	eb-write dev/wbm0 0x808/4 2
 *  - load production cycles from shared RAM
 *    	eb-write dev/wbm0 0x808/4 3
 *  - show actual pulse parameters again for verification (optional)
 *  	eb-write dev/wbm0 0x808/4 1
 *
 *  3. A target timing receiver is now ready to generate pulses at its output.
 *  Users need to inject timing messages to start and stop pulse generation:
 *
 *  - start pulse generation:
 *    	saft-ctl -p tr0 inject 0x0000991000000000 100000000 0
 *  - stop pulse generation:
 *    	saft-ctl -p tr0 inject 0x0000990000000000 0 0
 *
 * -----------------------------------------------------------------------------
 * License Agreement for this software:
 *
 * Copyright (C) 2017  Dietrich Beck
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
 * Last update: 25-April-2015
 ******************************************************************************/

/* standard includes */
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <stdint.h>

/* includes specific for bel_projects */
#include "mprintf.h"
#include "mini_sdb.h"
#include "aux.h"
#include "dbg.h"
#include "syscon.h"

#include "bg.h" // burst generation

/* function prototypes */
void buildTimingMsg(uint32_t *msg, uint32_t id); // build timing message
void injectTimingMsg(uint32_t *msg);  // inject timing message to ECA event input
void ecaHandler(uint32_t);            // pop pending eCPU actions from ECA queue

void ecaMsiHandler(int id);           // handler for the ECA MSIs
void hostMsiHandler(int id);          // handler for host MSIs
void triggerIoActions(int id);        // trigger IO actions to generate pulses
void dummyTask(int id);

/* definitions of MSI message buffers */
enum {
  ECA_MSI = 0,
  HOST_MSI,
  N_MSI_BUF
};

/* task configuration table */
static Task_t tasks[4]= {
  {0, 0, 0, 0, 0, 0, ALWAYS, 0, ecaMsiHandler     },
  {0, 0, 0, 0, 0, 0, ALWAYS, 0, triggerIoActions  },
  {0, 0, 0, 0, 0, 0, ALWAYS, 0, hostMsiHandler    },
  {0, 0, 0, 0, 0, 0, INTERVAL_2000MS, 0, dummyTask},
};

static Task_t *pTask = &tasks[0];                      // task table pointer
const int cNumTasks = sizeof(tasks) / sizeof(Task_t);  // number of tasks

volatile struct message_buffer msg_buf[N_MSI_BUF] = {0};   // MSI message buffers
volatile struct message_buffer *pMsgBufHead = &msg_buf[0]; // pointer to MSI msg buffer head

uint32_t bufTimMsg[LEN_TIM_MSG];  // buffer of timing message for IO action (will be sent by this LM32)

/* stuff required for environment */
unsigned int cpuId, cpuQty;
#define SHARED __attribute__((section(".shared")))
uint64_t SHARED dummy = 0;

// global variables
volatile uint32_t *pEcaCtl;         // WB address of ECA control
volatile uint32_t *pEca;            // WB address of ECA event input (discoverPeriphery())
volatile uint32_t *pECAQ;           // WB address of ECA queue
volatile uint32_t *pShared;         // pointer to begin of shared memory region
volatile uint32_t *pCpuRamExternal; // external address (seen from host bridge) of this CPU's RAM
volatile uint32_t *pSharedInput;    // pointer to a "user defined" u32 register; here: get input from host system
volatile uint32_t *pSharedCmd;      // pointer to a "user defined" u32 register; here: get commnand from host system

uint64_t gInjection = 0;            // time duration for local message injection to ECA event input
int gEcaChECPU = 0;                 // ECA channel for an embedded CPU (LM32), connected to ECA queue pointed by pECAQ
int gMbSlot = -1;                   // slot in mailbox subscribed by LM32, no slot is subscribed by default

void dummyTask(int id) {

  // wait for 60 seconds
  uint64_t t = getSysTime() - pTask[id].deadline;

  if (t > INTERVAL_60S)
  {
    //mprintf("elapsed %d ms\n",(uint32_t)(t / MS_SCALE)); // enable output msg only for debugging!
    pTask[id].deadline = getSysTime();
  }
}

/*******************************************************************************
 *
 * Trigger IO actions to generate pulses at IO pin
 *
 ******************************************************************************/
void triggerIoActions(int id) {

  if (pTask[id].deadline == 0)     // deadline is unset, nothing to do!
    return;

  if (pTask[id].cycle == 0)        // production cycle is over, nothing to do!
    return;

  uint64_t deadline = pTask[id].deadline;
  uint64_t now = getSysTime();

  if ((deadline - now) < gInjection || (deadline < now)) // setup due or late!
  {
    *(bufTimMsg +6) = hiU32(deadline);
    *(bufTimMsg +7) = loU32(deadline);

    injectTimingMsg(bufTimMsg);  // inject internal timing message for IO actions

    pTask[id].deadline += pTask[id].period;  // update deadline for next trigger

    if (pTask[id].cycle > 0)     // verify and update the production cycle
    {
      if (--pTask[id].cycle == 0)
      {
	pTask[id].deadline = 0;
	mprintf("cycle completed: reload!\n");
      }
    }
  }
}

/*******************************************************************************
 *
 * Clear ECA queue
 *
 * @param[in] cnt The number pending actions
 * \return        The number of cleared actions
 *
 ******************************************************************************/
uint32_t clearEcaQueue(uint32_t cnt)
{
  uint32_t flag;                // flag for the next action
  uint32_t i, j = 0;

  for ( i = 0; i < cnt; ++i) {

    flag = *(pECAQ + (ECA_QUEUE_FLAGS_GET >> 2));  // read flag and check if there was an action

    if (flag & (0x0001 << ECA_VALID)) {
      *(pECAQ + (ECA_QUEUE_POP_OWR >> 2)) = 0x1;   // pop action from channel
      ++j;
    }
  }

  return j;
}

/*******************************************************************************
 *
 * Clear pending valid actions
 *
 ******************************************************************************/
void clearActions()
{
  uint32_t valCnt;

  *(pEcaCtl + (ECA_CHANNEL_SELECT_RW >> 2)) = gEcaChECPU;    // select ECA channel for LM32
  valCnt = *(pEcaCtl + (ECA_CHANNEL_VALID_COUNT_GET >> 2));  // get/clear valid count
  if (valCnt) {
    mprintf("pending actions: %d\n", valCnt);
    valCnt = clearEcaQueue(valCnt);                          // pop pending actions
    mprintf("cleared actions: %d\n", valCnt);
  }
}

/*******************************************************************************
 *
 * Handle pending valid actions
 *
 ******************************************************************************/
void handleValidActions()
{
  uint32_t valCnt;
  *(pEcaCtl + (ECA_CHANNEL_SELECT_RW >> 2)) = gEcaChECPU;    // select ECA channel for LM32
  valCnt = *(pEcaCtl + (ECA_CHANNEL_VALID_COUNT_GET >> 2));  // read and clear valid counter
  mprintf("\nvalid=%d\n", valCnt);

  if (valCnt != 0)
    ecaHandler(valCnt);                             // pop pending valid actions
}

void handleFailedActions()
{
  atomic_on();
  *(pEcaCtl + (ECA_CHANNEL_SELECT_RW >> 2)) = gEcaChECPU;    // select ECA channel for LM32

  *(pEcaCtl + (ECA_CHANNEL_OVERFLOW_COUNT_GET >> 2));        // read and clear overflow counter

  *(pEcaCtl + (ECA_CHANNEL_CODE_SELECT_RW >> 2)) = (ECA_FG_DELAYED >> 16);
  *(pEcaCtl + (ECA_CHANNEL_FAILED_COUNT_GET >> 2));          // read and clear delayed counter
  *(pEcaCtl + (ECA_CHANNEL_CODE_SELECT_RW >> 2)) = (ECA_FG_CONFLICT >> 16);
  *(pEcaCtl + (ECA_CHANNEL_FAILED_COUNT_GET >> 2));          // read and clear conflict counter
  *(pEcaCtl + (ECA_CHANNEL_CODE_SELECT_RW >> 2)) = (ECA_FG_EARLY >> 16);
  *(pEcaCtl + (ECA_CHANNEL_FAILED_COUNT_GET >> 2));          // read and clear early counter
  *(pEcaCtl + (ECA_CHANNEL_CODE_SELECT_RW >> 2)) = (ECA_FG_LATE >> 16);
  *(pEcaCtl + (ECA_CHANNEL_FAILED_COUNT_GET >> 2));          // read and clear late counter
  atomic_off();
}

/*******************************************************************************
 *
 * Handle a pending ECA MSI
 *
 ******************************************************************************/
void ecaMsiHandler(int id)
{
  if (has_msg(pMsgBufHead, ECA_MSI)) {

    struct msi m = remove_msg(pMsgBufHead, ECA_MSI);

    mprintf("\n!Got MSI 0x%08x (h16: 0-3 faild, 4 vald, 5 ovrflw, 6 full)\n", m.msg); // debugging, remove later

    switch (m.msg & ECA_FG_MASK)
    {
      case ECA_FG_VALID: // valid actions are pending
	handleValidActions(); // ECA MSI handling
	break;
      case ECA_FG_MOSTFULL:
	break;
      default:
	handleFailedActions();
	break;
    }
  }
}

/*******************************************************************************
 *
 * Configure ECA to send MSI to embedded soft-core LM32:
 * - MSI is sent on production of actions for the ECA action
 *   channel for LM32
 * - ECA action channel is selected and MSI target address of LM32 is set in the
 *   ECA MSI target register
 *
 * @param[in] enable  Enable or disable ECA MSI
 * @param[in] channel The index of the selected ECA action channel
 *
 ******************************************************************************/
void configureEcaMsi(int enable, uint32_t channel) {

  if (enable != 0 && enable != 1) {
    mprintf("Bad enable argument. %s\n", errMsgEcaMsi);
    return;
  }

  if (channel > ECAQMAX) {
    mprintf("Bad channel argument. %s\n", errMsgEcaMsi);
    return;
  }

  clearActions();     // clean ECA queue and channel from previous actions

  atomic_on();
  *(pEcaCtl + (ECA_CHANNEL_SELECT_RW >> 2)) = channel;            // select channel
  *(pEcaCtl + (ECA_CHANNEL_MSI_SET_ENABLE_OWR >> 2)) = 0;         // disable ECA MSI (required to set a target address)
  *(pEcaCtl + (ECA_CHANNEL_MSI_SET_TARGET_OWR >> 2)) = (uint32_t)pMyMsi;  // set MSI destination address as a target address
  *(pEcaCtl + (ECA_CHANNEL_MSI_SET_ENABLE_OWR >> 2)) = enable;    // enable ECA MSI
  atomic_off();

  mprintf("\nMSI path (ECA -> LM32)      : %s\n\tECA channel = %d\n\tdestination = 0x%08x)\n",
          enable == 1 ? "enabled" : "disabled", channel, (uint32_t)pMyMsi);
}

/*******************************************************************************
 *
 * Respond to host request
 *
 * @param[in] data Response data to host request
 *
 ******************************************************************************/
void respondToHost(uint32_t data)
{
  mprintf("host request %s\n", data == STATUS_OK ? "accepted" : "rejected!");
}

/*******************************************************************************
 *
 * Check if unhandled MSI messages exist
 *
 * /return  status  Returns non-zero value if unhandled MSI messages exist
 *
 ******************************************************************************/
int hasPendingMsi(void)
{
  return (has_msg(pMsgBufHead, ECA_MSI) || has_msg(pMsgBufHead, HOST_MSI));
}

/*******************************************************************************
 *
 * Handle MSIs
 *
 * Any WB device connected to MSI crossbar as a master can send MSIs: ECA, SCU bus etc.
 * Besides data value, an MSI message includes also a destination address.
 * In order to identify a particular sender, LM32 has to inform them distinct
 * MSI destinations as its destination.
 *
 * Handling ECA MSIs
 * If interrupt was caused by a valid ECA action, then MSI has value of (4<<16|num).
 * Both ECA action channel and ECA queue connected to that channel must be handled:
 * - read and clear the valid counter value of ECA action channel for LM32 and,
 * - pop pending actions from ECA queue connected to this action channel
 *
 ******************************************************************************/
void irqHandler() {

  struct msi m;
  m.msg = global_msi.msg;
  m.adr = global_msi.adr;

  uint32_t sender = m.adr & MSI_OFFS_MASK;
  switch (sender) {

    case MSI_OFFS_ECA:     // ECA
      add_msg(pMsgBufHead, ECA_MSI, m);
      break;

    case MSI_OFFS_HOST:    // HOST
      add_msg(pMsgBufHead, HOST_MSI, m);
      break;

    default:
      mprintf("%s: %x Unknown MSI sender. Cannot handle MSI!\n", m.adr);
  }
  //mprintf(" MSI:\t%08x\nAdr:\t%08x\nSel:\t%01x\n", global_msi.msg, global_msi.adr, global_msi.sel);
}

/*******************************************************************************
 *
 * Initialize interrupt table
 * - set up an interrupt handler
 * - enable interrupt generation globally
 *
 ******************************************************************************/
void initIrqTable() {
  isr_table_clr();
  memset((void *)pMsgBufHead, 0, N_MSI_BUF * sizeof(struct message_buffer));
  if (hasPendingMsi())
  {
    mprintf("MSI buffers are not empty!!!\n");
    mprintf("Cannot enable interrupt!!!\n");
    return;
  }
  else
    mprintf("MSI buffers are clean.\n");

  isr_ptr_table[0] = &irqHandler;
  irq_set_mask(0x01);
  irq_enable();
  mprintf("Init IRQ table is done.\n");
}

/*******************************************************************************
 *
 * Demonstrate exchange of data to Wishbone via shared RAM
 * - the data can be accessed via Etherbone->Wishbone
 * - try eb-read/eb-write from the host system
 *
 ******************************************************************************/
void initSharedMem()
{
  uint32_t i,j;
  uint32_t idx;
  const uint32_t c_Max_Rams = 10;
  sdb_location found_sdb[c_Max_Rams];
  sdb_location found_clu;

  // get pointer to shared memory; internal perspective of this LM32
  pShared        = (uint32_t *)_startshared;                // begin of shared mem

  // print pointer info to UART
  mprintf("\n");
  mprintf("Internal shared memory    @ 0x%08x\n", (uint32_t)pShared);

  // get pointer to shared memory; external perspective from host bridge
  idx = 0;
  find_device_multi(&found_clu, &idx, 1, GSI, LM32_CB_CLUSTER);
  idx = 0;
  find_device_multi_in_subtree(&found_clu, &found_sdb[0], &idx, c_Max_Rams, GSI, LM32_RAM_USER);
  if(idx >= cpuId) {
    pCpuRamExternal = (uint32_t*)(getSdbAdr(&found_sdb[cpuId]) & 0x7FFFFFFF); // CPU sees the 'world' under 0x8..., remove that bit to get host bridge perspective
    // print external WB info to UART
    mprintf("External shared memory    @ 0x%08x\n", (uint32_t)(pCpuRamExternal + (SHARED_OFFS >> 2)));
  } else {
    pCpuRamExternal = (uint32_t*)ERROR_NOT_FOUND;
    mprintf("Could not find external WB address of my own RAM !\n");
  }
}

/*******************************************************************************
 *
 * Get/subscribe slot in mailbox
 *
 * Check mailbox slots starting from the second slot. If a slot has the same
 * destination address, then re-use it. If a slot is free, then subscribe it.
 *
 * @param[in] offset  Offset address used to recognize a sender.
 * /return    slot    Subscribed slot number. Returns -1, if no free slot is found.
 *
 ******************************************************************************/
int getMboxSlot(uint32_t offset)
{
  uint32_t myDestAddr = (uint32_t)(pMyMsi + (offset >> 2));
  uint32_t destination;
  unsigned char notFound = 1;
  uint8_t slot = 1;

  atomic_on();
  while (notFound && (slot < 128))
  {
    destination = *(pCpuMsiBox + (slot << 1)); // get destination address
    if (destination == myDestAddr)             // slot has my destination address
      notFound = 0;
    else if (destination == 0xffffffff) {      // slot is free, subscribe it
      cfgMsiBox(slot,offset);
      notFound = 0;
    }
    else
      slot++;
  }
  atomic_off();

  return (notFound ? -1 : slot);
}

/*******************************************************************************
 *
 * Find WB address of ECA queue connect to ECA channel for LM32
 *
 * - ECA queue address is set to "pECAQ"
 * - index of ECA channel for LM32 is set to "gEcaChECPU"
 *
 * /return  status  Return OK if a queue is found, otherwise return ERROR
 *
 ******************************************************************************/
uint32_t findEcaQueue()
{
  sdb_location EcaQ_base[ECAQMAX];
  uint32_t EcaQ_idx = 0;
  uint32_t *tmp;
  int i;

  // get list of ECA queues
  find_device_multi(EcaQ_base, &EcaQ_idx, ECAQMAX, ECA_QUEUE_SDB_VENDOR_ID, ECA_QUEUE_SDB_DEVICE_ID);
  pECAQ = 0x0;

  // find ECA queue connected to ECA channel for LM32
  for (i=0; i < EcaQ_idx; i++) {
    tmp = (uint32_t *)(getSdbAdr(&EcaQ_base[i]));
    //mprintf("-- found ECA queue 0x%08x, idx %d\n", (uint32_t)tmp, i);
    if ( *(tmp + (ECA_QUEUE_QUEUE_ID_GET >> 2)) == ECACHANNELFORLM32) {
      pECAQ = tmp;    // update global variables
      gEcaChECPU = ECACHANNELFORLM32 +1; // refer to eca_queue_regs.h
      i = EcaQ_idx;   // break loop
    }
  }

  return (pECAQ ? STATUS_OK : STATUS_ERR);
}

/*******************************************************************************
*
* Pop pending eCPU actions from an ECA queue and handle them
*
* @param[in] cnt The number of pending valid actions
*
*******************************************************************************/
void ecaHandler(uint32_t cnt)
{
  uint32_t flag;                // flag for the next action
  uint32_t evtIdHigh;           // event id (high 32bit)
  uint32_t evtIdLow;            // event id (low 32bit)
  uint32_t evtDeadlHigh;        // deadline (high 32bit)
  uint32_t evtDeadlLow;         // deadline (low 32bit)
  uint32_t actTag;              // tag of action
  uint32_t paramHigh;           // event parameter (high 32bit)
  uint32_t paramLow;            // event parameter (low 32bit)

  for (int i = 0; i < cnt; ++i) {
    // read flag and check if there was an action
    flag         = *(pECAQ + (ECA_QUEUE_FLAGS_GET >> 2));
    if (flag & (0x0001 << ECA_VALID)) {
      // read data
      evtIdHigh    = *(pECAQ + (ECA_QUEUE_EVENT_ID_HI_GET >> 2));
      evtIdLow     = *(pECAQ + (ECA_QUEUE_EVENT_ID_LO_GET >> 2));
      evtDeadlHigh = *(pECAQ + (ECA_QUEUE_DEADLINE_HI_GET >> 2));
      evtDeadlLow  = *(pECAQ + (ECA_QUEUE_DEADLINE_LO_GET >> 2));
      actTag       = *(pECAQ + (ECA_QUEUE_TAG_GET >> 2));
      paramHigh    = *(pECAQ + (ECA_QUEUE_PARAM_HI_GET >> 2));
      paramLow     = *(pECAQ + (ECA_QUEUE_PARAM_LO_GET >> 2));

      // pop action from channel
      *(pECAQ + (ECA_QUEUE_POP_OWR >> 2)) = 0x1;

      // here: do s.th. according to action
      if (actTag == MY_ACT_TAG) {
        mprintf("id: 0x%08x:%08x; deadline: 0x%08x:%08x; param: 0x%08x:%08x; flag: 0x%08x\n",
                evtIdHigh, evtIdLow, evtDeadlHigh, evtDeadlLow, paramHigh, paramLow, flag);

	uint64_t d, p;

	toU64(evtDeadlHigh, evtDeadlLow, d);
	toU64(paramHigh, paramLow, p);

	switch (evtIdHigh & EVTNO_MASK) {

	  case IO_CYC_START: // start the IO pulse cycle
	    d += p;
	    p = getSysTime();

	    if (p >= (d + gInjection)) {
	      pTask[0].deadline = 0;
	      mprintf("late! now >= (due + inj)\n");
	      mprintf("now: 0x%08x:%08x\n", (uint32_t)(p >> 32), (uint32_t)p);
	      mprintf("due: 0x%08x:%08x\n", (uint32_t)(d >> 32), (uint32_t)d);
	      mprintf("inj: 0x%08x:%08x\n", (uint32_t)(gInjection >> 32), (uint32_t)gInjection);
	      mprintf("cycle ignored!\n");
	    }
	    else {
	      pTask[0].deadline = d;           // set deadline
	      pTask[0].interval = gInjection;  // set task sched. interval
	      mprintf("cycle ready\n");
	    }
	    break;

	  case IO_CYC_STOP: // stop the IO pulse cycle
	    d += p;
	    if (pTask[0].deadline == 0) {      // cycle never run or already stopped, cancel it
	      pTask[0].cycle = 0;
	      mprintf("cycle cancelled!\n");
	    }
	    else if (pTask[0].deadline > d) {  // deadline is over, stop immediatelly
	      pTask[0].deadline = 0;
	      pTask[0].cycle = 0;
	      mprintf("cycle stopped!\n");
	    }
	    else if (pTask[0].cycle > 0) { // update remaining cycle
	      p = d - pTask[0].deadline;
	      p = p / pTask[0].period;
	      pTask[0].cycle = p;
	      mprintf("cycle changed!\n");
	    }
	    break;

	  default:
	    break;
	}
      }
    }
  }
}

/*******************************************************************************
 *
 * Execute commands received from host
 *
 * @param[i]  cmd  Predefined command code
 *
 ******************************************************************************/
void execHostCmd(int32_t cmd)
{
  int result = STATUS_OK;
  // check, if a command has been issued (no cmd: 0x0)
  if (cmd) {
    mprintf("\ncmd 0x%x: ", cmd);
    switch (cmd) {

    case CMD_SHOW_ALL:    // show pulse parameters
      mprintf("show\n");  // show actual state

      mprintf("id=0x%x, cycle=0x%x:%x, period=0x%x:%x, deadline=0x%x:%x, interval=0x%x:%x\n",
	  (uint32_t)(*bufTimMsg),
	  (uint32_t)(pTask[0].cycle >> 32), (uint32_t)pTask[0].cycle,
	  (uint32_t)(pTask[0].period >> 32), (uint32_t)pTask[0].period,
	  (uint32_t)(pTask[0].deadline >> 32), (uint32_t)pTask[0].deadline,
	  (uint32_t)(pTask[0].interval >> 32), (uint32_t)pTask[0].interval);
      break;

    case CMD_GET_PARAM:    // get pulse parameters expressed by ECA conditions (e_id, delay, nConds, period) from shared RAM
      mprintf("get parameters\n");

      mprintf("%8x @ 0x%x\n", *(pSharedInput   ), (uint32_t)(pSharedInput   )); // e_id, h32
      mprintf("%8x @ 0x%x\n", *(pSharedInput +1), (uint32_t)(pSharedInput +1)); // delay
      mprintf("%8x @ 0x%x\n", *(pSharedInput +2), (uint32_t)(pSharedInput +2)); // number of conditions (pulse block)
      mprintf("%8x @ 0x%x\n", *(pSharedInput +3), (uint32_t)(pSharedInput +3)); // period of a pulse block, nanoseconds

      toU64(*pSharedInput, EVT_ID_IO_L32, pTask[0].event);
      buildTimingMsg(bufTimMsg, *pSharedInput);                 // re-build timing msg for IO actions
      pTask[0].period = *(pSharedInput +3);
      break;

    case CMD_GET_CYCLE:    // get pulse cycle (e_id, cycles * period of a pulse block) from shared RAM
      mprintf("get cycle\n");

      mprintf("%8x @ 0x%x\n", *(pSharedInput   ), (uint32_t)(pSharedInput   )); // e_id, h32
      mprintf("%8x @ 0x%x\n", *(pSharedInput +1), (uint32_t)(pSharedInput +1)); // pulse cycles (h32), nanoseconds
      mprintf("%8x @ 0x%x\n", *(pSharedInput +2), (uint32_t)(pSharedInput +2)); // pulse cycles (l32), nanoseconds

      if ((pTask[0].event >> 32) == *pSharedInput)
      {
	toU64(*(pSharedInput +1), *(pSharedInput +2), pTask[0].cycle);

	// init task deadline and interval
	pTask[0].deadline = 0;
	pTask[0].interval = ALWAYS;
      }
      break;

    case CMD_RD_MSI_ECPU: // read the ECA MSI settings for eCPU
      mprintf("read MSI cfg\n");

      atomic_on();
      *(pEcaCtl + (ECA_CHANNEL_SELECT_RW >> 2)) = gEcaChECPU;              // select channel for eCPU
      uint32_t dest   = *(pEcaCtl + (ECA_CHANNEL_MSI_GET_TARGET_GET >> 2));  // get MSI destination address
      uint32_t enable = *(pEcaCtl + (ECA_CHANNEL_MSI_GET_ENABLE_GET >> 2));  // get the MSI enable flag
      atomic_off();

      mprintf("MSI dest addr   = 0x%08x\n", dest);
      mprintf("MSI enable flag = 0x%x\n", enable);

      break;

    case CMD_RD_ECPU_CHAN: // read the content of the ECA eCPU channel
      mprintf("read eCPU chan counter\n");

      atomic_on();
      *(pEcaCtl + (ECA_CHANNEL_SELECT_RW >> 2)) = gEcaChECPU;
      uint32_t valid    = *(pEcaCtl + (ECA_CHANNEL_VALID_COUNT_GET >> 2));
      uint32_t overflow = *(pEcaCtl + (ECA_CHANNEL_OVERFLOW_COUNT_GET >> 2));
      uint32_t failed   = *(pEcaCtl + (ECA_CHANNEL_FAILED_COUNT_GET >> 2));
      uint32_t full     = *(pEcaCtl + (ECA_CHANNEL_MOSTFULL_ACK_GET >> 2));
      atomic_off();
      mprintf("failed: 0x%x, valid: 0x%x, overflow: 0x%x, full: 0x%x\n",
                failed, valid, overflow, full);
      break;

    case CMD_RD_ECPU_QUEUE: // read the content of ECA queue connected to eCPU channel
      mprintf("read eCPU queue\n");

      atomic_on();
      *(pEcaCtl + (ECA_CHANNEL_SELECT_RW >> 2)) = gEcaChECPU;
      uint32_t flag      = *(pECAQ + (ECA_QUEUE_FLAGS_GET >> 2));
      uint32_t evtHigh   = *(pECAQ + (ECA_QUEUE_EVENT_ID_HI_GET >> 2));
      uint32_t evtLow    = *(pECAQ + (ECA_QUEUE_EVENT_ID_LO_GET >> 2));
      uint32_t tag       = *(pECAQ + (ECA_QUEUE_TAG_GET >> 2));
      uint32_t paramHigh = *(pECAQ + (ECA_QUEUE_PARAM_HI_GET >> 2));
      uint32_t paramLow  = *(pECAQ + (ECA_QUEUE_PARAM_LO_GET >> 2));
      atomic_off();
      mprintf("event: 0x%08x:%08x, param: 0x%08x:%08x, tag: 0x%08x, flag: 0x%08x\n",
                evtHigh, evtLow, paramHigh, paramLow, tag, flag);
      break;

   default:
      mprintf("unknown\n");
      result = STATUS_ERR;
    }

    if (result == STATUS_ERR)
      respondToHost(result);
  }
}

/*******************************************************************************
 *
 * Handle a pending host MSI
 *
 ******************************************************************************/
void hostMsiHandler(int id)
{
  if (has_msg(pMsgBufHead, HOST_MSI)) {

    struct msi m = remove_msg(pMsgBufHead, HOST_MSI);
    execHostCmd(m.msg);
  }
}

/*******************************************************************************
*
* Initialize dedicated buffers in shared memory
*
*******************************************************************************/
void initSharedBuffers()
{
  pSharedCmd = (uint32_t *)(pShared + (SHARED_CMD >> 2));       // get pointer to shared command buffer
  pSharedInput = (uint32_t *)(pShared + (SHARED_INPUT >> 2));   // get pointer to shared input buffer

  mprintf("\n");
  mprintf("Command buffer (ext)      @ 0x%08x (0x%08x)\n",
      (uint32_t)pSharedCmd, (uint32_t)(pCpuRamExternal + ((SHARED_CMD + SHARED_OFFS) >> 2)));
  mprintf("Data buffer    (ext)      @ 0x%08x (0x%08x)\n",
      (uint32_t)pSharedInput, (uint32_t)(pCpuRamExternal + ((SHARED_INPUT + SHARED_OFFS) >> 2)));
  mprintf("\n");

  *pShared = BG_FW_ID; // label the starting point of the shared memory with the firmware id
  *pSharedCmd = 0x0;  // initalize command value: 0x0 means 'no command'
}

/*******************************************************************************
 *
 * Set up MSI handlers
 *
 ******************************************************************************/
void setupMsiHandlers(void)
{
  gMbSlot = getMboxSlot(MSI_OFFS_HOST); // host MSIs are forwarded to destination address of (pMyMsi + MSI_OFFS_HOST)

  if (gMbSlot == -1)  {
    mprintf("Could not find free slot in mailbox. Exit!\n");
    return;
  }
  else  {
    mprintf("Mailbox slot for host MSIs  : %d (base +0x%x)\n", gMbSlot, gMbSlot*8);

    if (pShared)
    {
      *(pShared + (SHARED_MB_SLOT >> 2)) = gMbSlot; // write the subscribed mailbox slot into the shared memory
    }
    else
    {
      mprintf("Logic error: shared memory must be initialized prior to the mailbox slot subscription");
      return;
    }
  }

  configureEcaMsi(1, gEcaChECPU); // ECA MSIs are sent to destination address of pMyMsi

  initIrqTable();     // set up MSI handler
}

/*******************************************************************************
 *
 * Build a timing message
 *
 * @param[out] msg The location of message buffer
 * @param[in]  id  The event id
 *
 ******************************************************************************/
void buildTimingMsg(uint32_t *msg, uint32_t id)
{
  *msg = id; // FID+GID*EVTNO+flags
  *(msg +1) = 0x0; // SID+BPID+resrv
  *(msg +2) = 0x0; // param_up
  *(msg +3) = 0x0; // param_lo
  *(msg +4) = 0x0; // resrv
  *(msg +5) = 0x0; // TEF
  *(msg +6) = 0x0;
  *(msg +7) = 0x0;

  /*mprintf("\nconstructed timing msg:\n");
  mprintf("event: %x-%x\n",msg[0], msg[1]);
  mprintf("param: %x-%x\n",msg[2], msg[3]);
  mprintf("resrv: %x\n",msg[4]);
  mprintf("TEF  : %x\n",msg[5]);*/
}

/*******************************************************************************
 *
 * Inject the given timing message to the ECA event input
 *
 * @param[in] msg The location of message buffer
 *
 ******************************************************************************/
void injectTimingMsg(uint32_t *msg)
{
  atomic_on();

  for (int i = 0; i < LEN_TIM_MSG; i++)
    *pEca = msg[i];

  atomic_off();
}

/*******************************************************************************
 *
 * Set up internal timing message for the IO actions
 *
 * @param[in]  msg  The location of message buffer
 *
 ******************************************************************************/
void setupTimingMsg(uint32_t *msg)
{
  buildTimingMsg(msg, EVT_ID_IO_H32 << 1); // build a dummy timing message

  uint64_t deadline = getSysTime();

  *(msg +6) = hiU32(deadline); // deadline can be late, don't care it
  *(msg +7) = loU32(deadline);

  injectTimingMsg(msg);        // inject a dummy message to estimate message injection duration

  gInjection = getSysTime() -deadline; // get the injection duration
  gInjection <<= 1;
  mprintf("Injection (ns)              : 0x%x%08x\n", (uint32_t) (gInjection >> 32), (uint32_t) gInjection );

  buildTimingMsg(msg, EVT_ID_IO_H32); // build the default timing message for IO actions
}

/*******************************************************************************
 *
 * Initialization
 * - discover WB devices
 * - init UART
 * - detect ECA control unit
 * - detect ECA queues
 *
 ******************************************************************************/
void init()
{
  discoverPeriphery();    // mini-sdb: get info on important Wishbone infrastructure, such as (this) CPU, flash, ...

  uart_init_hw();         // init UART, required for printf... . To view print message, you may use 'eb-console' from the host

  mprintf("\n Wishbone device detection (%s)\n", __FILE__);

  if (pEca)
    mprintf("ECA event input                @ 0x%08x\n", (uint32_t) pEca);
  else {
    mprintf("Could not find the ECA event input. Exit!\n");
    return;
  }

  mprintf("Mailbox                        @ 0x%08x\n", (uint32_t)pCpuMsiBox);
  mprintf("MSI destination path of LM32   : 0x%08x\n", (uint32_t)pMyMsi);

  cpuId = getCpuIdx();    // get ID of THIS CPU

  pEcaCtl = find_device_adr(ECA_SDB_VENDOR_ID, ECA_SDB_DEVICE_ID);

  if (pEcaCtl)
    mprintf("ECA channel control            @ 0x%08x\n", (uint32_t) pEcaCtl);
  else {
    mprintf("Could not find the ECA channel control. Exit!\n");
    return;
  }

  if (findEcaQueue() == STATUS_OK)
    mprintf("ECA queue to eCPU action ch    @ 0x%08x\n", (uint32_t) pECAQ);
  else {
    mprintf("Could not find an ECA queue connected to eCPU action ch. Exit!\n");
    return;
  }

  timer_init(1);          // needed by usleep_init()
  usleep_init();          // needed by scu_mil.c

  isr_table_clr();        // set MSI IRQ handler
  irq_set_mask(0x01);     // ...
  irq_disable();          // ...
}

void main(void) {

  uint64_t tick;

  init();               // discover mailbox, own MSI path, ECA event input, ECA queue for LM32 channel
  initSharedMem();      // init shared memory
  initSharedBuffers();  // init dedicated buffers in shared memory for command & data exchange with host

  setupTimingMsg(bufTimMsg);  // build default timing msg for IO action, estimate the duration of message injection to the ECA event input
  setupMsiHandlers();         // set up MSI handlers

  int taskIdx = 0;            // reset task index
  pTask[cNumTasks - 1].deadline = getSysTime(); // update the deadline of dummy task handler

  mprintf("\nwaiting host commmand ...\n");

  while(1) {

    // loop through all task. first, run all continuous tasks. then, if the number of ticks
    // since the last time the task was run is greater than or equal to the task interval, execute the task
    for (taskIdx = 0; taskIdx < cNumTasks; taskIdx++) {

      tick = getSysTime();

      if (pTask[taskIdx].interval == ALWAYS) {
        // run contiuous tasks
        (*pTask[taskIdx].func)(taskIdx);

      } else if ((tick - pTask[taskIdx].lasttick) >= pTask[taskIdx].interval) {
        // run task
        (*pTask[taskIdx].func)(taskIdx);
        pTask[taskIdx].lasttick = tick; // save last tick the task was ran
      }
    }
  }
}

#include "scu_mil.h"
#include "aux.h"


/***********************************************************
 ***********************************************************
 *  
 * 1st part: original MIL bus library
 *
 ***********************************************************
 ***********************************************************/


int write_mil(volatile unsigned int *base, short data, short fc_ifc_addr) {
  atomic_on();
  base[MIL_SIO3_TX_DATA] = data;
  base[MIL_SIO3_TX_CMD]  = fc_ifc_addr;
  atomic_off();
  return OKAY;
}


int write_mil_blk(volatile unsigned int *base, short *data, short fc_ifc_addr) {
  int i;
  atomic_on();
  base[MIL_SIO3_TX_DATA] = data[0];
  base[MIL_SIO3_TX_CMD]  = fc_ifc_addr;
  for (i = 1; i < 6; i++) {
      base[MIL_SIO3_TX_DATA] = data[i];
  }
  atomic_off();
  return OKAY;
}

int status_mil(volatile unsigned int *base, unsigned short *status) {
  //atomic_on();
  //*status = base[MIL_WR_RD_STATUS];
  //atomic_off();
  return OKAY;
}

int scub_status_mil(volatile unsigned short *base, int slot, unsigned short *status) {
  //atomic_on();
  //*status = base[CALC_OFFS(slot) + MIL_WR_RD_STATUS];
  //atomic_off();
  return OKAY;
}


int read_mil(volatile unsigned int *base, short *data, short fc_ifc_addr) {
  unsigned short rx_data_avail;
  unsigned short rx_err;
  unsigned short rx_req;

  // write fc and addr to taskram
  base[MIL_SIO3_TX_TASK1] = fc_ifc_addr;

  // wait for task to start (tx fifo full or other tasks running)
  rx_req = base[MIL_SIO3_TX_REQ];
  while(!(rx_req & 0x2)) {
    usleep(1);
    rx_req = base[MIL_SIO3_TX_REQ];
  }

  // wait for task to finish, a read over the dev bus needs at least 40us
  rx_data_avail = base[MIL_SIO3_D_RCVD];
  while(!(rx_data_avail & 0x2)) {
    usleep(1);
    rx_data_avail = base[MIL_SIO3_D_RCVD];
  }

  // task finished
  rx_err = base[MIL_SIO3_D_ERR];
  if ((rx_data_avail & 0x2) && !(rx_err & 0x2)) {
    // copy received value
    *data = 0xffff & base[MIL_SIO3_RX_TASK1];
    return OKAY;
  } else {
    // dummy read resets available and error bits
    base[MIL_SIO3_RX_TASK1];
    return RCV_TIMEOUT;
  }
}

int scub_read_mil(volatile unsigned short *base, int slot, short *data, short fc_ifc_addr) {
  unsigned short rx_data_avail;
  unsigned short rx_err;
  unsigned short rx_req;

  // write fc and addr to taskram
  base[CALC_OFFS(slot) + MIL_SIO3_TX_TASK1] = fc_ifc_addr;

  // wait for task to start (tx fifo full or other tasks running)
  rx_req = base[CALC_OFFS(slot) + MIL_SIO3_TX_REQ];
  while(!(rx_req & 0x2)) {
    usleep(1);
    rx_req = base[CALC_OFFS(slot) + MIL_SIO3_TX_REQ];
  }

  // wait for task to finish, a read over the dev bus needs at least 40us
  rx_data_avail = base[CALC_OFFS(slot) + MIL_SIO3_D_RCVD];
  while(!(rx_data_avail & 0x2)) {
    usleep(1);
    rx_data_avail = base[CALC_OFFS(slot) + MIL_SIO3_D_RCVD];
  }

  // task finished
  rx_err = base[CALC_OFFS(slot) + MIL_SIO3_D_ERR];
  if ((rx_data_avail & 0x2) && !(rx_err & 0x2)) {
    // copy received value
    *data = 0xffff & base[CALC_OFFS(slot) + MIL_SIO3_RX_TASK1];
    return OKAY;
  } else {
    // dummy read resets available and error bits
    base[CALC_OFFS(slot) + MIL_SIO3_RX_TASK1];
    return RCV_TIMEOUT;
  }
}


/***********************************************************
 ***********************************************************
 * 
 * 2st part:  (new) MIL bus library
 *
 ***********************************************************
 ***********************************************************/
int16_t writeDevMil(volatile uint32_t *base, uint16_t  ifbAddr, uint16_t  fctCode, uint16_t  data)
{
  // just a wrapper for the function of the original library
  // replace code once original library becomes deprecated
  
  uint16_t fc_ifb_addr;

  fc_ifb_addr = ifbAddr | (fctCode << 8);

  return (int16_t)write_mil((unsigned int *)base, (short)data, (short)fc_ifb_addr);
} // writeDevMil

int16_t readDevMil(volatile uint32_t *base, uint16_t  ifbAddr, uint16_t  fctCode, uint16_t  *data)
{
  // just a wrapper for the function of the original library
  // replace code once original library becomes deprecated

  uint16_t fc_ifb_addr;

  fc_ifb_addr = ifbAddr | (fctCode << 8);

  return (int16_t)read_mil((unsigned int *)base, (short *)data, (short)fc_ifb_addr);
} //writeDevMil

int16_t echoTestDevMil(volatile uint32_t *base, uint16_t  ifbAddr, uint16_t data)
{
  int32_t  busStatus;
  uint16_t rData = 0x0;

  busStatus = writeDevMil(base, ifbAddr, FC_WR_IFC_ECHO, data);
  if (busStatus != MIL_STAT_OK) return busStatus;

  busStatus = readDevMil(base, ifbAddr, FC_RD_IFC_ECHO, &rData);
  if (busStatus != MIL_STAT_OK) return busStatus;

  if (data != rData) return MIL_STAT_ERROR;
  else               return MIL_STAT_OK;
} //echoTestDevMil


int16_t clearFilterEvtMil(volatile uint32_t *base)
{
  uint32_t filterSize;         // size of filter RAM     
  uint32_t *pFilterRAM;        // RAM for event filters
  uint32_t i;

  filterSize = (MIL_REG_EV_FILT_LAST >> 2) - (MIL_REG_EV_FILT_FIRST >> 2) + 1;
  // mprintf("filtersize: %d, base 0x%08x\n", filterSize, base);

  pFilterRAM = (uint32_t *)(base + (MIL_REG_EV_FILT_FIRST >> 2));      // address to filter RAM 
  for (i=0; i < filterSize; i++) pFilterRAM[i] = 0x0;

  // mprintf("&pFilterRAM[0]: 0x%08x, &pFilterRAM[filterSize-1]: 0x%08x\n", &(pFilterRAM[0]), &(pFilterRAM[filterSize-1]));

  return MIL_STAT_OK;
} //clearFiterEvtMil

int16_t setFilterEvtMil(volatile uint32_t *base, uint16_t evtCode, uint16_t virtAcc, uint32_t filter)
{
  uint32_t *pFilterRAM;        // RAM for event filters

  if (virtAcc > 15) return MIL_STAT_OUT_OF_RANGE;

  pFilterRAM = (uint32_t*)(base + (uint32_t)(MIL_REG_EV_FILT_FIRST >> 2));  // address to filter RAM 

  pFilterRAM[virtAcc*256+evtCode] = filter;

  // mprintf("pFilter: 0x%08x, &pFilter[evt_code*16+acc_number]: 0x%08x\n", pFilterRAM, &(pFilterRAM[evtCode*16+virtAcc]));

  return MIL_STAT_OK;
} //setFilterEvtMil

int16_t enableFilterEvtMil(volatile uint32_t *base)
{
  uint32_t regValue;

  readCtrlStatRegEvtMil(base, &regValue);
  regValue = regValue | MIL_CTRL_STAT_EV_FILTER_ON;
  writeCtrlStatRegEvtMil(base, regValue);
  
  return MIL_STAT_OK;
} //enableFilterEvtMil


int16_t disableFilterEvtMil(volatile uint32_t *base)
{
  uint32_t regValue;

  readCtrlStatRegEvtMil(base, &regValue);
  regValue = regValue & (MIL_CTRL_STAT_EV_FILTER_ON);
  writeCtrlStatRegEvtMil(base, regValue);
    
  return MIL_STAT_OK;
} // disableFilterEvtMil

int16_t writeCtrlStatRegEvtMil(volatile uint32_t *base, uint32_t value)
{
  uint32_t *pControlRegister;  // control register of event filter

  pControlRegister  = (uint32_t *)(base + (MIL_REG_WR_RD_STATUS >> 2));
  *pControlRegister = value;

  return MIL_STAT_OK;
} // writeCtrlStatRegMil

int16_t readCtrlStatRegEvtMil(volatile uint32_t *base, uint32_t *value)
{
  uint32_t *pControlRegister;  // control register of event filter

  pControlRegister  = (uint32_t *)(base + (MIL_REG_WR_RD_STATUS >> 2));
  *value = *pControlRegister;

  return MIL_STAT_OK;
} //readCtrlStatRegMil

uint16_t fifoNotemptyEvtMil(volatile uint32_t *base)
{
  uint32_t regValue;
  uint16_t fifoNotEmpty;

  readCtrlStatRegEvtMil(base, &regValue);
  fifoNotEmpty = (uint16_t)(regValue & MIL_CTRL_STAT_EV_FIFO_NE);
  
  return (fifoNotEmpty);
} // fifoNotemptyEvtMil

int16_t clearFifoEvtMil(volatile uint32_t *base)
{
  uint32_t *pFIFO;

  pFIFO = (uint32_t *)(base + (MIL_REG_RD_CLR_EV_FIFO >> 2));
  *pFIFO = 0x1; // check value!!!

  return MIL_STAT_OK;
} // clearFifoEvtMil

int16_t popFifoEvtMil(volatile uint32_t *base, uint32_t *evtData)
{
  uint32_t *pFIFO;

  pFIFO = (uint32_t *)(base + (MIL_REG_RD_CLR_EV_FIFO >> 2));

  *evtData = *pFIFO;
  
  return MIL_STAT_OK;
} // popFifoEvtMil

int16_t configLemoPulseEvtMil(volatile uint32_t *base, uint32_t lemo)
{
  uint32_t *pConfigRegister;

  uint32_t statRegValue;
  uint32_t confRegValue;
  
  if (lemo > 4) return MIL_STAT_OUT_OF_RANGE;

  // disable gate mode 
  readCtrlStatRegEvtMil(base, &statRegValue);
  if (lemo == 1) statRegValue = statRegValue & ~MIL_CTRL_STAT_PULS1_FRAME;
  if (lemo == 2) statRegValue = statRegValue & ~MIL_CTRL_STAT_PULS2_FRAME;
  writeCtrlStatRegEvtMil(base, statRegValue);

  // enable output
  pConfigRegister = (uint32_t *)(base + (MIL_REG_WR_RF_LEMO_CONF >> 2));
  confRegValue = *pConfigRegister;
  if (lemo == 1) confRegValue = confRegValue | MIL_LEMO_OUT_EN1 | MIL_LEMO_EVENT_EN1;
  if (lemo == 2) confRegValue = confRegValue | MIL_LEMO_OUT_EN2 | MIL_LEMO_EVENT_EN2;
  if (lemo == 3) confRegValue = confRegValue | MIL_LEMO_OUT_EN3 | MIL_LEMO_EVENT_EN3;
  if (lemo == 4) confRegValue = confRegValue | MIL_LEMO_OUT_EN4 | MIL_LEMO_EVENT_EN4;
  *pConfigRegister = confRegValue;

  return MIL_STAT_OK;
} // configLemoPulseEvtMil

int16_t configLemoGateEvtMil(volatile uint32_t *base, uint32_t lemo)
{
  uint32_t *pConfigRegister;

  uint32_t statRegValue;
  uint32_t confRegValue;

  if (lemo > 2) return MIL_STAT_OUT_OF_RANGE;
  
  // enable gate mode 
  readCtrlStatRegEvtMil(base, &statRegValue);
  if (lemo == 1) statRegValue = statRegValue | MIL_CTRL_STAT_PULS1_FRAME;
  if (lemo == 2) statRegValue = statRegValue | MIL_CTRL_STAT_PULS2_FRAME;
  writeCtrlStatRegEvtMil(base, statRegValue);

  // enable output
  pConfigRegister = (uint32_t *)(base + (MIL_REG_WR_RF_LEMO_CONF >> 2));
  confRegValue = *pConfigRegister;
  if (lemo == 1) confRegValue = confRegValue | MIL_LEMO_EVENT_EN1;
  if (lemo == 2) confRegValue = confRegValue | MIL_LEMO_EVENT_EN2;
  *pConfigRegister = confRegValue;
  
  return MIL_STAT_OK;  
} //enableLemoGateEvtMil

int16_t configLemoOutputEvtMil(volatile uint32_t *base, uint32_t lemo)
{
  uint32_t *pConfigRegister;

  uint32_t statRegValue;
  uint32_t confRegValue;
  
  if (lemo > 4) return MIL_STAT_OUT_OF_RANGE;

  // disable gate mode 
  readCtrlStatRegEvtMil(base, &statRegValue);
  if (lemo == 1) statRegValue = statRegValue & ~MIL_CTRL_STAT_PULS1_FRAME;
  if (lemo == 2) statRegValue = statRegValue & ~MIL_CTRL_STAT_PULS2_FRAME;
  writeCtrlStatRegEvtMil(base, statRegValue);

  // enable output for programable operation
  pConfigRegister = (uint32_t *)(base + (MIL_REG_WR_RF_LEMO_CONF >> 2));
  confRegValue = *pConfigRegister;
  if (lemo == 1) confRegValue = confRegValue | MIL_LEMO_OUT_EN1;
  if (lemo == 2) confRegValue = confRegValue | MIL_LEMO_OUT_EN2;
  if (lemo == 3) confRegValue = confRegValue | MIL_LEMO_OUT_EN3;
  if (lemo == 4) confRegValue = confRegValue | MIL_LEMO_OUT_EN4;
  *pConfigRegister = confRegValue;

  return MIL_STAT_OK; 
} //configLemoOutputEvtMil

int16_t setLemoOutputEvtMil(volatile uint32_t *base, uint32_t lemo, uint32_t on)
{
  uint32_t *pLemoDataRegister;

  uint32_t dataRegValue;

  if (lemo > 4) return MIL_STAT_OUT_OF_RANGE;
  if (on > 1)   return MIL_STAT_OUT_OF_RANGE;

  // read current value of register
  pLemoDataRegister = (uint32_t *)(base + (MIL_REG_WR_RD_LEMO_DAT >> 2));
  dataRegValue = *pLemoDataRegister;

  // modify value for register
  if (on) {
    if (lemo == 1) dataRegValue = dataRegValue | MIL_LEMO_OUT_EN1;
    if (lemo == 2) dataRegValue = dataRegValue | MIL_LEMO_OUT_EN2;
    if (lemo == 3) dataRegValue = dataRegValue | MIL_LEMO_OUT_EN3;
    if (lemo == 4) dataRegValue = dataRegValue | MIL_LEMO_OUT_EN4;
  } // if on
  else {
    if (lemo == 1) dataRegValue = dataRegValue & ~MIL_LEMO_OUT_EN1;
    if (lemo == 2) dataRegValue = dataRegValue & ~MIL_LEMO_OUT_EN2;
    if (lemo == 3) dataRegValue = dataRegValue & ~MIL_LEMO_OUT_EN3;
    if (lemo == 4) dataRegValue = dataRegValue & ~MIL_LEMO_OUT_EN4;
  } //else if on

  //write new value to register
  *pLemoDataRegister = dataRegValue;

  return MIL_STAT_OK;
} //setLemoOutputEvtMil


int16_t disableLemoEvtMil(volatile uint32_t *base, uint32_t lemo)
{
  uint32_t *pConfigRegister;

  uint32_t statRegValue;
  uint32_t confRegValue;

  if (lemo > 4) return MIL_STAT_OUT_OF_RANGE;

  // disable gate mode 
  readCtrlStatRegEvtMil(base, &statRegValue);
  if (lemo == 1) statRegValue = statRegValue & ~MIL_CTRL_STAT_PULS1_FRAME;
  if (lemo == 2) statRegValue = statRegValue & ~MIL_CTRL_STAT_PULS2_FRAME;
  writeCtrlStatRegEvtMil(base, statRegValue);

  // disable output
  pConfigRegister = (uint32_t *)(base + (MIL_REG_WR_RF_LEMO_CONF >> 2));
  confRegValue = *pConfigRegister;
  if (lemo == 1) confRegValue = confRegValue & ~MIL_LEMO_OUT_EN1 & ~MIL_LEMO_EVENT_EN1;
  if (lemo == 2) confRegValue = confRegValue & ~MIL_LEMO_OUT_EN2 & ~MIL_LEMO_EVENT_EN2;
  if (lemo == 3) confRegValue = confRegValue & ~MIL_LEMO_OUT_EN3 & ~MIL_LEMO_EVENT_EN3;
  if (lemo == 4) confRegValue = confRegValue & ~MIL_LEMO_OUT_EN4 & ~MIL_LEMO_EVENT_EN4;
  *pConfigRegister = confRegValue;

  return MIL_STAT_OK;
} // disableLemoEvtMil



#include <wr_mil_cmd.h>
#include "mini_sdb.h"
#include "mprintf.h"
#include "wr_mil_delay.h"
#include "wr_mil_eca_queue.h"
#include "wr_mil_events.h"

extern volatile uint32_t _startshared[]; // provided in linker script "ram.ld"
#define SHARED __attribute__((section(".shared")))
uint64_t SHARED dummy = UINT64_C(0); // not sure if that variable is really needed
                                     // the extern volatile uint32_t _startshared[] should be enough

#define CMD_FULL_STOP UINT32_C(1) // command to stop the LM32 from running
#define CMD_PAUSE_10S UINT32_C(2) // command to stop the LM32 for 10sec

volatile MilCmdRegs *MilCmd_init()
{
  volatile MilCmdRegs *cmd = (volatile MilCmdRegs*)_startshared;
  cmd->cmd               = UINT32_C(0);
  cmd->utc_trigger       = MIL_EVT_END_CYCLE;
  cmd->utc_delay         = 100;    //    100 us delay
  cmd->trigger_utc_delay = 100000; // 100000 us delay (100 ms) 
  cmd->event_source      = EVENT_SOURCE_NOT_CONFIGURED; // not configured by default
  return cmd;
}

// check if the command (cmd) register is != 0, 
//  do stuff according to its value, and set it 
//  back to 0
void MilCmd_poll(volatile MilCmdRegs *cmd)
{
  if (cmd->event_source == EVENT_SOURCE_NOT_CONFIGURED)
  {
    mprintf("event souce %d\n", cmd->event_source);
  }
  if (cmd->cmd)
  {
    switch(cmd->cmd)
    {
      case CMD_FULL_STOP:
        mprintf("stop MCU\n");
        while(1);
      case CMD_PAUSE_10S:
        mprintf("pause MCU for 10 sec\n");
        for (int i = 0; i < 10000; ++i) DELAY1000us;
        break;
      default:
        mprintf("found command %08x\n", cmd->cmd);
        break;
    }
    cmd->cmd = UINT32_C(0);
  }   
}


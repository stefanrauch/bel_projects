/*!
 * @file ecaMsiTest.c
 * @brief Test program for ECA -Interrupts
 *
 *        Prepared test for FreeRTOS outsourced from ecaMsiExample.c (D.Beck)
 *
 * @see https://www-acc.gsi.de/wiki/Timing/TimingSystemHowSoftCPUHandleECAMSIs
 * @copyright 2020 GSI Helmholtz Centre for Heavy Ion Research GmbH
 * @date 07.04.2020
 * @author Ulrich Becker <u.becker@gsi.de>
 *
 *
 *  This example demonstrates handling of message-signaled interrupts (MSI)
 *  caused by ECA channel.\n
 *  ECA is capable to send MSIs on certain conditions such as producing actions
 *  on reception of timing messages.\n
 *
 *  build: make clean && make TARGET=ecaMsiExample\n
 *  deploy: scp ecaMsiExample.bin root@scuxl4711.acc:.\n
 *  load: eb-fwload dev/wbm0 u 0x0 ecaMsiExample.bin \n
 *  run: eb-reset dev/wbm0 cpureset 0 (assume only one LM32 is instantiated)\n
 *  debug: eb-console dev/wbm0
 *
 *  To run example:
 *  - set ECA rules for eCPU action channel
 * @code
 *  saft-ecpu-ctl tr0 -d -c 0x1122334455667788 0xFFFFFFFFFFFFFFFF 0 0x42
 * @endcode
 *  - debug firmware output
 * @code
 *  eb-console dev/wbm0
 * @endcode
 *  - inject timing message (invoke on the second terminal)
 * @code
 *  saft-ctl -p tr0 inject 0x1122334455667788 0x8877887766556642 0
 * @endcode
 *
 ******************************************************************************/
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <stdint.h>

#include "eb_console_helper.h"
#include "scu_msi.h"
#include "eca_queue_type.h"


/*!
 * @brief ECA actions tagged for this LM32 CPU
 *
 * @code
 * saft-ecpu-ctl tr0 -d -c 0x1122334455667788 0xFFFFFFFFFFFFFFFF 0 0x42
 *                                                                 ====
 * @endcode
 */
#define MY_ACT_TAG 0x42

/*!
 * @brief WB address of ECA control register set.
 */
ECA_CONTROL_T* g_pEcaCtl;

/*!
 * @brief WB address of ECA queue
 */
ECA_QUEUE_ITEM_T* g_pEcaQueue;

/*! ---------------------------------------------------------------------------
 * @brief Clear pending valid actions for LM32
 */
void clearActions( void )
{
   uint32_t valCnt = ecaControlGetAndResetLM32ValidCount( g_pEcaCtl );
   if( valCnt != 0 )
   {
      mprintf( "pending actions: %d\n", valCnt );
      valCnt = ecaClearQueue( g_pEcaQueue, valCnt ); // pop pending actions
      mprintf( "cleared actions: %d\n", valCnt );
   }
}

/*! ---------------------------------------------------------------------------
 * @brief Configure ECA to send MSI to embedded soft-core LM32.
 *
 * - ECA action channel for LM32 is selected and MSI target address of LM32 is
 *   set in the ECA MSI target register
 *
 */
void configureEcaMsiForLM32( void )
{
   clearActions();   // clean ECA queue and channel from pending actions
   ecaControlSetMsiLM32TargetAddress( g_pEcaCtl, (void*)pMyMsi, true );
   mprintf( "MSI path (ECA -> LM32)           : enabled\n"
           "\tECA channel = %d\n\tdestination = 0x%08x\n",
            ECA_SELECT_LM32_CHANNEL, (uint32_t)pMyMsi);
}

/*! ---------------------------------------------------------------------------
 * @brief Pop pending embedded CPU actions from an ECA queue and handle them
 *
 * @param cnt The number of pending valid actions
 *
 */
STATIC void ecaHandler( const unsigned int cnt )
{
   for( unsigned int i = 0; i < cnt; i++ )
   {
      if( !ecaIsValid( g_pEcaQueue ) )
         continue;

      ECA_QUEUE_ITEM_T ecaItem = *g_pEcaQueue;

      ecaPop( g_pEcaQueue );

      /*
       * here: do something according to action
       */
      if( ecaItem.tag == MY_ACT_TAG )
      {
         mprintf( "ecaHandler: id: 0x%08x%08x\n"
                  "deadline:       0x%08x%08x\n"
                  "param:          0x%08x%08x\n"
                  "flag:           0x%08x\n",
                  ecaItem.eventIdH,  ecaItem.eventIdL,
                  ecaItem.deadlineH, ecaItem.deadlineL,
                  ecaItem.paramH,    ecaItem.paramL,
                  ecaItem.flags
                );
      }
      else
      {
         mprintf( "ecaHandler: unknown tag: %d\n", ecaItem.tag );
      }
   }
}

/*! ---------------------------------------------------------------------------
 * @brief Handle pending valid actions
 */
STATIC void handleValidActions( void )
{
   const uint32_t valCnt = ecaControlGetAndResetLM32ValidCount( g_pEcaCtl );
   mprintf( "valid:\t%d\n", valCnt );
   if( valCnt != 0 )
      ecaHandler( valCnt );  // pop pending valid actions
}

/*! ---------------------------------------------------------------------------
 * @brief Interrupt function: Handle MSIs sent by ECA
 *
 * If interrupt was caused by a valid action, then MSI has value of (4<<16|num).\n
 * Both ECA action channel and ECA queue connected to that channel must be handled:\n
 * - read and clear the valid counter value of ECA action channel for LM32 and,\n
 * - pop pending actions from ECA queue connected to this action channel
 */
STATIC void onIrqEcaEvent( const unsigned int intNum,
                           const void* pContext UNUSED )
{
   MSI_ITEM_T m;

   irqMsiCopyObjectAndRemove( &m, intNum );

   mprintf( "\nMSI:\t0x%08x\n"
              "Adr:\t0x%08x\n"
              "Sel:\t0x%02x\n",
              m.msg,
              m.adr,
              m.sel );

   if( (m.msg & ECA_VALID_ACTION) != 0 ) // valid actions are pending
      handleValidActions();              // ECA MSI handling
}

/*! ---------------------------------------------------------------------------
 * @brief Initializes and enables the interrupt.
 */
STATIC void initIrqTable( void )
{
   irqRegisterISR( ECA_INTERRUPT_NUMBER, NULL, onIrqEcaEvent );
   irqEnable();
   mprintf( "Init IRQ table is done.\n" );
}

/*! ---------------------------------------------------------------------------
 * @brief Initialization
 *
 * - discover WB devices
 * - init UART
 * - detect ECA control unit
 * - detect ECA queues
 */
STATIC void init( void )
{
   discoverPeriphery();    // mini-sdb: get info on important Wishbone infrastructure, such as (this) CPU, flash, ...
   uart_init_hw();         // init UART, required for printf... . To view print message, you may use 'eb-console' from the host

   mprintf( ESC_CLR_SCR ESC_XY( "1", "1" ) "--- Demo for ECA MSI handling ---\n");

   if( pEca != NULL )
     mprintf("ECA event input                  @ 0x%08x\n", (uint32_t) pEca);
   else
   {
      mprintf(ESC_ERROR"Could not find the ECA event input. Exit!\n");
      return;
   }

   mprintf("MSI destination addr for LM32    : 0x%08x\n", (uint32_t)pMyMsi);

   g_pEcaCtl = ecaControlGetRegisters();
   if( g_pEcaCtl != NULL )
      mprintf( "ECA channel control              @ 0x%08x\n",
               (uint32_t) g_pEcaCtl);
   else
   {
      mprintf(ESC_ERROR "Could not find the ECA channel control. Exit!\n");
      return;
   }

   g_pEcaQueue = ecaGetLM32Queue();
   if( g_pEcaQueue != NULL )
   {
      mprintf( "ECA queue to LM32 action channel @ 0x%08x\n",
               (uint32_t) g_pEcaQueue );
   }
   else
   {
      mprintf( ESC_ERROR "Could not find the ECA queue connected"
                         " to eCPU action channel. Exit!\n" );
      return;
   }
}

/* ==========================================================================*/
void main( void )
{
  init(); // get own MSI target addr, ECA event input and ECA queue for LM32 channel

  configureEcaMsiForLM32();
  initIrqTable();                 // set up MSI handler

  mprintf("waiting for MSI ...\n");

  /* main loop */
  while( true );

}

/* ================================= EOF ====================================*/

/*!
 *  @file daq_command_interface_uc.c
 *  @brief Definition of DAQ-commandos and data object for shared memory
 *         LM32 part
 *
 *  @date 27.02.2019
 *  @copyright (C) 2019 GSI Helmholtz Centre for Heavy Ion Research GmbH
 *
 *  @author Ulrich Becker <u.becker@gsi.de>
 *
 ******************************************************************************
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 ******************************************************************************
 */
#include <daq_command_interface_uc.h>
#include <scu_lm32_macros.h>
#include <daq_main.h>
#include <daq_ramBuffer.h>
#include <dbg.h>

/*!!!!!!!!!!!!!!!!!!!!!! Begin of shared memory area !!!!!!!!!!!!!!!!!!!!!!!!*/
volatile DAQ_SHARED_IO_T SHARED g_shared =
{
   .magicNumber = DAQ_MAGIC_NUMBER,
   .ramIndexes  = RAM_RING_SHARED_OBJECT_INITIALIZER,
   .operation =
   {
      .code    = DAQ_OP_IDLE,
      .retCode = DAQ_RET_OK
   }
};
/*!!!!!!!!!!!!!!!!!!!!!!! End of shared memory area !!!!!!!!!!!!!!!!!!!!!!!!!*/

typedef int32_t (*DAQ_OPERATION_FT)( DAQ_ADMIN_T* pDaqAdmin,
                                     volatile DAQ_OPERATION_IO_T* );

/*! ---------------------------------------------------------------------------
 * @ingroup DAQ_INTERFACE
 * @brief Definition of the item for the operation match list.
 */
typedef struct
{
   /*!
    * @brief Operation code
    * @see DAQ_OPERATION_CODE_T
    */
   DAQ_OPERATION_CODE_T code;
   /*!
    * @brief Pointer of the related function
    */
   DAQ_OPERATION_FT     operation;
} DAQ_OPERATION_TAB_ITEM_T;

/*! ---------------------------------------------------------------------------
 */
#ifdef DEBUGLEVEL
static void printFunctionName( const char* str )
{
   DBPRINT1( "DBG: executing %s(),\tDevice: %d, Channel: %d\n",
             str,
             g_shared.operation.ioData.location.deviceNumber,
             g_shared.operation.ioData.location.channel
           );
}
  #define FUNCTION_INFO() printFunctionName( __func__ )
#else
  #define FUNCTION_INFO()
#endif

/*! ---------------------------------------------------------------------------
 */
int initBuffer( RAM_SCU_T* poRam )
{
   return ramInit( poRam, (RAM_RING_SHARED_OBJECT_T*)&g_shared.ramIndexes );
}

/*! ---------------------------------------------------------------------------
 */
static int
verifyDeviceAccess( DAQ_BUS_T* pDaqBus,
                    volatile DAQ_CHANNEL_LOCATION_T* pLocation )
{
   if( (pLocation->deviceNumber == 0) || (pLocation->deviceNumber > DAQ_MAX) )
   {
      DBPRINT1( "DBG: DAQ_RET_ERR_SLAVE_OUT_OF_RANGE\n" );
      return DAQ_RET_ERR_SLAVE_OUT_OF_RANGE;
   }

   if( pLocation->deviceNumber > pDaqBus->foundDevices )
   {
      DBPRINT1( "DBG: DAQ_RET_ERR_SLAVE_NOT_PRESENT\n" );
      return DAQ_RET_ERR_SLAVE_NOT_PRESENT;
   }

   return DAQ_RET_OK;
}

/*! ---------------------------------------------------------------------------
 */
static int
verifyChannelAccess( DAQ_BUS_T* pDaqBus,
                     volatile DAQ_CHANNEL_LOCATION_T* pLocation )
{
   int ret = verifyDeviceAccess( pDaqBus, pLocation );
   if( ret != DAQ_RET_OK )
      return ret;

   if( (pLocation->channel == 0) || (pLocation->channel > DAQ_MAX_CHANNELS))
   {
      DBPRINT1( "DBG: DAQ_RET_ERR_CHANNEL_OUT_OF_RANGE\n" );
      return DAQ_RET_ERR_CHANNEL_OUT_OF_RANGE;
   }

   if( pLocation->channel > pDaqBus->aDaq[pLocation->deviceNumber-1].maxChannels )
   {
      DBPRINT1( "DBG: DAQ_RET_ERR_CHANNEL_NOT_PRESENT\n" );
      return DAQ_RET_ERR_CHANNEL_NOT_PRESENT;
   }

   return DAQ_RET_OK;
}

/*! ---------------------------------------------------------------------------
 */
static
int32_t opLock( DAQ_ADMIN_T* pDaqAdmin, volatile DAQ_OPERATION_IO_T* pData )
{
   FUNCTION_INFO();
   return DAQ_RET_OK;
}

/*! ---------------------------------------------------------------------------
 */
static
int32_t opUnlock( DAQ_ADMIN_T* pDaqAdmin, volatile DAQ_OPERATION_IO_T* pData )
{
   FUNCTION_INFO();
   return DAQ_RET_OK;
}


/*! ---------------------------------------------------------------------------
 */
static
int32_t opReset( DAQ_ADMIN_T* pDaqAdmin, volatile DAQ_OPERATION_IO_T* pData )
{
   FUNCTION_INFO();
   daqBusReset( &pDaqAdmin->oDaqDevs );
   ramRingReset( &pDaqAdmin->oRam.pSharedObj->ringIndexes );
   return DAQ_RET_OK;
}

/*! ---------------------------------------------------------------------------
 */
static
int32_t opGetMacroVersion( DAQ_ADMIN_T* pDaqAdmin,
                           volatile DAQ_OPERATION_IO_T* pData )
{
   FUNCTION_INFO();
   int ret = verifyDeviceAccess( &pDaqAdmin->oDaqDevs, &pData->location );
   if( ret != DAQ_RET_OK )
      return ret;

   pData->param1 = daqDeviceGetMacroVersion(
                   &pDaqAdmin->oDaqDevs.aDaq[pData->location.deviceNumber-1] );

   return DAQ_RET_OK;
}

/*! ---------------------------------------------------------------------------
 */
static int32_t opGetSlots( DAQ_ADMIN_T* pDaqAdmin,
                           volatile DAQ_OPERATION_IO_T* pData )
{
   FUNCTION_INFO();
   STATIC_ASSERT( sizeof( pData->param1 ) >= sizeof(pDaqAdmin->oDaqDevs.slotDaqUsedFlags));

   pData->param1 = pDaqAdmin->oDaqDevs.slotDaqUsedFlags;

   return DAQ_RET_OK;
}

/*! ---------------------------------------------------------------------------
 */
static int32_t opGetChannels( DAQ_ADMIN_T* pDaqAdmin,
                              volatile DAQ_OPERATION_IO_T* pData )
{
   FUNCTION_INFO();
   int ret = verifyDeviceAccess( &pDaqAdmin->oDaqDevs, &pData->location );
   if( ret != DAQ_RET_OK )
      return ret;

   pData->param1 = pDaqAdmin->oDaqDevs.aDaq[pData->location.deviceNumber-1].maxChannels;

   return DAQ_RET_OK;
}

/*! ---------------------------------------------------------------------------
 */
static int32_t opRescan( DAQ_ADMIN_T* pDaqAdmin,
                         volatile DAQ_OPERATION_IO_T* pData )
{
   FUNCTION_INFO();
   scanScuBus( &pDaqAdmin->oDaqDevs );
   return DAQ_RET_RESCAN;
}

/*! ---------------------------------------------------------------------------
 */
static inline
DAQ_CANNEL_T* getChannel( DAQ_ADMIN_T* pDaqAdmin,
                          volatile DAQ_OPERATION_IO_T* pData )
{
   return &pDaqAdmin->oDaqDevs.aDaq
            [pData->location.deviceNumber-1].aChannel[pData->location.channel-1];
}

/*! ---------------------------------------------------------------------------
 */
static int32_t opPostMortemOn( DAQ_ADMIN_T* pDaqAdmin,
                               volatile DAQ_OPERATION_IO_T* pData )
{
   FUNCTION_INFO();

   int ret = verifyChannelAccess( &pDaqAdmin->oDaqDevs, &pData->location );
   if( ret != DAQ_RET_OK )
      return ret;

   DAQ_CANNEL_T* pChannel = getChannel( pDaqAdmin, pData );
   pChannel->properties.restart = (pData->param1 != 0);
   daqChannelEnablePostMortem( pChannel );

   return DAQ_RET_OK;
}

/*! ---------------------------------------------------------------------------
 */
static int32_t opHighResolutionOn( DAQ_ADMIN_T* pDaqAdmin,
                                   volatile DAQ_OPERATION_IO_T* pData )
{
   FUNCTION_INFO();

   int ret = verifyChannelAccess( &pDaqAdmin->oDaqDevs, &pData->location );
   if( ret != DAQ_RET_OK )
      return ret;

   daqChannelEnableHighResolution( getChannel( pDaqAdmin, pData ) );

   return DAQ_RET_OK;
}

/*! ---------------------------------------------------------------------------
 */
static int32_t opPmHighResOff( DAQ_ADMIN_T* pDaqAdmin,
                               volatile DAQ_OPERATION_IO_T* pData )
{
   FUNCTION_INFO();

   int ret = verifyChannelAccess( &pDaqAdmin->oDaqDevs, &pData->location );
   if( ret != DAQ_RET_OK )
      return ret;

   DAQ_CANNEL_T* pChannel = getChannel( pDaqAdmin, pData );
   pChannel->properties.restart = (pData->param1 != 0);
   daqChannelDisablePostMortem( pChannel );
   daqChannelDisableHighResolution( pChannel );
   return DAQ_RET_OK;
}

/*! ---------------------------------------------------------------------------
 */
static int32_t opContinueOn( DAQ_ADMIN_T* pDaqAdmin,
                             volatile DAQ_OPERATION_IO_T* pData )
{
   FUNCTION_INFO();

   int ret = verifyChannelAccess( &pDaqAdmin->oDaqDevs, &pData->location );
   if( ret != DAQ_RET_OK )
      return ret;

   DAQ_CANNEL_T* pChannel = getChannel( pDaqAdmin, pData );

   pChannel->blockDownCounter = pData->param2;
   DBPRINT1( "DBG: blockDownCounter = %d\n", pChannel->blockDownCounter );

   switch( (DAQ_SAMPLE_RATE_T)pData->param1 )
   {
      case DAQ_SAMPLE_1MS:
      {
         DBPRINT1( "DBG: 1 ms sample ON\n" );
         daqChannelSample1msOn( pChannel );
         break;
      }
      case DAQ_SAMPLE_100US:
      {
         DBPRINT1( "DBG: 100 us sample ON\n" );
         daqChannelSample100usOn( pChannel );
         break;
      }
      case DAQ_SAMPLE_10US:
      {
         DBPRINT1( "DBG: 10 us sample ON\n" );
         daqChannelSample10usOn( pChannel );
         break;
      }
      default:
      {
         return DAQ_RET_ERR_WRONG_SAMPLE_PARAMETER;
      }
   }

   return DAQ_RET_OK;
}

/*! ---------------------------------------------------------------------------
 */
static int32_t opContinueOff( DAQ_ADMIN_T* pDaqAdmin,
                              volatile DAQ_OPERATION_IO_T* pData )
{
   FUNCTION_INFO();

   int ret = verifyChannelAccess( &pDaqAdmin->oDaqDevs, &pData->location );
   if( ret != DAQ_RET_OK )
      return ret;

   DAQ_CANNEL_T* pChannel = getChannel( pDaqAdmin, pData );

   daqChannelSample10usOff( pChannel );
   daqChannelSample100usOff( pChannel );
   daqChannelSample1msOff( pChannel );

   return DAQ_RET_OK;
}

/*! ---------------------------------------------------------------------------
 */
static int32_t opSetTriggerCondition( DAQ_ADMIN_T* pDaqAdmin,
                                      volatile DAQ_OPERATION_IO_T* pData )
{
   FUNCTION_INFO();
   int ret = verifyChannelAccess( &pDaqAdmin->oDaqDevs, &pData->location );
   if( ret != DAQ_RET_OK )
      return ret;

   DAQ_CANNEL_T* pChannel = getChannel( pDaqAdmin, pData );
   daqChannelSetTriggerConditionLW( pChannel, pData->param1 );
   daqChannelSetTriggerConditionHW( pChannel, pData->param2 );
   return DAQ_RET_OK;
}

/*! ---------------------------------------------------------------------------
 */
static int32_t opGetTriggerCondition( DAQ_ADMIN_T* pDaqAdmin,
                                      volatile DAQ_OPERATION_IO_T* pData )
{
   FUNCTION_INFO();
   int ret = verifyChannelAccess( &pDaqAdmin->oDaqDevs, &pData->location );
   if( ret != DAQ_RET_OK )
      return ret;

   DAQ_CANNEL_T* pChannel = getChannel( pDaqAdmin, pData );
   pData->param1 = daqChannelGetTriggerConditionLW( pChannel );
   pData->param2 = daqChannelGetTriggerConditionHW( pChannel );
   return DAQ_RET_OK;
}


/*! ---------------------------------------------------------------------------
 */
static int32_t opSetTriggerDelay( DAQ_ADMIN_T* pDaqAdmin,
                                      volatile DAQ_OPERATION_IO_T* pData )
{
   FUNCTION_INFO();
   int ret = verifyChannelAccess( &pDaqAdmin->oDaqDevs, &pData->location );
   if( ret != DAQ_RET_OK )
      return ret;

   daqChannelSetTriggerDelay( getChannel( pDaqAdmin, pData ), pData->param1 );
   return DAQ_RET_OK;
}

/*! ---------------------------------------------------------------------------
 */
static int32_t opGetTriggerDelay( DAQ_ADMIN_T* pDaqAdmin,
                                      volatile DAQ_OPERATION_IO_T* pData )
{
   FUNCTION_INFO();
   int ret = verifyChannelAccess( &pDaqAdmin->oDaqDevs, &pData->location );
   if( ret != DAQ_RET_OK )
      return ret;

   pData->param1 = daqChannelGetTriggerDelay( getChannel( pDaqAdmin, pData ) );
   return DAQ_RET_OK;
}

/*! ---------------------------------------------------------------------------
 */
static int32_t opSetTriggerMode( DAQ_ADMIN_T* pDaqAdmin,
                                 volatile DAQ_OPERATION_IO_T* pData )
{
   FUNCTION_INFO();
   int ret = verifyChannelAccess( &pDaqAdmin->oDaqDevs, &pData->location );
   if( ret != DAQ_RET_OK )
      return ret;

   DAQ_CANNEL_T* pChannel = getChannel( pDaqAdmin, pData );

   if( pData->param1 != 0 )
      daqChannelEnableTriggerMode( pChannel );
   else
      daqChannelDisableTriggerMode( pChannel );

   return DAQ_RET_OK;
}

/*! ---------------------------------------------------------------------------
 */
static int32_t opGetTriggerMode( DAQ_ADMIN_T* pDaqAdmin,
                                 volatile DAQ_OPERATION_IO_T* pData )
{
   FUNCTION_INFO();
   int ret = verifyChannelAccess( &pDaqAdmin->oDaqDevs, &pData->location );
   if( ret != DAQ_RET_OK )
      return ret;
   pData->param1 = daqChannelIsTriggerModeEnabled( getChannel( pDaqAdmin, pData ) );
   return DAQ_RET_OK;
}

/*! ---------------------------------------------------------------------------
 */
static int32_t opSetTriggerSourceCon( DAQ_ADMIN_T* pDaqAdmin,
                                      volatile DAQ_OPERATION_IO_T* pData )
{
   FUNCTION_INFO();
   int ret = verifyChannelAccess( &pDaqAdmin->oDaqDevs, &pData->location );
   if( ret != DAQ_RET_OK )
      return ret;

   DAQ_CANNEL_T* pChannel = getChannel( pDaqAdmin, pData );

   if( pData->param1 != 0 )
      daqChannelEnableExtrenTrigger( pChannel );
   else
      daqChannelEnableEventTrigger( pChannel );

   return DAQ_RET_OK;
}

/*! ---------------------------------------------------------------------------
 */
static int32_t opGetTriggerSourceCon( DAQ_ADMIN_T* pDaqAdmin,
                                      volatile DAQ_OPERATION_IO_T* pData )
{
   FUNCTION_INFO();
   int ret = verifyChannelAccess( &pDaqAdmin->oDaqDevs, &pData->location );
   if( ret != DAQ_RET_OK )
      return ret;

   pData->param1 = daqChannelGetTriggerSource( getChannel( pDaqAdmin, pData ) );

   return DAQ_RET_OK;
}

/*! ---------------------------------------------------------------------------
 */
static int32_t opSetTriggerSourceHir( DAQ_ADMIN_T* pDaqAdmin,
                                      volatile DAQ_OPERATION_IO_T* pData )
{
   FUNCTION_INFO();
   int ret = verifyChannelAccess( &pDaqAdmin->oDaqDevs, &pData->location );
   if( ret != DAQ_RET_OK )
      return ret;

   DAQ_CANNEL_T* pChannel = getChannel( pDaqAdmin, pData );

   if( pData->param1 != 0 )
      daqChannelEnableExternTriggerHighRes( pChannel );
   else
      daqChannelEnableEventTriggerHighRes( pChannel );

   return DAQ_RET_OK;
}

/*! ---------------------------------------------------------------------------
 */
static int32_t opGetTriggerSourceHir( DAQ_ADMIN_T* pDaqAdmin,
                                      volatile DAQ_OPERATION_IO_T* pData )
{
   FUNCTION_INFO();
   int ret = verifyChannelAccess( &pDaqAdmin->oDaqDevs, &pData->location );
   if( ret != DAQ_RET_OK )
      return ret;

   pData->param1 = daqChannelGetTriggerSourceHighRes( getChannel( pDaqAdmin, pData ) );

   return DAQ_RET_OK;
}


#define OPERATION_ITEM( opcode, function ) \
   { .code = opcode, .operation = function }

/*!
 * @ingroup DAQ_INTERFACE
 * @brief Last item of the operation match list.
 */
#define OPERATION_ITEM_TERMINATOR OPERATION_ITEM( DAQ_OP_IDLE, NULL )

/*! ---------------------------------------------------------------------------
 * @ingroup DAQ_INTERFACE
 * @brief Operation match list
 */
static const DAQ_OPERATION_TAB_ITEM_T g_operationTab[] =
{
   OPERATION_ITEM( DAQ_OP_LOCK,                   opLock                ),
   OPERATION_ITEM( DAQ_OP_UNLOCK,                 opUnlock              ),
   OPERATION_ITEM( DAQ_OP_RESET,                  opReset               ),
   OPERATION_ITEM( DAQ_OP_GET_MACRO_VERSION,      opGetMacroVersion     ),
   OPERATION_ITEM( DAQ_OP_GET_SLOTS,              opGetSlots            ),
   OPERATION_ITEM( DAQ_OP_GET_CHANNELS,           opGetChannels         ),
   OPERATION_ITEM( DAQ_OP_RESCAN,                 opRescan              ),
   OPERATION_ITEM( DAQ_OP_PM_ON,                  opPostMortemOn        ),
   OPERATION_ITEM( DAQ_OP_HIRES_ON,               opHighResolutionOn    ),
   OPERATION_ITEM( DAQ_OP_PM_HIRES_OFF,           opPmHighResOff        ),
   OPERATION_ITEM( DAQ_OP_CONTINUE_ON,            opContinueOn          ),
   OPERATION_ITEM( DAQ_OP_CONTINUE_OFF,           opContinueOff         ),
   OPERATION_ITEM( DAQ_OP_SET_TRIGGER_CONDITION,  opSetTriggerCondition ),
   OPERATION_ITEM( DAQ_OP_GET_TRIGGER_CONDITION,  opGetTriggerCondition ),
   OPERATION_ITEM( DAQ_OP_SET_TRIGGER_DELAY,      opSetTriggerDelay     ),
   OPERATION_ITEM( DAQ_OP_GET_TRIGGER_DELAY,      opGetTriggerDelay     ),
   OPERATION_ITEM( DAQ_OP_SET_TRIGGER_MODE,       opSetTriggerMode      ),
   OPERATION_ITEM( DAQ_OP_GET_TRIGGER_MODE,       opGetTriggerMode      ),
   OPERATION_ITEM( DAQ_OP_SET_TRIGGER_SOURCE_CON, opSetTriggerSourceCon ),
   OPERATION_ITEM( DAQ_OP_GET_TRIGGER_SOURCE_CON, opGetTriggerSourceCon ),
   OPERATION_ITEM( DAQ_OP_SET_TRIGGER_SOURCE_HIR, opSetTriggerSourceHir ),
   OPERATION_ITEM( DAQ_OP_GET_TRIGGER_SOURCE_HIR, opGetTriggerSourceHir ),

   OPERATION_ITEM_TERMINATOR
};

/*! ---------------------------------------------------------------------------
 */
bool executeIfRequested( DAQ_ADMIN_T* pDaqAdmin )
{
   /*
    * Requests the Linux host an operation?
    */
   if( g_shared.operation.code == DAQ_OP_IDLE )
   { /*
      * No, there is nothing to do...
      */
      return false;
   }

   /*
    * Yes, executing the requested operation if present
    * in the operation table.
    */
   unsigned int i = 0;
   while( g_operationTab[i].operation != NULL )
   {
      if( g_operationTab[i].code == g_shared.operation.code )
      {
         g_shared.operation.retCode =
            g_operationTab[i].operation( pDaqAdmin,
                                         &g_shared.operation.ioData );
         break;
      }
      i++;
   }

   /*
    * Was the requested operation known?
    */
   if( g_operationTab[i].operation == NULL )
   { /*
      * No, making known this for the Linux host.
      */
      DBPRINT1( "DBG: DAQ_RET_ERR_UNKNOWN_OPERATION\n" );
      g_shared.operation.retCode = DAQ_RET_ERR_UNKNOWN_OPERATION;
   }

   bool ret;
   if( g_shared.operation.retCode == DAQ_RET_RESCAN )
   {
      g_shared.operation.retCode = DAQ_RET_OK;
      ret = true;
   }
   else
      ret = false;

   /*
    * Making known for the Linux host that this application is ready
    * for the next operation.
    */
   g_shared.operation.code = DAQ_OP_IDLE;
   return ret;
}

/*================================== EOF ====================================*/

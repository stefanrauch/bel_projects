/*!
 * @file scu_mil_fg_handler.c
 * @brief Module for handling all MIL function generators and MIL DAQs
 * @copyright GSI Helmholtz Centre for Heavy Ion Research GmbH
 * @author Ulrich Becker <u.becker@gsi.de>
 * @date 04.02.2020
 * Outsourced from scu_main.c
 */

#ifndef __DOCFSM__
  #include "scu_fg_macros.h"
  #include "scu_mil_fg_handler.h"
  #include "scu_fg_list.h"
  #ifdef CONFIG_MIL_DAQ_USE_RAM
    #include <daq_main.h>
  #endif
#endif

#ifdef CONFIG_MIL_DAQ_USE_RAM
extern DAQ_ADMIN_T g_scuDaqAdmin;
#endif

extern volatile uint16_t*     g_pScub_base;
extern volatile unsigned int* g_pScu_mil_base;

#ifdef _CONFIG_VARIABLE_MIL_GAP_READING
   unsigned int g_gapReadingTime = DEFAULT_GAP_READING_INTERVAL;
#endif

/*!
 * @brief Slot-value when no slave selected yet.
 */
#define INVALID_SLAVE_NR ((unsigned int)~0)

#ifdef CONFIG_READ_MIL_TIME_GAP
  #define _GAP_TIME_INIT .gapReadingTime = 0LL,
#else
  #define _GAP_TIME_INIT
#endif

QUEUE_CREATE_STATIC( g_queueMilSio, MAX_FG_CHANNELS, QUEUE_MIL_SOCKET_T );
QUEUE_CREATE_STATIC( g_queueMilBus, MAX_FG_CHANNELS, QUEUE_MIL_SOCKET_T );

/*! ---------------------------------------------------------------------------
 * @ingroup MIL_FSM
 * @brief Initializer of a single MIL task
 * @see g_aMilTaskData
 */
#define MIL_TASK_DATA_ITEM_INITIALIZER      \
{                                           \
   .state            = ST_WAIT,             \
   .slave_nr         = INVALID_SLAVE_NR,    \
   .lastChannel      = 0,                   \
   .task_timeout_cnt = 0,                   \
   .timestamp1       = 0LL,                 \
   _GAP_TIME_INIT                           \
   .aFgChannels =                           \
   {{                                       \
      .irq_data         = 0,                \
      .setvalue         = 0,                \
      .daq_timestamp    = 0LL               \
   }}                                       \
}

/*! ---------------------------------------------------------------------------
 * @ingroup MIL_FSM
 * @brief Memory space and pre-initializing of MIL-task data.
 */
MIL_TASK_DATA_T g_aMilTaskData[5] =
{
   MIL_TASK_DATA_ITEM_INITIALIZER,
   MIL_TASK_DATA_ITEM_INITIALIZER,
   MIL_TASK_DATA_ITEM_INITIALIZER,
   MIL_TASK_DATA_ITEM_INITIALIZER,
   MIL_TASK_DATA_ITEM_INITIALIZER
};

#ifndef __DOXYGEN__
STATIC_ASSERT( TASKMAX >= (ARRAY_SIZE( g_aMilTaskData ) + MAX_FG_CHANNELS-1 + TASKMIN));

/*
 * Mil-library uses "short" rather than "uint16_t"! :-(
 */
STATIC_ASSERT( sizeof( short ) == sizeof( int16_t ) );
#endif

//#define _CONFIG_DBG_TIMESTAMP

#ifdef _CONFIG_DBG_TIMESTAMP
#warning Debug of MIL-Timestamps is active!
volatile uint64_t g_aLastTimestamps[ ARRAY_SIZE( g_aMilTaskData[0].aFgChannels ) ];
#endif

/*! ----------------------------------------------------------------------------
 */
void milInitTasks( void )
{
   for( unsigned int i = 0; i < ARRAY_SIZE( g_aMilTaskData ); i++ )
   {
      g_aMilTaskData[i].state             = ST_WAIT;
      g_aMilTaskData[i].slave_nr          = INVALID_SLAVE_NR;
      g_aMilTaskData[i].lastChannel       = 0;
      g_aMilTaskData[i].task_timeout_cnt  = 0;
      g_aMilTaskData[i].timestamp1        = 0LL;
   #ifdef CONFIG_READ_MIL_TIME_GAP
      g_aMilTaskData[i].gapReadingTime    = 0LL;
   #endif
      for( unsigned int j = 0; j < ARRAY_SIZE( g_aMilTaskData[0].aFgChannels ); j++ )
      {
         g_aMilTaskData[i].aFgChannels[j].irq_data      = 0;
         g_aMilTaskData[i].aFgChannels[j].setvalue      = 0;
         g_aMilTaskData[i].aFgChannels[j].daq_timestamp = 0LL;
      }
   }
#ifdef CONFIG_MIL_DAQ_USE_RAM
   ramRingReset( &g_shared.mDaq.indexes );
   g_shared.mDaq.wasRead = 0;
#endif
#ifdef _CONFIG_DBG_TIMESTAMP
   for( unsigned int i = 0; i < ARRAY_SIZE( g_aLastTimestamps ); i++ )
      g_aLastTimestamps[i] = 0LL;
#endif
}

/*! ---------------------------------------------------------------------------
 * @ingroup MIL_FSM
 * @brief Converts the states of the FSM in strings.
 * @note For debug purposes only!
 */
STATIC const char* state2string( const FG_STATE_T state )
{
   #define __CASE_RETURN( s ) case s: return #s
   switch( state )
   {
      __CASE_RETURN( ST_WAIT );
      __CASE_RETURN( ST_PREPARE );
      __CASE_RETURN( ST_FETCH_STATUS );
      __CASE_RETURN( ST_HANDLE_IRQS );
      __CASE_RETURN( ST_DATA_AQUISITION );
      __CASE_RETURN( ST_FETCH_DATA );
   }
   return "unknown";
   #undef __CASE_RETURN
}


#ifdef _CONFIG_DBG_MIL_TASK
/*! ---------------------------------------------------------------------------
 * @brief For debug purposes only!
 */
void dbgPrintMilTaskData( void )
{
   for( unsigned int i = 0; i < ARRAY_SIZE( g_aMilTaskData ); i++ )
   {
      mprintf( "FSM-state[%u]: %s\n",          i, state2string( g_aMilTaskData[i].state ));
      mprintf( "slave_nr[%u]: 0x%08X\n",       i, g_aMilTaskData[i].slave_nr );
      mprintf( "lastChannel[%u]: %u\n",        i, g_aMilTaskData[i].lastChannel );
      mprintf( "task_timeout_cnt[%u]: %u\n",   i, g_aMilTaskData[i].task_timeout_cnt );
      mprintf( "timestamp1[%u]: 0x%08X%08X\n", i, (uint32_t)GET_UPPER_HALF(g_aMilTaskData[i].timestamp1),
                                                  (uint32_t)GET_LOWER_HALF(g_aMilTaskData[i].timestamp1) );
   #ifdef CONFIG_READ_MIL_TIME_GAP
      mprintf( "gapReadingTime[%u]: %08X%08X\n", i, (uint32_t)GET_UPPER_HALF(g_aMilTaskData[i].gapReadingTime),
                                                    (uint32_t)GET_LOWER_HALF(g_aMilTaskData[i].gapReadingTime) );
   #endif
      for( unsigned int j = 0; j < ARRAY_SIZE( g_aMilTaskData[0].aFgChannels ); j++ )
      {
         mprintf( "\tirq_data[%u][%u]: 0x%04X\n", i, j, g_aMilTaskData[i].aFgChannels[j].irq_data );
         mprintf( "\tsetvalue[%u][%u]: %u\n", i, j, g_aMilTaskData[i].aFgChannels[j].setvalue );
         mprintf( "\tdaq_timestamp[%u][%u]: 0x%08X%08X\n", i, j,
                   (uint32_t)GET_UPPER_HALF(g_aMilTaskData[i].aFgChannels[j].daq_timestamp),
                   (uint32_t)GET_LOWER_HALF(g_aMilTaskData[i].aFgChannels[j].daq_timestamp) );
      }
   }
}
#endif /* ifdef _CONFIG_DBG_MIL_TASK */

/*! ---------------------------------------------------------------------------
 * @ingroup MIL_FSM
 * @brief Prints a possible MIL-Bus error.
 */
STATIC void printMilError( const int status, const int slave_nr )
{
   #define __CASE_ITEM( s ) case s: errStr = #s; break
   char* errStr = '\0';
   switch( status )
   {
      __CASE_ITEM( TRM_NOT_FREE );
      __CASE_ITEM( RCV_PARITY );
      __CASE_ITEM( RCV_TIMEOUT );
      __CASE_ITEM( RCV_ERROR );
      default: break;
   }
   #undef __CASE_ITEM
   mprintf( ESC_ERROR "MIL-Error: \"%s\" %d, slave: %d\n" ESC_NORMAL,
            errStr, status, slave_nr );
}

/*! ---------------------------------------------------------------------------
 * @see scu_mil_fg_handler.h
 */
void fgMilClearHandlerState( const unsigned int socket )
{
   if( isMilScuBusFg( socket ) )
   {
      FG_ASSERT( getFgSlotNumber( socket ) > 0 );
      const QUEUE_MIL_SOCKET_T slot = getFgSlotNumber( socket );
     /*
      * Triggering of a software pseudo interrupt.
      */
      ATOMIC_SECTION() pushInQueue( &g_queueMilSio, &slot );
      return;
   }

   if( isMilExtentionFg( socket ) )
   {
      const QUEUE_MIL_SOCKET_T slot = 0;
     /*
      * Triggering of a software pseudo interrupt.
      */
      ATOMIC_SECTION() pushInQueue( &g_queueMilBus, &slot );
   }
}

#if defined( CONFIG_READ_MIL_TIME_GAP ) && !defined(__DOCFSM__)
/*! ---------------------------------------------------------------------------
 * @see scu_mil_fg_handler.h.h
 */
bool isMilFsmInST_WAIT( void )
{
   for( unsigned int i = 0; i < ARRAY_SIZE( g_aMilTaskData ); i++ )
   {
      if( g_aMilTaskData[i].state != ST_WAIT )
         return false;
   }
   return true;
}

/*! ---------------------------------------------------------------------------
 * @see scu_mil_fg_handler.h.h
 */
void suspendGapReading( void )
{
   for( unsigned int i = 0; i < ARRAY_SIZE( g_aMilTaskData ); i++ )
      g_aMilTaskData[i].slave_nr = INVALID_SLAVE_NR;
}
#endif // if defined( CONFIG_READ_MIL_TIME_GAP ) && !defined(__DOCFSM__)

/*! ---------------------------------------------------------------------------
 * @ingroup MIL_FSM
 * @brief Returns the task-id-number of the given MIL-task data object.
 * @param pMilTaskData Pointer to the currently MIL-task-data object.
 */
inline STATIC unsigned int getMilTaskId( const MIL_TASK_DATA_T* pMilTaskData )
{
   return (((unsigned int)pMilTaskData) - ((unsigned int)g_aMilTaskData))
                                             / sizeof( MIL_TASK_DATA_T );
}

/*! ---------------------------------------------------------------------------
 * @ingroup MIL_FSM
 * @brief Returns the task number of a mil device.
 * @see https://www-acc.gsi.de/wiki/bin/viewauth/Hardware/Intern/PerfOpt
 * @param pMilTaskData Pointer to the currently running system task.
 * @param channel Channel number
 */
inline STATIC unsigned char getMilTaskNumber( const MIL_TASK_DATA_T* pMilTaskData,
                                              const unsigned int channel )
{
   //!!return TASKMIN + getFgMacroIndexFromFgRegister( channel );
   return TASKMIN + channel + getMilTaskId( pMilTaskData );
}


//#define CONFIG_LAGE_TIME_DETECT
#if defined( CONFIG_MIL_DAQ_USE_RAM ) && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
  STATIC_ASSERT( sizeof( FG_MACRO_T ) == sizeof(uint32_t) );
  IMPLEMENT_CONVERT_BYTE_ENDIAN( FG_MACRO_T )
#endif

/*! ---------------------------------------------------------------------------
 * @ingroup MIL_FSM
 * @ingroup TASK
 * @brief Writes the data set coming from one of the MIL-DAQs in the
 *        ring-buffer.
 * @see daq_eb_ram_buffer.hpp
 * @see daq_eb_ram_buffer.cpp
 * @param channel DAQ-channel where the data come from.
 * @param timestamp White-Rabbit time-stamp.
 * @param actValue Actual value.
 * @param setValue Set-value.
 * @param setValueInvalid If true, the set value is invalid.
 * @todo Storing the MIL-DAQ data in the DDR3-RAM instead wasting of
 *       shared memory.
 */
STATIC void pushDaqData( FG_MACRO_T fgMacro,
                         const uint64_t timestamp,
                         const uint16_t actValue,
                         const uint32_t setValue
                      #ifdef CONFIG_READ_MIL_TIME_GAP
                         , const bool setValueInvalid
                      #endif
                       )
{
#ifdef CONFIG_LAGE_TIME_DETECT
   static uint64_t lastTime = 0;
   static unsigned int count = 0;
   if( lastTime > 0 )
   {
      if( (timestamp - lastTime) > 100000000ULL )
         mprintf( ESC_WARNING "Time-gap: %d" ESC_NORMAL "\n", count++ );
   }
   lastTime = timestamp;
#endif

#ifdef CONFIG_READ_MIL_TIME_GAP
   if( setValueInvalid )
      fgMacro.outputBits |= SET_VALUE_NOT_VALID_MASK;
#endif

#ifdef CONFIG_MIL_DAQ_USE_RAM
   MIL_DAQ_RAM_ITEM_PAYLOAD_T pl;
 #if (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__) || defined(__DOXYGEN__)
   /*
    * In this case the LM32-server and Linux-client doesn't have the same
    * byte order.
    * The Linux library "libetherbone" will made a byte swapping of 32-bit
    * segments during reading out the DDR3-RAM.
    * Therefore the appropriate byte order has to be prepared here.
    * E.g. the 16 bit set- and actual values are reversed, that is correct.
    * See also daq_eb_ram_buffer.hpp and daq_eb_ram_buffer.cpp.
    */
   pl.item.timestamp = MERGE_HIGH_LOW( GET_LOWER_HALF( timestamp ), (uint32_t)GET_UPPER_HALF( timestamp ) );
   pl.item.setValue  = actValue;
   pl.item.actValue  = GET_UPPER_HALF( setValue );
   pl.item.fgMacro   = convertByteEndian_FG_MACRO_T( fgMacro );
 #else
   /*
    * In this case the server and client have the same byte order.
    */
   pl.item.timestamp = timestamp;
   pl.item.setValue  = GET_UPPER_HALF( setValue );
   pl.item.actValue  = actValue;
   pl.item.fgMacro   = fgMacro;
 #endif

   RAM_RING_INDEXES_T indexes = g_shared.mDaq.indexes;

   /*
    * Is the circular buffer full?
    */
   if( ramRingGetRemainingCapacity( &indexes ) < ARRAY_SIZE(pl.ramPayload) )
   { /*
      * Yes, removing the oldest item;
      */
      ramRingAddToReadIndex( &indexes, ARRAY_SIZE(pl.ramPayload) );
   }

   /*
    * Writing the prepared data set into the DDR3-RAM organized
    * as circular buffer.
    */
   for( unsigned int i = 0; i < ARRAY_SIZE(pl.ramPayload); i++ )
   {
      ramWriteItem( &g_scuDaqAdmin.oRam, ramRingGetWriteIndex( &indexes ), &pl.ramPayload[i] );
      ramRingIncWriteIndex( &indexes );
   }

   g_shared.mDaq.indexes = indexes;

#else /* ifdef CONFIG_MIL_DAQ_USE_RAM */
   #warning Deprecated: MIL-DAQ data will stored in the LM32 shared memory!
   MIL_DAQ_OBJ_T d;

   d.actvalue = actValue;
   d.tmstmp_l = GET_LOWER_HALF( timestamp );
   d.tmstmp_h = GET_UPPER_HALF( timestamp );
   d.fgMacro  = fgMacro;
   d.setvalue = setValue;
   add_daq_msg( &g_shared.daq_buf, d );
#endif /* else ifdef CONFIG_MIL_DAQ_USE_RAM */
}

/*! ---------------------------------------------------------------------------
 * @ingroup MIL_FSM
 * @brief Helper function printing a timeout message.
 */
STATIC void printTimeoutMessage( register MIL_TASK_DATA_T* pMilTaskData,
                                 const bool isScuBus )
{
   mprintf( ESC_WARNING "timeout %s: state %s, taskid %d index %d" ESC_NORMAL"\n",
            isScuBus? "dev_bus_handler" : "dev_sio_handler",
            state2string( pMilTaskData->state ),
            getMilTaskId( pMilTaskData ),
            pMilTaskData->lastChannel );
}

#ifndef __DOXYGEN__
/*
 * A little bit of paranoia doesn't hurt too much. ;-)
 */
STATIC_ASSERT( MAX_FG_CHANNELS == ARRAY_SIZE( g_aMilTaskData[0].aFgChannels ) );
STATIC_ASSERT( MAX_FG_CHANNELS == ARRAY_SIZE( g_aFgChannels ));
#endif

/*! ---------------------------------------------------------------------------
 * @ingroup MIL_FSM
 * @brief function returns true when no interrupt of the given channel
 *        is pending.
 *
 * Helper-function for function milDeviceHandler
 * @param pMilTaskData pointer to the current MIL-task-data object
 * @param channel channel number of function generator
 * @retval true No interrupt pending
 * @retval false Any interrupt pending
 * @see milDeviceHandler
 * @see milReqestStatus milGetStatus
 */
ALWAYS_INLINE STATIC inline
bool isNoIrqPending( register const MIL_TASK_DATA_T* pMilTaskData,
                                                 const unsigned int channel )
{
   return
   (pMilTaskData->aFgChannels[channel].irq_data & (DEV_STATE_IRQ | DEV_DRQ)) == 0;
}

/*! ---------------------------------------------------------------------------
 * @ingroup MIL_FSM
 * @brief Requests the current status of the MIL device.
 * @see https://www-acc.gsi.de/wiki/bin/viewauth/Hardware/Intern/PerfOpt
 * @param pMilTaskData pointer to the current MIL-task-data object
 * @param isScuBus if true via SCU bus MIL adapter
 * @param channel channel number of function generator
 * @return MIL-device status
 * @see milDeviceHandler
 */
STATIC inline
int milReqestStatus( register MIL_TASK_DATA_T* pMilTaskData,
                     const bool isScuBus,
                     const unsigned int channel )
{
   FG_ASSERT( pMilTaskData->slave_nr != INVALID_SLAVE_NR );
   const unsigned int socket     = getSocket( channel );
   const unsigned int devAndMode = getDevice( channel ) | FC_IRQ_ACT_RD;
   const unsigned int milTaskNo  = getMilTaskNumber( pMilTaskData, channel );
   pMilTaskData->aFgChannels[channel].irq_data = 0; // clear old irq data
   /* test only if as connected to sio */
   if( isScuBus )
   {
      if( getFgSlotNumber( socket ) != pMilTaskData->slave_nr )
         return OKAY;

      if( !isMilScuBusFg( socket ) )
         return OKAY;

      return scub_set_task_mil( g_pScub_base, pMilTaskData->slave_nr,
                                                    milTaskNo, devAndMode );
   }

   if( !isMilExtentionFg( socket ) )
       return OKAY;

   return set_task_mil( g_pScu_mil_base, milTaskNo, devAndMode );
}

/*! ---------------------------------------------------------------------------
 * @ingroup MIL_FSM
 * @brief Reads the currently status of the MIL device back.
 * @see https://www-acc.gsi.de/wiki/bin/viewauth/Hardware/Intern/PerfOpt
 * @param pMilTaskData pointer to the current MIL-task-data object
 * @param isScuBus if true via SCU bus MIL adapter
 * @param channel channel number of function generator
 * @return MIL-device status
 * @see milDeviceHandler
 */
STATIC inline
int milGetStatus( register MIL_TASK_DATA_T* pMilTaskData, const bool isScuBus,
                                                  const unsigned int channel )
{
   FG_ASSERT( pMilTaskData->slave_nr != INVALID_SLAVE_NR );
   const unsigned int  socket = getSocket( channel );
   const unsigned char milTaskNo = getMilTaskNumber( pMilTaskData, channel );
    /* test only if as connected to sio */
   if( isScuBus )
   {
      if( getFgSlotNumber( socket ) != pMilTaskData->slave_nr )
         return OKAY;
      if( !isMilScuBusFg( socket ) )
         return OKAY;

      return scub_get_task_mil( g_pScub_base, pMilTaskData->slave_nr,  milTaskNo,
                                &pMilTaskData->aFgChannels[channel].irq_data );
   }

   if( !isMilExtentionFg( socket ) )
      return OKAY;
   return get_task_mil( g_pScu_mil_base, milTaskNo,
                                &pMilTaskData->aFgChannels[channel].irq_data );
}

/*! ---------------------------------------------------------------------------
 * @brief Supplies the by "devNum" and "socket" addressed MIL function
 *        generator with new data.
 */
STATIC inline void feedMilFg( const unsigned int socket,
                              const unsigned int devNum,
                              const FG_CTRL_RG_T cntrl_reg,
                              signed int* pSetvalue )
{
   const unsigned int channel = cntrl_reg.bv.number;

   if( channel >= ARRAY_SIZE( g_aFgChannels ) )
   {
      mprintf( ESC_ERROR "%s: FG-number %d out of range!" ESC_NORMAL "\n",
               __func__, channel );
      return;
   }

   FG_PARAM_SET_T pset;
   /*
    * Reading circular buffer with new FG-data.
    */
   if( !cbRead( &g_shared.oSaftLib.oFg.aChannelBuffers[0],
                &g_shared.oSaftLib.oFg.aRegs[0], channel, &pset ) )
   {
      hist_addx(HISTORY_XYZ_MODULE, "buffer empty, no parameter sent", socket);
      return;
   }

   /*
    * Setting the set value of MIL-DAQ
    */
   *pSetvalue = pset.coeff_c;

   FG_MIL_REGISTER_T milFgRegs;
   /*
    * clear freq, step select, fg_running and fg_enabled
    */
   setMilFgRegs( &milFgRegs, &pset, (cntrl_reg.i16 & ~(0xfc07)) |
                                    (pset.control.i32 & 0x3F) << 10) ;
   int status;
 #if __GNUC__ >= 9
   #pragma GCC diagnostic push
   #pragma GCC diagnostic ignored "-Waddress-of-packed-member"
 #endif
   if( isMilExtentionFg( socket ) )
   { /*
      * Send FG-data via MIL-extention adapter.
      */
      status = write_mil_blk( g_pScu_mil_base, (short*)&milFgRegs,
                              FC_BLK_WR | devNum );
   }
   else
   { /*
      * Send FG-data via SCU-bus-slave MIL adapter "SIO"
      */
      status = scub_write_mil_blk( g_pScub_base, getFgSlotNumber( socket ),
                                   (short*)&milFgRegs, FC_BLK_WR | devNum );
   }
 #if __GNUC__ >= 9
   #pragma GCC diagnostic pop
 #endif
   if( status != OKAY )
   {
      milPrintDeviceError( status, getFgSlotNumber( socket ), __func__ );
      return;
   }
#ifdef CONFIG_USE_SENT_COUNTER
   g_aFgChannels[channel].param_sent++;
#endif
}

/*! ---------------------------------------------------------------------------
 * @brief Handling of a MIL function generator.
 * @see handleAdacFg
 */
STATIC inline
void handleMilFg( const unsigned int socket,
                  const unsigned int devNum,
                  const uint16_t irq_act_reg,
                  signed int* pSetvalue )
{
   FG_ASSERT( !isAddacFg( socket ) );
   const FG_CTRL_RG_T ctrlReg = { .i16 = irq_act_reg };
   const unsigned int channel = ctrlReg.bv.number;
   if( channel >= ARRAY_SIZE( g_shared.oSaftLib.oFg.aRegs ) )
   {
      mprintf( ESC_ERROR "%s: Channel out of range: %d\n" ESC_NORMAL,
               __func__, channel );
      return;
   }

   if( !ctrlReg.bv.isRunning )
   { /*
      * Send signal to SAFT-lib
      */
      makeStop( channel );
      return;
   }

   /*
    * The hardware of the MIL function generators doesn't have a ramp-counter
    * integrated, so this task will made by the software here.
    */
   g_shared.oSaftLib.oFg.aRegs[channel].ramp_count++;

   if( ctrlReg.bv.devStateIrq )
   { /*
      * Send signal to SAFT-lib
      */
      makeStart( channel );
   }

   if( ctrlReg.bv.devStateIrq || ctrlReg.bv.devDrq )
   { /*
      * Send signal to SAFT-lib.
      */
      sendRefillSignalIfThreshold( channel );

     /*
      * Send data via MIL-bis to function generator.
      */
      feedMilFg( socket, devNum, ctrlReg, pSetvalue );
   }
}

/*! ---------------------------------------------------------------------------
 * @ingroup MIL_FSM
 * @brief Writes data to the MIL function generator
 * @param pMilTaskData pointer to the current MIL-task-data object
 * @param isScuBus if true via SCU bus MIL adapter
 * @param channel channel number of function generator
 * @return MIL-device status
 * @see milDeviceHandler
 */
STATIC inline
int milHandleAndWrite( register MIL_TASK_DATA_T* pMilTaskData,
                       const bool isScuBus,
                       const unsigned int channel )
{
   FG_ASSERT( pMilTaskData->slave_nr != INVALID_SLAVE_NR );

   const unsigned int dev = getDevice( channel );

   handleMilFg( getSocket( channel ),
                dev,
                pMilTaskData->aFgChannels[channel].irq_data,
                &(pMilTaskData->aFgChannels[channel].setvalue) );
   /*
    * clear irq pending and end block transfer
    */
   if( isScuBus )
   {
      return scub_write_mil( g_pScub_base, pMilTaskData->slave_nr,
                                                    0,  dev | FC_IRQ_ACT_WR );
   }
   return write_mil( g_pScu_mil_base, 0, dev | FC_IRQ_ACT_WR );
}

/*! ---------------------------------------------------------------------------
 * @ingroup MIL_FSM
 * @brief Set the read task of the MIL device
 * @see https://www-acc.gsi.de/wiki/bin/viewauth/Hardware/Intern/PerfOpt
 * @param pMilTaskData pointer to the current MIL-task-data object
 * @param isScuBus if true via SCU bus MIL adapter
 * @param channel channel number of function generator
 * @return MIL-device status
 * @see milDeviceHandler
 */
STATIC inline
int milSetTask( register MIL_TASK_DATA_T* pMilTaskData, const bool isScuBus,
                const unsigned int channel )
{
   FG_ASSERT( pMilTaskData->slave_nr != INVALID_SLAVE_NR );
   const unsigned int  devAndMode = getDevice( channel ) | FC_ACT_RD;
   const unsigned char milTaskNo  = getMilTaskNumber( pMilTaskData, channel );
   if( isScuBus )
   {
      return scub_set_task_mil( g_pScub_base, pMilTaskData->slave_nr,
                                                      milTaskNo, devAndMode );
   }
   return set_task_mil( g_pScu_mil_base, milTaskNo, devAndMode );
}

#ifndef __DOXYGEN__
STATIC_ASSERT( sizeof(short) == sizeof(int16_t) );
#endif

/*! ---------------------------------------------------------------------------
 * @ingroup MIL_FSM
 * @brief Reads the actual ADC value from MIL-device
 * @see https://www-acc.gsi.de/wiki/bin/viewauth/Hardware/Intern/PerfOpt
 * @param pMilTaskData pointer to the current MIL-task-data object
 * @param isScuBus if true via SCU bus MIL adapter
 * @param channel channel number of function generator
 * @param pActAdcValue Pointer in which shall the ADC-value copied
 * @return MIL-device status
 * @see milDeviceHandler
 */
STATIC inline
int milGetTask( register MIL_TASK_DATA_T* pMilTaskData, const bool isScuBus,
                const unsigned int channel, int16_t* pActAdcValue )
{
   FG_ASSERT( pMilTaskData->slave_nr != INVALID_SLAVE_NR );
   const unsigned char milTaskNo = getMilTaskNumber( pMilTaskData, channel );
   if( isScuBus )
   {
      return scub_get_task_mil( g_pScub_base, pMilTaskData->slave_nr,
                                                   milTaskNo, pActAdcValue );
   }
   return get_task_mil( g_pScu_mil_base, milTaskNo, pActAdcValue );
}

/*! ---------------------------------------------------------------------------
 * @ingroup MIL_FSM
 * @brief Macro performs a FSM transition. \n
 *        Helper macro for documenting the FSM by the FSM-visualizer DOCFSM.
 * @see milDeviceHandler
 * @see https://github.com/UlrichBecker/DocFsm
 */
#define FSM_TRANSITION( newState, attr... ) pMilData->state = newState

/*! ---------------------------------------------------------------------------
 * @ingroup MIL_FSM
 * @brief Macro documenting a FSM transition to the same state. \n
 *        Helper dummy macro for documenting the FSM by the FSM-visualizer
 *        DOCFSM.
 * @see milDeviceHandler
 * @see https://github.com/UlrichBecker/DocFsm
 */
#define FSM_TRANSITION_SELF( attr... )

/*! ---------------------------------------------------------------------------
 * @ingroup MIL_FSM
 * @brief Initializer for Finite-State-Machines. \n
 *        Helper macro for documenting the FSM by the FSM-visualizer DOCFSM.
 * @see milDeviceHandler
 * @see https://github.com/UlrichBecker/DocFsm
 */
#define FSM_INIT_FSM( init, attr... )       pMilData->state = init

/*! ---------------------------------------------------------------------------
 * @ingroup MIL_FSM
 * @brief Loop control macro for all present function generator channels
 *        started form given channel in parameter "start"
 *
 * Helper-macro for function milDeviceHandler
 *
 * It works off the FG-list initialized by function scan_all_fgs() in file
 * scu_function_generator.c from the in parameter start given channel number.
 *
 * @see isFgPresent
 * @see milDeviceHandler
 * @param channel current channel number, shall be of type unsigned int.
 * @param start Channel number to start.
 */
#define FOR_EACH_FG_CONTINUING( channel, start ) \
   for( channel = start; isFgPresent( channel ); channel++ )

/*! ---------------------------------------------------------------------------
 * @ingroup MIL_FSM
 * @brief Loop control macro for all present function generator channels.
 *
 * It works off the entire FG-list initialized by function scan_all_fgs() in
 * file scu_function_generator.c
 *
 * Helper-macro for function milDeviceHandler
 *
 * @see isFgPresent
 * @see milDeviceHandler
 * @param channel current channel number, shall be of type unsigned int.
 */
#define FOR_EACH_FG( channel ) FOR_EACH_FG_CONTINUING( channel, 0 )

/*! ---------------------------------------------------------------------------
 * @ingroup MIL_FSM
 * @brief Task-function for handling all MIL-FGs and MIL-DAQs via FSM.
 * @param pThis pointer to the current task object
 * @param isScuBus if true via SCU bus MIL adapter called SCU_SIO.
 * @see https://www-acc.gsi.de/wiki/Hardware/Intern/ScuSio
 * @see https://www-acc.gsi.de/wiki/bin/viewauth/Hardware/Intern/PerfOpt
 *
 * @dotfile scu_mil_fg_handler.gv State graph for this function
 * @see https://github.com/UlrichBecker/DocFsm
 *
 * @todo When gap-reading is activated (compiler switch CONFIG_READ_MIL_TIME_GAP
 *       is defined) so the danger of jittering could be exist! \n
 *       Solution proposal: Linux-host resp. SAFTLIB shall send a
 *       "function-generator-announcement-signal", before the function generator
 *       issued a new analog signal.
 */
STATIC void milDeviceHandler( register TASK_T* pThis, const bool isScuBus )
{
   /*!
    * @brief Currently channel number for loop-macros FOR_EACH_FG and
    *        FOR_EACH_FG_CONTINUING
    * @see FOR_EACH_FG
    * @see FOR_EACH_FG_CONTINUING
    */
   unsigned int channel;

   /*!
    * @brief Return value of the MIL-access functions.
    */
   int status = OKAY;

  /*
   * Checking integrity of pointer when macro FG_ASSERT is activated, that means
   * CONFIG_FG_PEDANTIC_CHECK is defined.
   */
   FG_ASSERT( pThis->pTaskData != NULL );
   FG_ASSERT( (unsigned int)pThis->pTaskData >= (unsigned int)&g_aMilTaskData[0] );
   FG_ASSERT( (unsigned int)pThis->pTaskData <= (unsigned int)&g_aMilTaskData[ARRAY_SIZE(g_aMilTaskData)-1] );

   /*!
    * @brief Pointer to the currently MIL-task.
    */
   MIL_TASK_DATA_T* pMilData = (MIL_TASK_DATA_T*) pThis->pTaskData;

#ifdef CONFIG_MIL_DAQ_USE_RAM
   /*
    * Removing old data which has been possibly read by the Linux client.
    * See m_daq_administration.cpp function: DaqAdministration::distributeDataNew
    */
   ramRingAddToReadIndex( &g_shared.mDaq.indexes, g_shared.mDaq.wasRead );
   g_shared.mDaq.wasRead = 0;
#endif

   const FG_STATE_T lastState = pMilData->state;

  /*
   * Performing the FSM state-do activities.
   */
   switch( lastState )
   {
      case ST_WAIT:
      {
         STATIC_ASSERT( sizeof(pMilData->slave_nr) == sizeof(QUEUE_MIL_SOCKET_T) );
         if( queuePopSave( isScuBus? &g_queueMilSio : &g_queueMilBus, &pMilData->slave_nr ) )
         {
         #ifdef CONFIG_READ_MIL_TIME_GAP
           /*
            * Sets the gap reading time to zero this will signal the host that
            * the next data aren't from gap reading.
            * Refer state ST_FETCH_DATA at function call pushDaqData().
            */
            pMilData->gapReadingTime = 0;
         #endif
            pMilData->timestamp1 = getWrSysTime() + INTERVAL_200US;
            FSM_TRANSITION( ST_PREPARE, label='Massage received', color=green );
            break;
         }
      #ifdef CONFIG_READ_MIL_TIME_GAP
        /*
         * Only a task which has already served a function generator
         * can read a time-gap. That means its slave number has to be valid.
         */
         if(
           #ifdef _CONFIG_VARIABLE_MIL_GAP_READING
             ( g_gapReadingTime != 0 ) &&
           #endif
             ( pMilData->slave_nr != INVALID_SLAVE_NR ) &&
             ( getWrSysTime() >= pMilData->gapReadingTime )
           )
         {
            FSM_TRANSITION( ST_DATA_AQUISITION, label='Gap reading time\nexpired',
                                                color=magenta );
            break;
         }
      #endif /* ifdef CONFIG_READ_MIL_TIME_GAP */
         FSM_TRANSITION_SELF( label='No message', color=blue );
         break;
      } /* end case ST_WAIT */

      case ST_PREPARE:
      { /*
         * wait for 200 us
         */
         if( getWrSysTime() < pMilData->timestamp1 )
         {
            FSM_TRANSITION_SELF( label='200 us not expired', color=blue );
            break;
         }
         /*
          * poll all pending regs on the dev bus; non blocking read operation
          */
         FOR_EACH_FG( channel )
         {
            status = milReqestStatus( pMilData, isScuBus, channel );
            if( status != OKAY )
               milPrintDeviceError( status, 20, "dev_sio set task" );
         }
         FSM_TRANSITION( ST_FETCH_STATUS, color=green );
         break;
      }

      case ST_FETCH_STATUS:
      { /*
         * if timeout reached, proceed with next task
         */
         if( pMilData->task_timeout_cnt > TASK_TIMEOUT )
         {
            printTimeoutMessage( pMilData, isScuBus );
         #ifdef CONFIG_GOTO_STWAIT_WHEN_TIMEOUT
            FSM_TRANSITION( ST_WAIT, label='maximum timeout-count\nreached',
                                     color=red );
            break;
         #else
            /*
             * skipping the faulty channel
             */
            pMilData->lastChannel++;
            pMilData->task_timeout_cnt = 0;
         #endif
         }
         /*
          * fetch status from dev bus controller;
          */
         FOR_EACH_FG_CONTINUING( channel, pMilData->lastChannel )
         {
            status = milGetStatus( pMilData, isScuBus, channel );
            if( status == RCV_TASK_BSY )
               break; /* break from FOR_EACH_FG_CONTINUING loop */
            if( status != OKAY )
               printMilError( status, pMilData->slave_nr );
         }
         if( status == RCV_TASK_BSY )
         {
            pMilData->lastChannel = channel; // start next time from channel
            pMilData->task_timeout_cnt++;
            FSM_TRANSITION_SELF( label='Receiving busy', color=blue );
            break;
         }
         FSM_TRANSITION( ST_HANDLE_IRQS, color=green );
         break;
      } /* end case ST_FETCH_STATUS*/

      case ST_HANDLE_IRQS:
      { /*
         * handle irqs for ifas with active pending regs; non blocking write
         */
         FOR_EACH_FG( channel )
         {
            if( isNoIrqPending( pMilData, channel ) )
            { /*
               * Handle next channel...
               */
               continue;
            }
            status = milHandleAndWrite( pMilData, isScuBus, channel );
            if( status != OKAY )
               milPrintDeviceError( status, 22, "dev_sio end handle");
         }
         FSM_TRANSITION( ST_DATA_AQUISITION, color=green );
         break;
      } /* end case ST_HANDLE_IRQS */

      case ST_DATA_AQUISITION:
      {
         FOR_EACH_FG( channel )
         {
            if( isNoIrqPending( pMilData, channel ) )
            { /*
               * Handle next channel...
               */
               continue;
            }
            /*
             * Store the sample timestamp of DAQ.
             */
            pMilData->aFgChannels[channel].daq_timestamp = getWrSysTimeSafe(); //getWrSysTime();

            status = milSetTask( pMilData, isScuBus, channel );
            if( status != OKAY )
               milPrintDeviceError( status, 23, "dev_sio read daq" );
         }
         FSM_TRANSITION( ST_FETCH_DATA, color=green );
         break;
      } /* end case ST_DATA_AQUISITION */

      case ST_FETCH_DATA:
      { /*
         * if timeout reached, proceed with next task
         */
         if( pMilData->task_timeout_cnt > TASK_TIMEOUT )
         {
            printTimeoutMessage( pMilData, isScuBus );
         #ifdef CONFIG_GOTO_STWAIT_WHEN_TIMEOUT
            FSM_TRANSITION( ST_WAIT, label='maximum timeout-count\nreached',
                                     color=blue );
            break;
         #else
            /*
             * skipping the faulty channel
             */
            pMilData->lastChannel++;
            pMilData->task_timeout_cnt = 0;
         #endif
         }
        /*
         * fetch DAQ data
         */
         FOR_EACH_FG_CONTINUING( channel, pMilData->lastChannel )
         {
            if( isNoIrqPending( pMilData, channel ) )
            { /*
               * Handle next channel...
               */
               continue;
            }
            int16_t actAdcValue;
            status = milGetTask( pMilData, isScuBus, channel, &actAdcValue );
            if( status == RCV_TASK_BSY )
               break; /* break from FOR_EACH_FG_CONTINUING loop */

            if( status != OKAY )
            {
               printMilError( status, pMilData->slave_nr );
              /*
               * Handle next channel...
               */
               continue;
            }
            pushDaqData( getFgMacroViaFgRegister( channel ),
                         pMilData->aFgChannels[channel].daq_timestamp,
                         actAdcValue,
                         g_aFgChannels[channel].last_c_coeff
                      #ifdef CONFIG_READ_MIL_TIME_GAP
                         , pMilData->gapReadingTime != 0
                      #endif
                       );
         #ifdef _CONFIG_DBG_TIMESTAMP
            if( g_aLastTimestamps[channel] < pMilData->aFgChannels[channel].daq_timestamp )
               g_aLastTimestamps[channel] = pMilData->aFgChannels[channel].daq_timestamp;
            else
               mprintf( "*\n" );
         #endif
            /*
             * save the setvalue from the tuple sent for the next drq handling
             */
            g_aFgChannels[channel].last_c_coeff = pMilData->aFgChannels[channel].setvalue;
         } /* end FOR_EACH_FG_CONTINUING */

         if( status == RCV_TASK_BSY )
         { /*
            * Start next time from channel
            */
            pMilData->lastChannel = channel;
            pMilData->task_timeout_cnt++;
            FSM_TRANSITION_SELF( label='Receiving busy', color=blue );
            break;
         }
         FSM_TRANSITION( ST_WAIT, color=green );
         break;
      } /* end case ST_FETCH_DATA */

      default: /* Should never be reached! */
      {
         mprintf( ESC_ERROR "Unknown FSM-state of %s(): %d !\n" ESC_NORMAL,
                  __func__, pMilData->state );
         FSM_INIT_FSM( ST_WAIT, label='Initializing', color=blue );
         break;
      }
   } /* End of state-do activities */

   /*
    * Has the FSM-state changed?
    */
   if( lastState == pMilData->state )
   { /*
      * No, there is nothing more to do, leave this function.
      */
      return;
   }

   /*
    *    *** The FSM-state has changed! ***
    *
    * Performing FSM-state-transition activities if necessary,
    * respectively here the state-entry activities.
    */
   switch( pMilData->state )
   {
   #ifdef CONFIG_READ_MIL_TIME_GAP
      case ST_WAIT:
      {
      #ifdef _CONFIG_VARIABLE_MIL_GAP_READING
         pMilData->gapReadingTime = getWrSysTime() + INTERVAL_1MS * g_gapReadingTime;
      #else
         pMilData->gapReadingTime = getWrSysTime() + INTERVAL_10MS;
      #endif
         break;
      }
   #endif

      case ST_FETCH_STATUS: /* Go immediately to next case. */
      case ST_FETCH_DATA:
      { /*
         * start next time from channel 0
         */
         pMilData->lastChannel = 0;
         pMilData->task_timeout_cnt = 0;
         break;
      }

      default: break;
   } /* End of state entry activities */
} /* End function milDeviceHandler */

/*! ---------------------------------------------------------------------------
 * @ingroup TASK
 * @ingroup MIL_FSM
 * @brief can have multiple instances, one for each active sio card controlling
 * a dev bus persistent data, like the state or the sio slave_nr, is stored in
 * a global structure
 * @param pThis pointer to the current task object
 * @see schedule
 * @see https://www-acc.gsi.de/wiki/Hardware/Intern/ScuSio
 */
void dev_sio_handler( register TASK_T* pThis )
{
   milDeviceHandler( pThis, true ); //false );
}

/*! ---------------------------------------------------------------------------
 * @ingroup TASK
 * @ingroup MIL_FSM
 * @brief has only one instance
 * persistent data is stored in a global structure
 * @param pThis pointer to the current task object
 * @see schedule
 */
void dev_bus_handler( register TASK_T* pThis )
{
   milDeviceHandler( pThis, false ); //true );
}


/*================================== EOF ====================================*/

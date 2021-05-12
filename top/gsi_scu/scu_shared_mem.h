/*!
 *  @file scu_shared_mem.h
 *  @brief Definition of shared memory for communication of
 *         function generator between LM32 and Linux host
 *
 *  @see https://www-acc.gsi.de/wiki/Hardware/Intern/ScuFgDoc
 *  @date 10.07.2019
 *  @copyright (C) 2019 GSI Helmholtz Centre for Heavy Ion Research GmbH
 *
 *  @author Ulrich Becker <u.becker@gsi.de>
 *
 *  @todo Include this file in "saftlib/drivers/fg_regs.h" and
 *        replace or define the constants and offset-addresses defined
 *        in fg_regs.h by the constants and offest-addresses defined
 *        in this file, reducing dangerous redundancy.\n
 *        <b>That is highly recommended!</b>\n
 *        The dependencies building "SAFTLIB" has to be also depend on this
 *        file!\n
 *        Find a well defined place for this file, where the front end group
 *        AND the timing group can access.
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
#ifndef _SCU_SHARED_MEM_H
#define _SCU_SHARED_MEM_H

/*!
 * @defgroup SHARED_MEMORY Sheard memory for communication between
 *                         LM32 and Linux.
 */

#include <helper_macros.h>
#include <scu_mailbox.h>
#include <scu_function_generator.h>
#include <scu_circular_buffer.h>
#ifdef CONFIG_SCU_DAQ_INTEGRATION
  #include <daq_command_interface.h>
#else
  #ifdef CONFIG_MIL_DAQ_USE_RAM
    #include <daq_ramBuffer.h>
  #endif
#endif

#ifdef __cplusplus
extern "C"
{
namespace Scu
{
namespace FG
{
#endif

#ifdef CONFIG_MIL_DAQ_USE_RAM
/*!
 * @brief Data type for set and actual values of MIL-DAQs
 */
typedef uint16_t MIL_DAQ_T;

/*!
 * @brief Data set of a MIL-DAQ in the DDR3-RAM.
 */
typedef struct PACKED_SIZE
{
   uint64_t   timestamp;
   MIL_DAQ_T  setValue;
   MIL_DAQ_T  actValue;
   FG_MACRO_T fgMacro;
} MIL_DAQ_RAM_ITEM_T;

#ifndef __DOXYGEN__
STATIC_ASSERT( offsetof( MIL_DAQ_RAM_ITEM_T, timestamp ) == 0 );
STATIC_ASSERT( offsetof( MIL_DAQ_RAM_ITEM_T, setValue ) == sizeof(uint64_t) );
STATIC_ASSERT( offsetof( MIL_DAQ_RAM_ITEM_T, actValue ) ==
               offsetof( MIL_DAQ_RAM_ITEM_T, setValue ) + sizeof(MIL_DAQ_T) );
STATIC_ASSERT( offsetof( MIL_DAQ_RAM_ITEM_T, channel ) ==
               offsetof( MIL_DAQ_RAM_ITEM_T, actValue ) + sizeof(MIL_DAQ_T) );
STATIC_ASSERT( sizeof( MIL_DAQ_RAM_ITEM_T ) ==
                offsetof( MIL_DAQ_RAM_ITEM_T, channel ) + sizeof(uint32_t) );
#endif /* ifndef __DOXYGEN__ */

/*!
 * @brief Number of required RAM-items per Mil-DAQ item
 */
#define RAM_ITEM_PER_MIL_DAQ_ITEM                                   \
   (sizeof( MIL_DAQ_RAM_ITEM_T ) / sizeof( RAM_DAQ_PAYLOAD_T ) +    \
    !!(sizeof( MIL_DAQ_RAM_ITEM_T ) % sizeof( RAM_DAQ_PAYLOAD_T )))

typedef union PACKED_SIZE
{
   RAM_DAQ_PAYLOAD_T  ramPayload[RAM_ITEM_PER_MIL_DAQ_ITEM];
   MIL_DAQ_RAM_ITEM_T item;
} MIL_DAQ_RAM_ITEM_PAYLOAD_T;

#endif /* ifdef CONFIG_MIL_DAQ_USE_RAM */

/*!
 * @ingroup SHARED_MEMORY
 * @brief Adresses of LM32 shared memory
 * @see https://www-acc.gsi.de/wiki/bin/viewauth/Hardware/Intern/ScuFgDoc#Memory_map_of_the_LM32_ram
 */
typedef enum
{
   BOARD_ID        = 0x500, /*!<@brief onewire id of the scu base board    16 */
   EXT_ID          = 0x508, /*!<@brief onewire id of the scu extension board   16 */
   BACKPLANE_ID    = 0x510, /*!<@brief onewire id of the scu backplane     16 */
   BOARD_TEMP      = 0x518, /*!<@brief temperature of the scu base board   8 */
   EXT_TEMP        = 0x51C, /*!<@brief temperature of the scu extension board  8 */
   BACKPLANE_TEMP  = 0x520, /*!<@brief temperature of the scu backplane    8 */
   FG_VERSION_OFS  = 0x528, /*!<@brief version number of the fg macro  8 */
   FG_MB_SLOT      = 0x52C, /*!<@brief mailbox slot for swi from linux     8 */
   FG_NUM_CHANNELS = 0x530, /*!<@brief max number of fg channels   8 */
   FG_BUFFER_SIZE  = 0x534, /*!<@brief buffersize per channel of aChannelBuffers     8 */
   FG_MACROS       = 0x538, /*!<@brief [256]  hi..lo bytes: slot, device, version, output-bits    8 * 256 */
   FG_REGS         = 0xD38, /*!<@brief [maxChannels]    array of struct channel_regs    8 * 7 * 256 */
   FG_BUFFER       = 0x4538 /*!<@brief [maxChannels] */
} SHARED_ADDRESS_T;

/*!
 * @ingroup SHARED_MEMORY
 * @brief Data type of the temperature object in the shared memory.
 */
typedef struct PACKED_SIZE
{  /*!
    * @brief 1Wire ID of the pcb temp sensor
    * @see BOARD_ID in saftlib/drivers/fg_regs.
    * @todo Check if this variable is really necessary in the future.
    */
   uint64_t            board_id;

   /*!
    * @brief 1Wire ID of the extension board temp sensor
    * @see EXT_ID in saftlib/drivers/fg_regs.h
    * @todo Check if this variable is really necessary in the future.
    */
   uint64_t            ext_id;

   /*!
    * @brief 1Wire ID of the backplane temp sensor
    * @see BACKPLANE_ID in saftlib/drivers/fg_regs.h
    * @todo Check if this variable is really necessary in the future.
    */
   uint64_t            backplane_id;

   /*!
    * @brief temperature value of the pcb sensor
    * @see BOARD_TEMP in saftlib/drivers/fg_regs.h
    * @todo Check if this variable is really necessary in the future.
    */
   uint32_t            board_temp;

   /*!
    * @brief temperature value of the extension board sensor
    * @see EXT_TEMP in saftlib/drivers/fg_regs.h
    * @todo Check if this variable is really necessary in the future.
    */
   uint32_t            ext_temp;

   /*!
    * @brief temperature value of the backplane sensor
    * @see BACKPLACE_TEMP in saftlib/drivers/fg_regs.h
    * @todo Check if this variable is really necessary in the future.
    */
   uint32_t            backplane_temp;

} SCU_TEMPERATURE_T;

#ifndef __DOXYGEN__
STATIC_ASSERT( offsetof( SCU_TEMPERATURE_T, board_id ) == 0 );
STATIC_ASSERT( offsetof( SCU_TEMPERATURE_T, ext_id ) ==
               offsetof( SCU_TEMPERATURE_T, board_id ) + sizeof( uint64_t ));
STATIC_ASSERT( offsetof( SCU_TEMPERATURE_T, backplane_id ) ==
               offsetof( SCU_TEMPERATURE_T, ext_id ) + sizeof( uint64_t ));
STATIC_ASSERT( offsetof( SCU_TEMPERATURE_T, board_temp ) ==
               offsetof( SCU_TEMPERATURE_T, backplane_id ) +
               sizeof( uint64_t ));
STATIC_ASSERT( offsetof( SCU_TEMPERATURE_T, ext_temp ) ==
               offsetof( SCU_TEMPERATURE_T, board_temp ) +
               sizeof( uint32_t ));
STATIC_ASSERT( offsetof( SCU_TEMPERATURE_T, backplane_temp ) ==
               offsetof( SCU_TEMPERATURE_T, ext_temp ) + sizeof( uint32_t ));
#endif

/*!
 * @ingroup SHARED_MEMORY
 * @brief Data type in shared memory for exchanging function generator data
 *        between SAFT-lib and LM32-firmware.
 */
typedef struct PACKED_SIZE
{  /*!
    * @brief Magic number for recognizing the LM32 firmware.
    * @see FG_MAGIC_NUMBER in saftlib/drivers/fg_regs.h
    */
   const uint32_t      magicNumber;

   /*!
    * @brief Version of this firmware
    *        0x2 saftlib, 0x3 new msi system with
    * @see FG_VERSION in saftlib/drivers/fg_regs.h
    */
   const uint32_t      version;

   /*!
    * @brief Mailbox-slot for host => LM32
    * @see FG_MB_SLOT saftlib/drivers/fg_regs.h
    * @see FunctionGeneratorFirmware::ScanFgChannels() in
    *      saftlib/drivers/FunctionGeneratorFirmware.cpp
    * @see FunctionGeneratorFirmware::ScanMasterFg() in
    *      saftlib/drivers/FunctionGeneratorFirmware.cpp
    */
   uint32_t            mailBoxSlot;

   /*!
    * @brief Maximum number of function generator channels which can
    *        support this SCU.
    * @see MAX_FG_CHANNELS
    * @see FG_NUM_CHANNELS in saftlib/drivers/fg_regs.h
    * @see FunctionGeneratorImpl::acquireChannel() in
    *      saftlib/drivers/FunctionGeneratorImpl.cpp
    * @todo Check if this variable is really necessary in the future,
    *       this information can be obtained by macro ARRAY_SIZE(aMacros)
    *       or MAX_FG_CHANNELS. Once this file becomes included in the sources of
    *       SAFTLIB as well.
    */
   const uint32_t       maxChannels;

   /*!
    * @brief Maximum size of the data buffer for a single function generator channel.
    * @see FG_BUFFER_SIZE saftlib/drivers/fg_regs.h
    * @see BUFFER_SIZE
    * @todo Check if this variable is really necessary in the future,
    *       this information can be obtained by macro BUFFER_SIZE
    *       Once this file becomes included in the sources of
    *       SAFTLIB as well.
    */
   const uint32_t      channelBufferSize;

   /*!
    * @brief  Array of found function generator channels of
    *         this SCU. \n Bytes: slot, device, version, output-bits
    *
    * This array becomes initialized by scan_all_fgs
    *
    * @see scan_all_fgs
    * @see FG_MACROS in saftlib/drivers/fg_regs.h
    */
   FG_MACRO_T          aMacros[MAX_FG_MACROS];

   /*!
    * @see FG_REGS_BASE_ in saftlib/drivers/fg_regs.h
    * @see FunctionGeneratorImpl::acquireChannel() in
    *      saftlib/drivers/FunctionGeneratorImpl.cpp
    */
   FG_CHANNEL_REG_T    aRegs[MAX_FG_CHANNELS];

   /*!
    * @brief Container for all polynomial vectors of all supported
    *        function generator channels.
    * @see FG_CHANNEL_BUFFER_T
    * @see FG_PARAM_SET_T
    * @see FunctionGeneratorImpl::refill()
    *      in saftlib/drivers/FunctionGeneratorImpl.cpp
    */
   FG_CHANNEL_BUFFER_T aChannelBuffers[MAX_FG_CHANNELS];

   /*!
    * @see FG_SCAN_DONE in saftlib/drivers/fg_regs.h
    * @see FunctionGeneratorFirmware::firmware_rescan in
    *      saftlib/drivers/FunctionGeneratorFirmware.cpp
    */
   uint32_t           busy;
} FG_SHARED_DATA_T;

#ifndef __DOXYGEN__
STATIC_ASSERT( offsetof( FG_SHARED_DATA_T, magicNumber ) == 0 );
STATIC_ASSERT( offsetof( FG_SHARED_DATA_T, version ) ==
               offsetof( FG_SHARED_DATA_T, magicNumber ) +
               sizeof( uint32_t ));
STATIC_ASSERT( offsetof( FG_SHARED_DATA_T, mailBoxSlot ) ==
               offsetof( FG_SHARED_DATA_T, version ) +
               sizeof( uint32_t ));
STATIC_ASSERT( offsetof( FG_SHARED_DATA_T, maxChannels ) ==
               offsetof( FG_SHARED_DATA_T, mailBoxSlot ) +
               sizeof( uint32_t ));
STATIC_ASSERT( offsetof( FG_SHARED_DATA_T, channelBufferSize ) ==
               offsetof( FG_SHARED_DATA_T, maxChannels ) +
               sizeof( uint32_t ));
STATIC_ASSERT( offsetof( FG_SHARED_DATA_T, aMacros ) ==
               offsetof( FG_SHARED_DATA_T, channelBufferSize ) +
               sizeof( uint32_t ));
STATIC_ASSERT( offsetof( FG_SHARED_DATA_T, aRegs ) ==
               offsetof( FG_SHARED_DATA_T, aMacros ) +
               MAX_FG_MACROS * sizeof( uint32_t ));
STATIC_ASSERT( offsetof( FG_SHARED_DATA_T, aChannelBuffers ) ==
               offsetof( FG_SHARED_DATA_T, aRegs ) +
               MAX_FG_CHANNELS * sizeof( FG_CHANNEL_REG_T ));
#endif

/*! ---------------------------------------------------------------------------
 * @ingroup SHARED_MEMORY
 * @brief Definition of shared memory area for the communication between LM32
 *        and Linux.
 * @see https://www-acc.gsi.de/wiki/bin/viewauth/Hardware/Intern/ScuFgDoc#Memory_map_of_the_LM32_ram
 * @see saftlib/drivers/fg_regs.h
 */
typedef struct PACKED_SIZE
{  /*!
    * @brief Object including some SCU temperature values.
    */
   SCU_TEMPERATURE_T   oTemperatures;

   /*!
    * @brief Object including all function generator data for exchanging
    *        between SAFT-lib and LM32.
    */
   FG_SHARED_DATA_T    oFg;

#ifdef CONFIG_MIL_DAQ_USE_RAM
   /*!
    * @brief MIL-DAQ ring-buffer administration indexes
    *        for DDR3-RAM.
    */
   RAM_RING_INDEXES_T  mdaqRing;
#else
   /*!
    * @brief MIL-DAQ-ring-buffer object in LM32 shared memory
    * @var daq_buf
    */
   ADD_NAMESPACE( Scu::MiLdaq, MIL_DAQ_BUFFER_T ) daq_buf;
#endif

#ifdef CONFIG_SCU_DAQ_INTEGRATION
   /*!
    * @brief Shared memory objects of non-MIL-DAQs (ADDAC/ACU-DAQ)
    * @var sDaq
    */
   ADD_NAMESPACE( Scu::daq, DAQ_SHARED_IO_T ) sDaq;
#endif
} SCU_SHARED_DATA_T;

/*!
 * @brief All member variables under this offset value are known in SAFTLIB.
 * @note Don't move any of it!  
 */
#define FG_SHM_BASE_SIZE ( offsetof( SCU_SHARED_DATA_T, oFg ) + sizeof( FG_SHARED_DATA_T ) )


#ifdef CONFIG_MIL_DAQ_USE_RAM
  #define DAQ_SHM_OFFET (offsetof( SCU_SHARED_DATA_T, mdaqRing ) + sizeof( RAM_RING_INDEXES_T ))
#else
  #define DAQ_SHM_OFFET (offsetof( SCU_SHARED_DATA_T, daq_buf ) + sizeof( ADD_NAMESPACE( Scu::MiLdaq, MIL_DAQ_BUFFER_T ) ))
#endif


#define GET_SCU_SHM_OFFSET( m ) offsetof( SCU_SHARED_DATA_T, m )

#ifndef __DOXYGEN__
/*
 * Unfortunately necessary because in some elements of some sub-structures of
 * the main structure SCU_SHARED_DATA_T are still declared as "unsigned int",
 * and it's never guaranteed that the type "unsigned int" is a 32 bit type
 * on all platforms!
 */
STATIC_ASSERT( sizeof(uint32_t) == sizeof(unsigned int) );
STATIC_ASSERT( sizeof(int32_t) == sizeof(signed int) );
STATIC_ASSERT( sizeof(uint16_t) == sizeof(signed short) );

/*
 * We have to made a static check verifying whether the structure-format
 * is equal on both platforms: Linux and LM32.
 */
STATIC_ASSERT( offsetof( FG_PARAM_SET_T, coeff_a ) == 0 );
STATIC_ASSERT( offsetof( FG_PARAM_SET_T, coeff_b ) ==
               offsetof( FG_PARAM_SET_T, coeff_a ) + sizeof( uint16_t ));
STATIC_ASSERT( offsetof( FG_PARAM_SET_T, coeff_c ) ==
               offsetof( FG_PARAM_SET_T, coeff_b ) + sizeof( uint16_t ));
STATIC_ASSERT( offsetof( FG_PARAM_SET_T, control ) ==
               offsetof( FG_PARAM_SET_T, coeff_c ) + sizeof(int32_t) );
STATIC_ASSERT( sizeof( FG_PARAM_SET_T ) ==
               offsetof( FG_PARAM_SET_T, control ) + sizeof(uint32_t) );

STATIC_ASSERT( sizeof( FG_CHANNEL_BUFFER_T ) ==
               BUFFER_SIZE * sizeof( FG_PARAM_SET_T ) );

STATIC_ASSERT( offsetof( FG_CHANNEL_REG_T, wr_ptr  ) == 0 );
STATIC_ASSERT( offsetof( FG_CHANNEL_REG_T, rd_ptr  ) ==
               offsetof( FG_CHANNEL_REG_T, wr_ptr  ) + sizeof( uint32_t ));
STATIC_ASSERT( offsetof( FG_CHANNEL_REG_T, mbx_slot ) ==
               offsetof( FG_CHANNEL_REG_T, rd_ptr  ) + sizeof( uint32_t ));
STATIC_ASSERT( offsetof( FG_CHANNEL_REG_T, macro_number ) ==
               offsetof( FG_CHANNEL_REG_T, mbx_slot ) + sizeof( uint32_t ));
STATIC_ASSERT( offsetof( FG_CHANNEL_REG_T, ramp_count ) ==
               offsetof( FG_CHANNEL_REG_T, macro_number ) +
               sizeof( uint32_t ));
STATIC_ASSERT( offsetof( FG_CHANNEL_REG_T, tag ) ==
               offsetof( FG_CHANNEL_REG_T, ramp_count ) +
               sizeof( uint32_t ));
STATIC_ASSERT( offsetof( FG_CHANNEL_REG_T, state ) ==
               offsetof( FG_CHANNEL_REG_T, tag ) + sizeof( uint32_t ));
STATIC_ASSERT( sizeof( FG_CHANNEL_REG_T ) ==
               offsetof( FG_CHANNEL_REG_T, state ) + sizeof( uint32_t ));


STATIC_ASSERT( offsetof( SCU_SHARED_DATA_T, oFg ) ==
               offsetof( SCU_SHARED_DATA_T, oTemperatures ) +
               sizeof( SCU_TEMPERATURE_T ));

 #ifdef CONFIG_MIL_DAQ_USE_RAM
  STATIC_ASSERT( offsetof( SCU_SHARED_DATA_T, mdaqRing ) == FG_SHM_BASE_SIZE );
 #else
  STATIC_ASSERT( offsetof( SCU_SHARED_DATA_T, daq_buf ) == FG_SHM_BASE_SIZE );
 #endif
 
 #ifdef CONFIG_SCU_DAQ_INTEGRATION
  #ifdef CONFIG_MIL_DAQ_USE_RAM
   STATIC_ASSERT( offsetof( SCU_SHARED_DATA_T, sDaq ) ==
                  offsetof( SCU_SHARED_DATA_T, mdaqRing ) +
                  sizeof( RAM_RING_INDEXES_T ));
  #else
   STATIC_ASSERT( offsetof( SCU_SHARED_DATA_T, sDaq ) == DAQ_SHM_OFFET );
  #endif
  STATIC_ASSERT( sizeof( SCU_SHARED_DATA_T ) ==
                 offsetof( SCU_SHARED_DATA_T, sDaq ) +
                 sizeof( ADD_NAMESPACE( Scu::daq, DAQ_SHARED_IO_T ) ));
 #else /* ifdef CONFIG_SCU_DAQ_INTEGRATION */
  #ifdef CONFIG_MIL_DAQ_USE_RAM
    STATIC_ASSERT( sizeof( SCU_SHARED_DATA_T ) ==
                   offsetof( SCU_SHARED_DATA_T, mdaqRing ) +
                   sizeof( RAM_RING_INDEXES_T ));
  #else
    STATIC_ASSERT( offsetof( SCU_SHARED_DATA_T, daq_buf ) ==
                   offsetof( SCU_SHARED_DATA_T, busy ) +
                   sizeof( uint32_t ));
  #endif
 #endif /* / ifdef CONFIG_SCU_DAQ_INTEGRATION */
#endif /* ifndef __DOXYGEN__ */

/* ++++++++++++++ Initializer ++++++++++++++++++++++++++++++++++++++++++++++ */
/*!
 * @brief Magic number for the host to recognize the correct firmware.
 */
#define FG_MAGIC_NUMBER ((uint32_t)0xDEADBEEF)

#define SCU_INVALID_VALUE -1

#ifndef FG_VERSION
  #ifdef __lm32__
    #error Macro FG_VERSION has to be defined in Makefile!
  #else
    #define FG_VERSION  0
  #endif
#endif

#ifdef CONFIG_SCU_DAQ_INTEGRATION
  #define __DAQ_SHARAD_MEM_INITIALIZER_ITEM \
     , .sDaq = DAQ_SHARAD_MEM_INITIALIZER
#else
  #define __DAQ_SHARAD_MEM_INITIALIZER_ITEM
#endif

#ifdef CONFIG_MIL_DAQ_USE_RAM
  #define __MIL_DAQ_SHARAD_MEM_INITIALIZER_ITEM \
     , .mdaqRing = RAM_RING_INDEXES_MDAQ_INITIALIZER
#else
  #define __MIL_DAQ_SHARAD_MEM_INITIALIZER_ITEM \
     , .daq_buf = {0}
#endif

/*! ---------------------------------------------------------------------------
 * @brief Initializer of the entire LM32 shared memory of application
 *        scu_control.
 */
#define SCU_SHARED_DATA_INITIALIZER                          \
{                                                            \
   .oTemperatures.board_id         = SCU_INVALID_VALUE,      \
   .oTemperatures.ext_id           = SCU_INVALID_VALUE,      \
   .oTemperatures.backplane_id     = SCU_INVALID_VALUE,      \
   .oTemperatures.board_temp       = SCU_INVALID_VALUE,      \
   .oTemperatures.ext_temp         = SCU_INVALID_VALUE,      \
   .oTemperatures.backplane_temp   = SCU_INVALID_VALUE,      \
   .oFg.magicNumber                = FG_MAGIC_NUMBER,        \
   .oFg.version                    = FG_VERSION,             \
   .oFg.mailBoxSlot                = SCU_INVALID_VALUE,      \
   .oFg.maxChannels                = MAX_FG_CHANNELS,        \
   .oFg.channelBufferSize          = BUFFER_SIZE,            \
   .oFg.aMacros                    = {{0,0,0,0}},            \
   .oFg.busy                       = 0                       \
   __MIL_DAQ_SHARAD_MEM_INITIALIZER_ITEM                     \
   __DAQ_SHARAD_MEM_INITIALIZER_ITEM                         \
}

/* ++++++++++ End  Initializer +++++++++++++++++++++++++++++++++++++++++++++ */

typedef enum
{
   FG_OP_RESET_CHANNEL       = 0,
   FG_OP_MIL_GAP_INTERVAL    = 1, //!<@brief set interval time for MIL gap reading.
   FG_OP_ENABLE_CHANNEL      = 2, //!<@brief SWI_ENABLE
   FG_OP_DISABLE_CHANNEL     = 3, //!<@brief SWI_DISABLE
   FG_OP_RESCAN              = 4, //!<@brief SWI_SCAN
   FG_OP_CLEAR_HANDLER_STATE = 5,
   FG_OP_PRINT_HISTORY       = 6
} FG_OP_CODE_T;

/*!
 * @brief Helper function for debug purposes only.
 */
STATIC inline const char* fgCommand2String( const FG_OP_CODE_T op )
{
   #define __FG_COMMAND_CASE( cmd ) case cmd: return #cmd
   switch( op )
   {
      __FG_COMMAND_CASE( FG_OP_RESET_CHANNEL );
      __FG_COMMAND_CASE( FG_OP_MIL_GAP_INTERVAL );
      __FG_COMMAND_CASE( FG_OP_ENABLE_CHANNEL );
      __FG_COMMAND_CASE( FG_OP_DISABLE_CHANNEL );
      __FG_COMMAND_CASE( FG_OP_RESCAN );
      __FG_COMMAND_CASE( FG_OP_CLEAR_HANDLER_STATE );
      __FG_COMMAND_CASE( FG_OP_PRINT_HISTORY );
   }
   return "unknown";
   #undef __FG_COMMAND_CASE
}

/*!
 * @ingroup MAILBOX
 * @brief Definition of signals to send from server (LM32) to client (SAFTLIB)
 * @see sendSignal
 * @see saftlib/drivers/fg_regs.h
 * @see FunctionGeneratorImpl::irq_handler in saftlib/drivers/FunctionGeneratorImpl.cpp
 */
typedef enum
{
   IRQ_DAT_REFILL         = 0, /*!<@brief Buffer level becomes low -> refill request */
   IRQ_DAT_START          = 1, /*!<@brief FG started */
   IRQ_DAT_STOP_EMPTY     = 2, /*!<@brief normal stop or "microControllerUnderflow" */
   IRQ_DAT_STOP_NOT_EMPTY = 3, /*!<@brief something went wrong "hardwareMacroUnderflow" */
   IRQ_DAT_ARMED          = 4, /*!<@brief FG ready for data */
   IRQ_DAT_DISARMED       = 5  /*!<@brief FG not ready */
} SIGNAL_T;

/*!
 * @ingroup MAILBOX
 * @brief Helper function for debug purposes only.
 */
STATIC inline const char* signal2String( const SIGNAL_T sig )
{
   #define __SIGNAL_CASE( sig ) case sig: return #sig
   switch( sig )
   {
      __SIGNAL_CASE( IRQ_DAT_REFILL );
      __SIGNAL_CASE( IRQ_DAT_START );
      __SIGNAL_CASE( IRQ_DAT_STOP_EMPTY );
      __SIGNAL_CASE( IRQ_DAT_STOP_NOT_EMPTY );
      __SIGNAL_CASE( IRQ_DAT_ARMED );
      __SIGNAL_CASE( IRQ_DAT_DISARMED );
   }
   return "unknown";
   #undef  __SIGNAL_CASE
}

#ifdef __cplusplus
} /* namespace FG */

namespace MiLdaq
{
#endif

/*! ---------------------------------------------------------------------------
 */
STATIC inline ALWAYS_INLINE
unsigned int getSocketByFgMacro( const FG_MACRO_T fgMacro )
{
   return fgMacro.socket;
}

/*! ---------------------------------------------------------------------------
 */
STATIC inline ALWAYS_INLINE
unsigned int getDeviceByFgMacro( const FG_MACRO_T fgMacro )
{
   return fgMacro.device;
}

/*! ---------------------------------------------------------------------------
 */
STATIC inline ALWAYS_INLINE
unsigned int getFgMacroVersion( const FG_MACRO_T fgMacro )
{
   return fgMacro.version;
}

/*! ---------------------------------------------------------------------------
 */
STATIC inline ALWAYS_INLINE
unsigned int getFgOutputBits( const FG_MACRO_T fgMacro )
{
   return fgMacro.outputBits;
}

/*! ---------------------------------------------------------------------------
 */
STATIC inline ALWAYS_INLINE
unsigned int getMilDaqDevice( const register MIL_DAQ_OBJ_T* pMilDaq )
{
   return getDeviceByFgMacro( pMilDaq->fgMacro );
}

/*! ---------------------------------------------------------------------------
 */
STATIC inline ALWAYS_INLINE
unsigned int getMilDaqSocket( const register MIL_DAQ_OBJ_T* pMilDaq )
{
   return getSocketByFgMacro( pMilDaq->fgMacro );
}

/*! ---------------------------------------------------------------------------
 */
STATIC inline ALWAYS_INLINE
unsigned int getDaqMilScuBusSlotbySocket( const unsigned int socket )
{
   return socket & SCU_BUS_SLOT_MASK;
}

/*! ---------------------------------------------------------------------------
 */
STATIC inline ALWAYS_INLINE
unsigned int getDaqMilExtentionBySocket( const unsigned int socket )
{
   return socket >> 4;
}

/*! ---------------------------------------------------------------------------
 */
STATIC inline ALWAYS_INLINE
unsigned int getMilDaqScuBusSlot( const register MIL_DAQ_OBJ_T* pMilDaq )
{
   return getDaqMilScuBusSlotbySocket( getMilDaqSocket( pMilDaq ));
}

/*! ---------------------------------------------------------------------------
 */
STATIC inline ALWAYS_INLINE
unsigned int getMilDaqScuMilExtention( const register MIL_DAQ_OBJ_T* pMilDaq )
{
   return getDaqMilExtentionBySocket( getMilDaqSocket( pMilDaq ) );
}
#ifdef __cplusplus
} // namespace MiLdaq
} // namespace Scu
} // extern "C"
#endif

#endif /* ifndef _SCU_SHARED_MEM_H */
/*================================== EOF ====================================*/

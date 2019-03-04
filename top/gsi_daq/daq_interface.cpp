/*!
 *  @file daq_interface.cpp
 *  @brief DAQ Interface Library for Linux
 *
 *  @date 28.02.2019
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
#include <daq_interface.hpp>
#include <sys/select.h>

using namespace daq;

#define FUNCTION_NAME_TO_STD_STRING static_cast<const std::string>(__func__)

#define __THROW_EB_EXCEPTION() \
   throw Exception( FUNCTION_NAME_TO_STD_STRING + "(): "\
   + static_cast<const std::string>(::ebGetStatusString( m_poEbHandle )) )

///////////////////////////////////////////////////////////////////////////////
/*! ---------------------------------------------------------------------------
 * @brief Constructor of class daq::DaqInterface
 */
DaqInterface::DaqInterface( const std::string wbDevice )
   :m_wbDevice( wbDevice )
   ,m_poEbHandle( nullptr )
{
   if( ::ebOpen( &m_oEbHandle, m_wbDevice.c_str() ) != EB_OK )
      throw Exception( ::ebGetStatusString( &m_oEbHandle ) );

   m_poEbHandle = &m_oEbHandle;

   if( ::ramInit( &m_oScuRam, &m_oSharedData.ramIndexes, m_poEbHandle ) < 0 )
   {
      ebClose();
      throw( Exception( "Could not find RAM-device!" ) );
   }

   readSharedTotal();
   setCommand( DAQ_OP_RESET );
  // setCommand( DAQ_OP_GET_SLOTS ); //!!
 //  setCommand( DAQ_OP_LOCK );       //!!
}

/*! ---------------------------------------------------------------------------
 * @brief Destructor of class daq::DaqInterfaceInterface
 */
DaqInterface::~DaqInterface( void )
{
   ebClose();
}

/*! ---------------------------------------------------------------------------
 */
void DaqInterface::ebClose( void )
{
   if( m_poEbHandle == nullptr )
      return;

   if( ::ebClose( m_poEbHandle ) != EB_OK )
     __THROW_EB_EXCEPTION();

   m_poEbHandle = nullptr;
}

/*! ---------------------------------------------------------------------------
 */
void DaqInterface::readSharedTotal( void )
{
   EB_MEMBER_INFO_T info[3];
   EB_INIT_INFO_ITEM_STATIC( info, 0, m_oSharedData.magicNumber );
   EB_INIT_INFO_ITEM_STATIC( info, 1, m_oSharedData.operation.code );
   EB_INIT_INFO_ITEM_STATIC( info, 2, m_oSharedData.operation.retCode );

   EB_MAKE_CB_OR_ARG( cArg, info );

   if( ebReadObjectCycleOpen( cArg ) != EB_OK )
      __THROW_EB_EXCEPTION();

   EB_OJECT_MEMBER_READ( m_poEbHandle, DAQ_SHARED_IO_T, magicNumber );
   EB_OJECT_MEMBER_READ( m_poEbHandle, DAQ_SHARED_IO_T, operation.code );
   EB_OJECT_MEMBER_READ( m_poEbHandle, DAQ_SHARED_IO_T, operation.retCode );
   ebCycleClose();

   while( !cArg.exit )
      ebSocketRun();

   m_poEbHandle->status = cArg.status;
   if( m_poEbHandle->status != EB_OK )
      __THROW_EB_EXCEPTION();

   if( m_oSharedData.magicNumber != DAQ_MAGIC_NUMBER )
      throw Exception( "Wrong DAQ magic number" );
}

/*! ---------------------------------------------------------------------------
 */
bool DaqInterface::onCommandReadyPoll( unsigned int pollCount )
{
   if( pollCount >= c_maxCmdPoll )
      return true;
   struct timeval sleepTime = {0, 1};
   ::select( 0, nullptr, nullptr, nullptr, &sleepTime );
   return false;
}

/*! ---------------------------------------------------------------------------
 */
bool DaqInterface::cmdReadyWait( void )
{
   unsigned int pollCount = 0;
   while( getCommand() != DAQ_OP_IDLE )
   {
      if( onCommandReadyPoll( pollCount ) )
         return true;
      pollCount++;
   }
   return false;
}

/*! ---------------------------------------------------------------------------
 */
void DaqInterface::setCommand( DAQ_OPERATION_CODE_T cmd )
{

   m_oSharedData.operation.code = cmd;
   EB_MAKE_CB_OW_ARG( cArg );

    if( ebWriteObjectCycleOpen( cArg ) != EB_OK )
       __THROW_EB_EXCEPTION();

   EB_LM32_OJECT_MEMBER_WRITE( m_poEbHandle, &m_oSharedData, operation.code );
   ebCycleClose();

   while( !cArg.exit )
      ebSocketRun();

   m_poEbHandle->status = cArg.status;
   if( m_poEbHandle->status != EB_OK )
      __THROW_EB_EXCEPTION();

   if( cmdReadyWait() )
      throw Exception( "Timeout" );
}

/*! ---------------------------------------------------------------------------
 */
DAQ_OPERATION_CODE_T DaqInterface::getCommand( void )
{
   EB_MEMBER_INFO_T info[2];

   EB_INIT_INFO_ITEM_STATIC( info, 0, m_oSharedData.operation.code );
   EB_INIT_INFO_ITEM_STATIC( info, 1, m_oSharedData.operation.retCode );
   EB_MAKE_CB_OR_ARG( cArg, info );

   if( ebReadObjectCycleOpen( cArg ) != EB_OK )
      __THROW_EB_EXCEPTION();

   EB_OJECT_MEMBER_READ( m_poEbHandle, DAQ_SHARED_IO_T, operation.code );
   EB_OJECT_MEMBER_READ( m_poEbHandle, DAQ_SHARED_IO_T, operation.retCode );
   ebCycleClose();

   while( !cArg.exit )
      ebSocketRun();

   m_poEbHandle->status = cArg.status;
   if( m_poEbHandle->status != EB_OK )
      __THROW_EB_EXCEPTION();

   return m_oSharedData.operation.code;
}

/*! ---------------------------------------------------------------------------
 */
DaqInterface::RETURN_CODE_T DaqInterface::readParam1( void )
{
   EB_MEMBER_INFO_T info[2];

   EB_INIT_INFO_ITEM_STATIC( info, 0, m_oSharedData.operation.retCode );
   EB_INIT_INFO_ITEM_STATIC( info, 1, m_oSharedData.operation.ioData.param1 );
   EB_MAKE_CB_OR_ARG( cArg, info );

   if( ebReadObjectCycleOpen( cArg ) != EB_OK )
      __THROW_EB_EXCEPTION();

   EB_OJECT_MEMBER_READ( m_poEbHandle, DAQ_SHARED_IO_T, operation.retCode );
   EB_OJECT_MEMBER_READ( m_poEbHandle, DAQ_SHARED_IO_T, operation.ioData.param1 );
   ebCycleClose();

   while( !cArg.exit )
      ebSocketRun();

   m_poEbHandle->status = cArg.status;
   if( m_poEbHandle->status != EB_OK )
      __THROW_EB_EXCEPTION();

   return m_oSharedData.operation.retCode;
}

/*! ---------------------------------------------------------------------------
 */
DaqInterface::RETURN_CODE_T DaqInterface::readParam12( void )
{
   EB_MEMBER_INFO_T info[3];

   EB_INIT_INFO_ITEM_STATIC( info, 0, m_oSharedData.operation.retCode );
   EB_INIT_INFO_ITEM_STATIC( info, 1, m_oSharedData.operation.ioData.param1 );
   EB_INIT_INFO_ITEM_STATIC( info, 2, m_oSharedData.operation.ioData.param2 );
   EB_MAKE_CB_OR_ARG( cArg, info );

   if( ebReadObjectCycleOpen( cArg ) != EB_OK )
      __THROW_EB_EXCEPTION();

   EB_OJECT_MEMBER_READ( m_poEbHandle, DAQ_SHARED_IO_T, operation.retCode );
   EB_OJECT_MEMBER_READ( m_poEbHandle, DAQ_SHARED_IO_T, operation.ioData.param1 );
   EB_OJECT_MEMBER_READ( m_poEbHandle, DAQ_SHARED_IO_T, operation.ioData.param2 );
   ebCycleClose();

   while( !cArg.exit )
      ebSocketRun();

   m_poEbHandle->status = cArg.status;
   if( m_poEbHandle->status != EB_OK )
      __THROW_EB_EXCEPTION();

   return m_oSharedData.operation.retCode;
}

/*! ---------------------------------------------------------------------------
 */
DaqInterface::RETURN_CODE_T DaqInterface::readParam123( void )
{
   EB_MEMBER_INFO_T info[4];

   EB_INIT_INFO_ITEM_STATIC( info, 0, m_oSharedData.operation.retCode );
   EB_INIT_INFO_ITEM_STATIC( info, 1, m_oSharedData.operation.ioData.param1 );
   EB_INIT_INFO_ITEM_STATIC( info, 2, m_oSharedData.operation.ioData.param2 );
   EB_INIT_INFO_ITEM_STATIC( info, 3, m_oSharedData.operation.ioData.param3 );
   EB_MAKE_CB_OR_ARG( cArg, info );

   if( ebReadObjectCycleOpen( cArg ) != EB_OK )
      __THROW_EB_EXCEPTION();

   EB_OJECT_MEMBER_READ( m_poEbHandle, DAQ_SHARED_IO_T, operation.retCode );
   EB_OJECT_MEMBER_READ( m_poEbHandle, DAQ_SHARED_IO_T, operation.ioData.param1 );
   EB_OJECT_MEMBER_READ( m_poEbHandle, DAQ_SHARED_IO_T, operation.ioData.param2 );
   EB_OJECT_MEMBER_READ( m_poEbHandle, DAQ_SHARED_IO_T, operation.ioData.param3 );
   ebCycleClose();

   while( !cArg.exit )
      ebSocketRun();

   m_poEbHandle->status = cArg.status;
   if( m_poEbHandle->status != EB_OK )
      __THROW_EB_EXCEPTION();


   return m_oSharedData.operation.retCode;
}

/*! ---------------------------------------------------------------------------
 */
DaqInterface::RETURN_CODE_T DaqInterface::readParam1234( void )
{
   EB_MEMBER_INFO_T info[5];

   EB_INIT_INFO_ITEM_STATIC( info, 0, m_oSharedData.operation.retCode );
   EB_INIT_INFO_ITEM_STATIC( info, 1, m_oSharedData.operation.ioData.param1 );
   EB_INIT_INFO_ITEM_STATIC( info, 2, m_oSharedData.operation.ioData.param2 );
   EB_INIT_INFO_ITEM_STATIC( info, 3, m_oSharedData.operation.ioData.param3 );
   EB_INIT_INFO_ITEM_STATIC( info, 4, m_oSharedData.operation.ioData.param4 );
   EB_MAKE_CB_OR_ARG( cArg, info );

   if( ebReadObjectCycleOpen( cArg ) != EB_OK )
      __THROW_EB_EXCEPTION();

   EB_OJECT_MEMBER_READ( m_poEbHandle, DAQ_SHARED_IO_T, operation.retCode );
   EB_OJECT_MEMBER_READ( m_poEbHandle, DAQ_SHARED_IO_T, operation.ioData.param1 );
   EB_OJECT_MEMBER_READ( m_poEbHandle, DAQ_SHARED_IO_T, operation.ioData.param2 );
   EB_OJECT_MEMBER_READ( m_poEbHandle, DAQ_SHARED_IO_T, operation.ioData.param3 );
   EB_OJECT_MEMBER_READ( m_poEbHandle, DAQ_SHARED_IO_T, operation.ioData.param4 );
   ebCycleClose();

   while( !cArg.exit )
      ebSocketRun();

   m_poEbHandle->status = cArg.status;
   if( m_poEbHandle->status != EB_OK )
      __THROW_EB_EXCEPTION();

   return m_oSharedData.operation.retCode;
}

/*! ---------------------------------------------------------------------------
 */
DaqInterface::SLOT_FLAGS_T DaqInterface::readSlotStatus( void )
{
   setCommand( DAQ_OP_GET_SLOTS );
   readParam1();
   return m_oSharedData.operation.ioData.param1;
}

//================================== EOF ======================================

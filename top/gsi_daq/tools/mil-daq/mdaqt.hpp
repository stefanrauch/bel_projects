/*!
 *  @file mdaqt.hpp
 *  @brief Main module of MIL-Data Acquisition Tool
 *
 *  @date 14.08.2019
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
#ifndef _MDAQT_HPP
#define _MDAQT_HPP

#ifndef __DOCFSM__
 #include <string>
 #include <stdlib.h>
 #include <iostream>
 #include <vector>
 #include <mdaq_administration.hpp>
 #include <daq_eb_ram_buffer.hpp>
 #include <daq_calculations.hpp>
 #include <mdaqt_command_line.hpp>
#endif

namespace Scu
{
namespace MiLdaq
{
namespace MiLdaqt
{

#ifndef GNUPLOT_DEFAULT_TERMINAL
  #define GNUPLOT_DEFAULT_TERMINAL "X11 size 1200,600"
#endif

#ifdef FSM_DECLARE_STATE
   #undef  FSM_DECLARE_STATE
#endif
#define FSM_DECLARE_STATE( state, attr... ) state

class Plot;
class Device;

//////////////////////////////////////////////////////////////////////////////
class DaqMilCompare: public DaqCompare
{
   friend class Plot;
   struct PLOT_T
   {
      double  m_time;
      float   m_set;
      float   m_act;
   };

   enum STATE_T
   {
      FSM_DECLARE_STATE( WAIT, label='waiting for trigger condition' ),
      FSM_DECLARE_STATE( START, label='initialize new plot' ),
      FSM_DECLARE_STATE( COLLECT, label='collecting data' ),
      FSM_DECLARE_STATE( PLOT, label='plot values' )
   };

   STATE_T            m_state;

   typedef std::vector<PLOT_T> PLOT_LIST_T;

   MIL_DAQ_T          m_lastSetRawValue;
   MIL_DAQ_T          m_lastActRawValue;
   uint64_t           m_startTime;
   uint64_t           m_lastTime;
   PLOT_LIST_T        m_aPlotList;
   Plot*              m_pPlot;

public:
   DaqMilCompare( uint iterfaceAddress );
   virtual ~DaqMilCompare( void );

   Device* getParent( void );

   uint64_t getPlotStartTime( void ) const
   {
      return m_startTime;
   }

   uint64_t getTimeLimitNanoSec( void ) const
   {
      return 10 * daq::NANOSECS_PER_SEC;
   }

   double getTimeLimit( void ) const
   {
      return static_cast<double>(getTimeLimitNanoSec()) /
             static_cast<double>(daq::NANOSECS_PER_SEC);
   }

   void reset( void );

   std::size_t getItemLimit( void ) const
   {
      return 20000;
   }

   std::string getOutputTerminal( void )
   {
      return GNUPLOT_DEFAULT_TERMINAL;
   }

private:
   void onData( uint64_t wrTimeStamp, MIL_DAQ_T actValue,
                                      MIL_DAQ_T setValue ) override;

   void addItem( uint64_t time, MIL_DAQ_T actValue, MIL_DAQ_T setValue );

   void onInit( void ) override;
};

class MilDaqAdministration;
//////////////////////////////////////////////////////////////////////////////
class Device: public DaqDevice
{
public:
   Device( uint n ): DaqDevice( n ) {}
   DaqMilCompare* getDaqCompare( const uint address )
   {
      return static_cast<DaqMilCompare*>(DaqDevice::getDaqCompare( address ));
   }

   MilDaqAdministration* getParent( void );
};

class CommandLine;

//////////////////////////////////////////////////////////////////////////////
class MilDaqAdministration: public DaqAdministration
{
   friend class CommandLine;
   CommandLine*   m_poCommandLine;

public:
   MilDaqAdministration( CommandLine* m_poCommandLine, std::string ebAddress );
   virtual ~MilDaqAdministration( void );

   CommandLine* getCommandLine( void )
   {
      return m_poCommandLine;
   }

   Device* getDevice( const uint number )
   {
      return static_cast<Device*>(DaqAdministration::getDevice( number ));
   }

   void onUnregistered( RingItem* pUnknownItem ) override;

   bool showUngegistered( void );
};

inline
Device* DaqMilCompare::getParent( void )
{
   return static_cast<Device*>(DaqCompare::getParent());
}

inline
MilDaqAdministration* Device::getParent( void )
{
   return static_cast<MilDaqAdministration*>(DaqDevice::getParent());
}


} // namespace MiLdaqt
} // namespace MilDaq
} // namespace Scu
#endif // ifndef _MDAQT_HPP
//================================== EOF ======================================

/*!
 *  @file mdaqt_plot.cpp
 *  @brief Specialization of class PlotStream for plotting set and actual
 *         values of MIL-DAQs
 *
 *  @date 19.08.2019
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
#include "mdaq_plot.hpp"
#include <daq_calculations.hpp>

using namespace Scu::MiLdaq::MiLdaqt;
using namespace std;

/*! ----------------------------------------------------------------------------
 */
Plot::Plot( DaqMilCompare* pParent,
            const std::string gpOpt,
            const std::string gpExe,
            const std::size_t pipeSize )
   :gpstr::PlotStream( gpOpt, gpExe, pipeSize )
   ,m_pParent( pParent )
{
   *this << "set terminal "
         << m_pParent->getParent()->getParent()->getCommandLine()->getTerminal()
         << " title \"SCU: "
         << m_pParent->getParent()->getParent()->getScuDomainName()
         << "\"" << endl;
   *this << "set grid" << endl;
   *this << "set ylabel \"Voltage\"" << endl;
   *this << "set yrange [" << -DAQ_VPP_MAX/2 << ':'
                           << DAQ_VPP_MAX/2 << ']' << endl;
}

/*! ----------------------------------------------------------------------------
 */
void Plot::plot( void )
{
   *this << "set title \"fg-" << m_pParent->getParent()->getLocation()
         << '-' << m_pParent->getAddress()
         << "  Date: "
         << daq::wrToTimeDateString( m_pParent->getPlotStartTime() ) << endl;

   *this << "set xrange [0:" << m_pParent->getTimeLimit() << ']' << endl;
   *this << "set xlabel \"Plot start time: " << m_pParent->getPlotStartTime()
         << " ns\"" << endl;

   //if( m_pParent->m_aPlotList.empty() )
   //   return;

   *this << "plot '-' title 'set value' with lines,"
                " '-' title 'actual value' with lines"
         << endl;

   for( auto& i: m_pParent->m_aPlotList )
   {
      *this << i.m_time << ' ' << i.m_set << endl;
   }
   *this << 'e' << endl;

   for( auto& i: m_pParent->m_aPlotList )
   {
      *this << i.m_time << ' ' << i.m_act << endl;
   }
   *this << 'e' << endl;
}

//================================== EOF ======================================

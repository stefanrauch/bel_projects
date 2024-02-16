/******************************************************************************
 *  wrmillib.h
 *
 *  created : 2024
 *  author  : Dietrich Beck, GSI-Darmstadt
 *  version : 15-Feb-2024
 *
 * library for wr-mil
 *
 * -------------------------------------------------------------------------------------------
 * License Agreement for this software:
 *
 * Copyright (C) 2013  Dietrich Beck
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
 * Last update: 17-May-2017
 ********************************************************************************************/
#ifndef _WRMIL_LIB_H_
#define _WRMIL_LIB_H_

#ifdef __cplusplus
extern "C" {
#endif

#define WRMILLIB_VERSION 0x000001

// (error) codes; duplicated to avoid the need of joining bel_projects and acc git repos
#define  WRMILLIB_STATUS_OK                 0            // OK
#define  WRMILLIB_STATUS_ERROR              1            // an error occured
#define  WRMILLIB_STATUS_TIMEDOUT           2            // a timeout occured
#define  WRMILLIB_STATUS_OUTOFRANGE         3            // some value is out of range
#define  WRMILLIB_STATUS_EB                 4            // an Etherbone error occured
#define  WRMILLIB_STATUS_NOIP               5            // DHCP request via WR network failed
#define  WRMILLIB_STATUS_WRONGIP            6            // IP received via DHCP does not match local config
#define  WRMILLIB_STATUS_EBREADTIMEDOUT     7            // EB read via WR network timed out
#define  WRMILLIB_STATUS_WRBADSYNC          8            // White Rabbit: not in 'TRACK_PHASE'
#define  WRMILLIB_STATUS_AUTORECOVERY       9            // trying auto-recovery from state ERROR
#define  WRMILLIB_STATUS_RESERVEDTILHERE   15            // 00..15 reserved for common error codes

// states; duplicated to avoid the need of joining bel_projects and acc git repos
#define  WRMILLIB_STATE_UNKNOWN             0            // unknown state
#define  WRMILLIB_STATE_S0                  1            // initial state -> IDLE (automatic)
#define  WRMILLIB_STATE_IDLE                2            // idle state -> CONFIGURED (by command "configure")
#define  WRMILLIB_STATE_CONFIGURED          3            // configured state -> IDLE ("idle"), CONFIGURED ("configure"), OPREADY ("startop")
#define  WRMILLIB_STATE_OPREADY             4            // in operation -> STOPPING ("stopop")
#define  WRMILLIB_STATE_STOPPING            5            // in operation -> CONFIGURED (automatic)
#define  WRMILLIB_STATE_ERROR               6            // in error -> IDLE ("recover")
#define  WRMILLIB_STATE_FATAL               7            // in fatal error; RIP

  // data type set values; data are in 'native units' used by the lm32 firmware; NAN of unsigned integers is signaled by all bits set
  typedef struct{                                      
  } setval_t;
  
  // data type get values; data are in 'native units' used by the lm32 firmware
  typedef struct{                                      
  } getval_t;

    
  // ---------------------------------
  // helper routines
  // ---------------------------------
  
  // get host system time [ns]
  uint64_t wrmil_getSysTime();

  // convert status code to status text
  const char* wrmil_status_text(uint32_t code                    // status code
                              );

  // convert state code to state text
  const char* wrmil_state_text(uint32_t code                     // state code
                               );

  // convert numeric version number to string
  const char* wrmil_version_text(uint32_t number                 // version number
                               );

  //convert timestamp [ns] to seconds and nanoseconds
  void wrmil_t2secs(uint64_t ts,                                 // timestamp [ns]
                  uint32_t *secs,                              // seconds
                  uint32_t *nsecs                              // nanosecons
                  );

  // enable debugging to trace library activity (experimental)
  void wrmil_debug(uint32_t flagDebug                            // 1: debug on; 0: debug off
                 );

  // ---------------------------------
  // communication with lm32 firmware
  // ---------------------------------

  // open connection to firmware, returns error code
  uint32_t wrmil_firmware_open(uint64_t       *ebDevice,         // EB device
                               const char*    device,            // EB device such as 'dev/wbm0'
                               uint32_t       cpu,               // # of CPU, 0..0xff
                               uint32_t       *wbAddr            // WB address of firmware
                               );
  
  // close connection to firmware, returns error code
  uint32_t wrmil_firmware_close(uint64_t ebDevice                // EB device
                                );
  
  // get version of firmware, returns error code
  uint32_t wrmil_version_firmware(uint64_t ebDevice,             // EB device
                                  uint32_t *version              // version number
                                  );
  
  // get version of library, returns error code
  uint32_t wrmil_version_library(uint32_t *version               // version number
                                 );
  
  // get info from firmware, returns error code
  uint32_t wrmil_info_read(uint64_t ebDevice,                    // EB device
                           uint32_t *utcTrigger,                 // the MIL event that triggers the generation of UTC events
                           uint32_t *utcDelay,                   // delay [us] between the 5 generated UTC MIL events
                           uint32_t *trigUtcDelay,               // delay [us] between the trigger event and the first UTC (and other) generated events
                           uint32_t *gid,                        // timing group ID for which the gateway is generating MIL events (example: 0x12c is SIS18)
                           int32_t  *latency,                    // MIL event is generated 100us+latency after the WR event. The value of latency can be negative
                           uint64_t *utcOffset,                  // delay [ms] between the TAI and the MIL-UTC, high word   
                           uint32_t *requestFill,                // if this is written to 1, the gateway will send a fill event as soon as possible
                           uint32_t *milDev,                     // wishbone address of MIL device; MIL device could be a MIL piggy or a SIO
                           uint32_t *milMon,                     // 1: monitor MIL events; 0; don't monitor MIL events
                           uint64_t *nEvtsSnd,                   // number of MIL telegrams sent
                           uint64_t *nEvtsRec,                   // number of MIL telegrams received
                           uint32_t *nEvtsLate,                  // number of translated events that could not be delivered in time
                           uint32_t *comLatency,                 // latency for messages received from via ECA (tDeadline - tNow)) [ns]
                           int      printFlag                    // print info to screen 
                           );

  // get common properties from firmware, returns error code
  uint32_t wrmil_common_read(uint64_t ebDevice,                  // EB device
                             uint64_t *statusArray,              // array with status bits
                             uint32_t *state,                    // state
                             uint32_t *nBadStatus,               // # of bad status incidents
                             uint32_t *nBadState,                // # of bad state incidents
                             uint32_t *version,                  // FW version
                             uint32_t *nTransfer,                // # of transfer
                             int      printDiag                  // prints info on common firmware properties to stdout
                             );
  
  // uploads configuration, returns error code
  uint32_t wrmil_upload(uint64_t ebDevice,                       // EB device
                        uint32_t utcTrigger,                     // the MIL event that triggers the generation of UTC events
                        uint32_t utcUtcDelay,                    // delay [us] between the 5 generated UTC MIL events
                        uint32_t trigUtcDelay,                   // delay [us] between the trigger event and the first UTC (and other) generated events
                        uint32_t gid,                            // timing group ID for which the gateway is generating MIL events (example: 0x12c is SIS18)
                        int32_t  latency,                        // MIL event is generated 100us+latency after the WR event. The value of latency can be negative
                        uint64_t utcOffset,                      // delay [ms] between the TAI and the MIL-UTC, high word   
                        uint32_t requestFill,                    // if this is written to 1, the gateway will send a fill event as soon as possible
                        uint32_t milDev,                         // MIL device for sending MIL messages; 0: MIL Piggy; 1..: SIO in slot 1..
                        uint32_t milMon                          // 1: monitor MIL events; 0; don't monitor MIL events          
                        );

  // commands requesting state transitions
  void wrmil_cmd_configure(uint64_t ebDevice);                   // to state 'configured'
  void wrmil_cmd_startop(uint64_t ebDevice);                     // to state 'opready'
  void wrmil_cmd_stopop(uint64_t ebDevice);                      // back to state 'configured'
  void wrmil_cmd_recover(uint64_t ebDevice);                     // try error recovery
  void wrmil_cmd_idle(uint64_t ebDevice);                        // to state idle
  
  // commands for normal operation
  void wrmil_cmd_cleardiag(uint64_t ebDevice);                   // clear diagnostic data
  void wrmil_cmd_submit(uint64_t ebDevice);                      // submit pending context
  void wrmil_cmd_clearConfig(uint64_t ebDevice);                 // clear all context data

  
#ifdef __cplusplus
}
#endif 

#endif

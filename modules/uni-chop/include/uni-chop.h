#ifndef _UNICHOP_
#define _UNICHOP_

#include <common-defs.h>

// this file is structured in two parts
// 1st: definitions of general things like error messages
// 2nd: definitions for data exchange via DP RAM

// ****************************************************************************************
// general things
// ****************************************************************************************

// (error) status, each status will be represented by one bit, bits will be ORed into a 32bit word
#define UNICHOP_STATUS_LATEMESSAGE            16   // late timing message received
#define UNICHOP_STATUS_BADSETTING             17   // bad setting data
#define UNICHOP_STATUS_MIL                    18   // an error on MIL hardware occured (MIL piggy etc...)

// activity requested by ECA Handler, the relevant codes are also used as "tags"
#define UNICHOP_ECADO_TIMEOUT    COMMON_ECADO_TIMEOUT
#define UNICHOP_ECADO_UNKOWN                   1   // unkown activity requested (unexpected action by ECA)
#define UNICHOP_ECADO_STRAHLWEG_WRITE      0xfa0   // timing message received from host; writes data to chopper control; param 0..15: Strahlwegregister; param 16..31: Strahlwegmaske
#define UNICHOP_ECADO_STRAHLWEG_READ       0xfa1   // timing message received from host; request read data from chopper control
#define UNICHOP_ECADO_RPG_WRITE            0xfa2   // timing message received from host; writes data to Rahmenpulsgeneratoren; param 48..63 HSI start..stop, 32..47 HLI start..stop, 16..31 IQL start..stop, 0..15 IQR start..stop
#define UNICHOP_ECADO_DIAG_MIL_WRITE       0xfa8   // diagnostic timing message sent by firmware for each MIL write; param: 48..55 status,  40..47 slot, 32..39 ifb addr, 24..31 mod addr, 16..23: reg addr, 0..15 data
#define UNICHOP_ECADO_DIAG_MIL_READ        0xfa9   // diagnostic timing message sent by firmware for each MIL read; param ...
#define UNICHOP_ECADO_DIAG_STRAHLWEG_READ  0xfaa   // diagnostic timing message sent by firmware after Strahlweg info has been read; param 0..15: Strahlwegregister; param 16..31: Strahlwegmaske

// commands from the outside

// GIDs
#define GID_INVALID                          0x0   // invalid GID
#define GID_LOCAL                          0xff0   // internal: group for local timing messages received/sent exchanged between host and lm32 firmware
#define GID_DIAG_MIL                       0xff1   // internal: timing messages for MIL monitoring

// specialities
// part below provided by Ludwig Hechler and Stefan Rauch
// interface boards: common function codes
#define IFB_FC_ADDR_BUS_W                   0x11   // function code, address bus write
#define IFB_FC_DATA_BUS_W                   0x10   // function code, data bus write
#define IFB_FC_DATA_BUS_R                   0x90   // function code, data bus read

// interface board: main chopper control
#define IFB_ADDR_CU                         0x60   // MIL address of chopper IF; FG 380.221

// modulebus module for Strahlwege
#define MOD_LOGIC1_ADDR                     0x09   // logic module 1 (Strahlwege et al),  module bus address, FG 450.410/411
#define MOD_LOGIC1_REG_STRAHLWEG_REG        0x60   // logic module 1, register/address for Strahlwegregister
#define MOD_LOGIC1_REG_STRAHLWEG_MASK       0x62   // logic module 1, register/subaddress for Strahlwegmaske
#define MOD_LOGIC1_REG_STATUSGLOBAL         0x66   // logic module 1, register/subaddress for global status, contains version number (2024: 0x14)

// modulebus modules for Rahmenpulsgeneratoren
#define MOD_RPG_IQR_ADDR                    0x01   // Rahmenpulsgenerator IQL, module bus address, FG 450.681
#define MOD_RPG_IQL_ADDR                    0x02   // Rahmenpulsgenerator IQL, module bus address, FG 450.681
#define MOD_RPG_HLI_ADDR                    0x03   // Rahmenpulsgenerator HLI, module bus address, FG 450.681
#define MOD_RPG_HSI_ADDR                    0x04   // Rahmenpulsgenerator HSI, module bus address, FG 450.681
#define MOD_RPG_XXX_STARTEVT_REG            0x12   // Rahmenpulsgeneratoren, register/subaddress for start event
#define MOD_RPG_XXX_STOPEVT_REG             0x14   // Rahmenpulsgeneratoren, register/subaddress for stop event

// interrupts
//#define UNICHOP_GW_MSI_LATE_EVENT         0x1
//#define UNICHOP_GW_MSI_STATE_CHANGED      0x2
//#define UNICHOP_GW_MSI_EVENT              0x3  // ored with (mil_event_number << 8)

// constants
#define UNICHOP_MILMODULE_ACCESST     100000   // time required to access (read/write) HW via MIL<->Modulebus  [ns]

// default values

// ****************************************************************************************
// DP RAM
// ****************************************************************************************

// offsets
// set values
#define UNICHOP_SHARED_SET_MIL_DEV           (COMMON_SHARED_END                   + _32b_SIZE_)  // MIL device for sending MIL messages; 0: MIL Piggy; 1..: SIO in slot 1..

// get values
#define UNICHOP_SHARED_GET_N_MIL_SND_HI      (UNICHOP_SHARED_SET_MIL_DEV          + _32b_SIZE_)  // number of sent MIL telegrams, high word
#define UNICHOP_SHARED_GET_N_MIL_SND_LO      (UNICHOP_SHARED_GET_N_MIL_SND_HI     + _32b_SIZE_)  // number of sent MIL telegrams, low word
#define UNICHOP_SHARED_GET_N_MIL_SND_ERR     (UNICHOP_SHARED_GET_N_MIL_SND_LO     + _32b_SIZE_)  // number of failed MIL writes
#define UNICHOP_SHARED_GET_N_EVTS_REC_HI     (UNICHOP_SHARED_GET_N_MIL_SND_ERR    + _32b_SIZE_)  // number of received timing messages with Strahlweg data, high word
#define UNICHOP_SHARED_GET_N_EVTS_REC_LO     (UNICHOP_SHARED_GET_N_EVTS_REC_HI    + _32b_SIZE_)  // number of received timing messages with Strahlweg data, low word

// diagnosis: end of used shared memory
#define UNICHOP_SHARED_END                   (UNICHOP_SHARED_GET_N_EVTS_REC_LO    + _32b_SIZE_) 

#endif

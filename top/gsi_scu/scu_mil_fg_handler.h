/*!
 * @file scu_mil_fg_handler.h
 * @brief Module for handling all MIL function generators and MIL DAQs
 * @copyright GSI Helmholtz Centre for Heavy Ion Research GmbH
 * @author Ulrich Becker <u.becker@gsi.de>
 * @date 04.02.2020
 * Outsourced from scu_main.c
 */
#ifndef _SCU_MIL_FG_HANDLER_H
#define _SCU_MIL_FG_HANDLER_H
#if !defined( CONFIG_MIL_FG ) && !defined(__DOCFSM__)
  #error Macro CONFIG_MIL_FG has to be defined!
#endif
#ifndef __DOCFSM__
/* Headers will not need for FSM analysator "docfsm" */
  #include "scu_main.h"
  #include "scu_fg_macros.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

extern FG_CHANNEL_T g_aFgChannels[MAX_FG_CHANNELS]; //!!

/*!
 * @brief Message queue for MIL-FGs filled by interrupt.
 */
extern SW_QUEUE_T g_queueMilFg;


#ifdef _CONFIG_VARIABLE_MIL_GAP_READING
 #ifndef CONFIG_READ_MIL_TIME_GAP
  #error CONFIG_READ_MIL_TIME_GAP has to be defined when _CONFIG_VARIABLE_MIL_GAP_READING is defined!
 #endif
 /*!
  * @brief MIL gap-reading time in milliseconds.
  * 
  * This variable is valid for all detected MIL function generator channels.
  * @note The value of zero means that the gap-reading is disabled.
  */
 extern unsigned int g_gapReadingTime;

 /*!
  * @brief Initializing value for global variable g_gapReadingTime
  *        in milliseconds.
  * @see g_gapReadingTime
  */
 #ifndef DEFAULT_GAP_READING_INTERVAL
  #define DEFAULT_GAP_READING_INTERVAL 0
 #endif
#endif /* ifdef _CONFIG_VARIABLE_MIL_GAP_READING */

/*! ---------------------------------------------------------------------------
 * @ingroup MIL_FSM
 * @brief Mecro declares a state of a Finite-State-Machine. \n
 *        Helper macro for documenting the FSM by the FSM-visualizer DOCFSM.
 * @see FG_STATE_T
 * @see https://github.com/UlrichBecker/DocFsm
 */
#define FSM_DECLARE_STATE( state, attr... ) state

/*! ---------------------------------------------------------------------------
 * @ingroup MIL_FSM
 * @brief Declaration of the states of the task- FSM
 * @see https://github.com/UlrichBecker/DocFsm
 */
typedef enum
{
   FSM_DECLARE_STATE( ST_WAIT,            label='Wait for message', color=blue ),
   FSM_DECLARE_STATE( ST_PREPARE,         label='Request MIL-IRQ-flags\nclear old IRQ-flags', color=cyan ),
   FSM_DECLARE_STATE( ST_FETCH_STATUS,    label='Read MIL-IRQ-flags', color=green ),
   FSM_DECLARE_STATE( ST_HANDLE_IRQS,     label='Send data to\nfunction generator\nif IRQ-flag set', color=red ),
   FSM_DECLARE_STATE( ST_DATA_AQUISITION, label='Request MIL-DAQ data\nif IRQ-flag set', color=cyan ),
   FSM_DECLARE_STATE( ST_FETCH_DATA,      label='Read MIL-DAQ data\nif IRQ-flag set',color=green )
} FG_STATE_T;


#ifdef _CONFIG_DBG_MIL_TASK
void  dbgPrintMilTaskData( void );
#else
#define dbgPrintMilTaskData()
#endif

/*! ---------------------------------------------------------------------------
 * @brief helper function which clears the state of MIL-bus after malfunction
 */
void fgMilClearHandlerState( const unsigned int socket );

/*! ---------------------------------------------------------------------------
 * @ingroup MIL_FSM
 * @brief
 */
typedef struct
{  /*!
    * @brief Saved irq state, becomes initialized in function milGetStatus()
    *        respectively scub_get_task_mil() or get_task_mil()
    * @see milGetStatus
    * @see scub_get_task_mil
    * @see get_task_mil
    */
   int16_t   irqFlags;

   /*!
    * @brief Setvalue from the tuple sent
    */
   int       setvalue;

   /*!
    * @brief Timestamp of DAQ sampling.
    */
   uint64_t  daqTimestamp;
} FG_CHANNEL_TASK_T;

/*! -------------------------------------------------------------------------
 * @brief Payload data type for the message queue g_queueMilFg.
 *        A instance of this type becomes initialized by each interrupt.
 */
typedef struct
{ /*!
   * @brief Slave number respectively slot number of the controlling SIO-card
   *        when value > 0. When 0 then the the MIL extention is concerned.
   */
   unsigned int  slot;
   
   /*!
    * @brief Moment of interrupt which fills the queue of this object.
    */
   uint64_t      time;
} MIL_QEUE_T;

/*! --------------------------------------------------------------------------
 * @ingroup MIL_FSM
 * @brief Task data type for MIL-FGs and MIL-DAQs
 * @see TASK_T
 * @see milDeviceHandler
 */
typedef struct
{  /*!
    * @brief Current FSM state
    */
   FG_STATE_T        state;

   /*!
    * @brief Contains the last message generated by interrupt.
    */
   MIL_QEUE_T        lastMessage;

   /*!
    * @brief Continuation of loop index for channel.
    */
   unsigned int      lastChannel;

   /*!
    * @brief timeout counter
    */
   unsigned int      timeoutCounter;

   /*!
    * @brief Waiting time after interrupt.
    */
   uint64_t          waitingTime;

#ifdef CONFIG_USE_INTERRUPT_TIMESTAMP
   /*!
    * @brief Duration in nanoseconds since the last interrupt.
    * 
    * For measurement and debug purposes only. 
    */
   uint64_t          irqDurationTime;
#endif

   /*!
    * @see FG_CHANNEL_TASK_T
    */
   FG_CHANNEL_TASK_T aFgChannels[ARRAY_SIZE(g_aFgChannels)];
} MIL_TASK_DATA_T;

extern MIL_TASK_DATA_T g_aMilTaskData[5];

#if defined( CONFIG_READ_MIL_TIME_GAP ) && !defined(__DOCFSM__)
/*! ---------------------------------------------------------------------------
 * @ingroup MIL_FSM
 * @brief Returns true, when the states of all MIL-FSMs are in the state
 *        ST_WAIT.
 */
bool isMilFsmInST_WAIT( void );

/*! ---------------------------------------------------------------------------
 * @ingroup MIL_FSM
 * @brief Suspends the DAQ- gap reading. The gap reading becomes resumed once
 *        the concerning function generator has been sent its first data.
 */
void suspendGapReading( void );
#endif /* defined( CONFIG_READ_MIL_TIME_GAP ) */

/*! ---------------------------------------------------------------------------
 * @ingroup TASK
 * @ingroup MIL_FSM
 * @brief Pre-initializing of all MIL- task data.
 * 
 * @note This is necessary, especially after a LM32-reset because this
 *       objects are not in the .bss memory section.
 */
void milInitTasks( void );

/*! ---------------------------------------------------------------------------
 * @ingroup MIL_FSM
 * @ingroup TASK
 * @brief Task-function for handling all MIL-FGs and MIL-DAQs via FSM.
 * @param pThis pointer to the current task object
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
void milDeviceHandler( register TASK_T* pThis );

#ifdef __cplusplus
}
#endif
#endif /* _SCU_MIL_FG_HANDLER_H */
/*================================== EOF ====================================*/

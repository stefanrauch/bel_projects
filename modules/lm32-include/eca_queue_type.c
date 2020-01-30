/*!
 * @file eca_queue_type.c
 * @brief Initialization of  ECA register object type for Wishbone interface of VHDL entity
 * @author Ulrich Becker <u.becker@gsi.de>
 * @copyright   2020 GSI Helmholtz Centre for Heavy Ion Research GmbH
 * @date 30.01.2020
 */
#ifndef __lm32__
#error This module is for LM32 only!
#endif
#include <eca_queue_type.h>
#include <mini_sdb.h>
#define ECAQMAX           4  /*!<@brief  max number of ECA queues */

/*! ---------------------------------------------------------------------------
 * @see eca_queue_type.h
 */
ECA_QUEUE_ITEM_T* getECAQueue( const unsigned int id )
{
   sdb_location ECAQeueBase[ECAQMAX];
   uint32_t queueIndex = 0;

   find_device_multi( ECAQeueBase, &queueIndex, ARRAY_SIZE(ECAQeueBase),
                      ECA_QUEUE_SDB_VENDOR_ID, ECA_QUEUE_SDB_DEVICE_ID );

   ECA_QUEUE_ITEM_T* pEcaQueue = NULL;
   ECA_QUEUE_ITEM_T* pEcaTemp;
   for( uint32_t i = 0; i < queueIndex; i++ )
   {
      pEcaTemp = (ECA_QUEUE_ITEM_T*) getSdbAdr( &ECAQeueBase[i] );
      if( pEcaTemp->id == id )
         pEcaQueue = pEcaTemp;
   }

   return pEcaQueue;
}

/*================================== EOF ====================================*/

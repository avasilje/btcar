/*******************************************************************************
 *  Based on CSR uEnergy SDK 2.2.2
 *
 *  FILE
 *      gap_service.h
 *
 *  DESCRIPTION
 *      Header definitions for GAP service
 *
 ******************************************************************************/

#ifndef __GAP_SERVICE_H__
#define __GAP_SERVICE_H__

/*=============================================================================*
 *  SDK Header Files
 *============================================================================*/

#include <types.h>
#include <bt_event_types.h>

/*=============================================================================*
 *  Local Header Files
 *============================================================================*/

#include "user_config.h"

/*=============================================================================*
 *  Public Function Prototypes
 *============================================================================*/

/* This function is used to initialise GAP service data structure.*/
extern void GapDataInit(void);

/* This function handles read operation on GAP service attributes
 * maintained by the application
 */
extern void GapHandleAccessRead(GATT_ACCESS_IND_T *p_ind);

/* This function handles write operation on GAP service attributes
 * maintained by the application
 */
extern void GapHandleAccessWrite(GATT_ACCESS_IND_T *p_ind);

/* This function is used to read GAP specific data stored in NVM */
extern void GapReadDataFromNVM(uint16 *p_offset);

/* This function is used to write GAP specific data to NVM for 
 * the first time during application initialisation
 */
extern void GapInitWriteDataToNVM(uint16 *p_offset);

#ifdef __GAP_PRIVACY_SUPPORT__

/* This function is used to get the status of peripheral privacy flag */
extern bool IsGapPeripheralPrivacyEnabled(void);

/* This function is used to get the pointer to the reconnection address value */
extern BD_ADDR_T *GapGetReconnectionAddress(void);

/* This function is used to check whether a valid reconnection address has been
 * written by the remote device
 */
extern bool GapIsReconnectionAddressValid(void);

/* This function is used to set the peripheral privacy flag and update the same
 * in NVM
 */
extern void GapSetPeripheralPrivacyFlag(bool flag);

/* This function is used to set the reconnection address and update the same in
 * NVM
 */
extern void GapSetReconnectionAddress(uint8 *p_val);

#endif /* __GAP_PRIVACY_SUPPORT__ */

/* This function is used to check if the handle belongs to the GAP 
 * service
 */
extern bool GapCheckHandleRange(uint16 handle);

/* This function is used to get the reference to the 'g_device_name' 
 * array, which contains AD Type and device name
 */
extern uint8 *GapGetNameAndLength(uint16 *p_name_length);

#endif /* __GAP_SERVICE_H__ */

/*******************************************************************************
 *  Based on CSR uEnergy SDK 2.2.2
 *
 *  FILE
 *      scan_param_service.h
 *
 *  DESCRIPTION
 *      Header definitions for Scan Parameter service.
 *
 ******************************************************************************/

#ifndef __SCAN_PARAM_SERVICE_H__
#define __SCAN_PARAM_SERVICE_H__

/*=============================================================================*
 *  SDK Header Files
 *============================================================================*/

#include <types.h>
#include <bt_event_types.h>

/*=============================================================================*
Public Definitions
*=============================================================================*/

#define SCAN_PARAM_INVALID_INTERVAL         (0)
#define SCAN_PARAM_INVALID_WINDOW           (0)

#define SERVER_REQUIRES_REFRESH             (0)

/*=============================================================================*
 *  Public Function Prototypes
 *============================================================================*/

/* This function initializes scan parameters service data structure.*/
extern void ScanParamDataInit(void);

/* This function initializes scan parameters service data structure at 
 * chip reset
 */
extern void ScanParamInitChipReset(void);

/* This function handles read operation on scan parameters service attributes
 * maintained by the application
 */
extern void ScanParamHandleAccessRead(GATT_ACCESS_IND_T *p_ind);

/* This function handles write operation on scan parameters service attributes
 * maintained by the application
 */
extern void ScanParamHandleAccessWrite(GATT_ACCESS_IND_T *p_ind);

/* This function is used to read scan parameters service specific data stored in 
 * NVM
 */
extern void ScanParamReadDataFromNVM(bool bonded, uint16 *p_offset);

/* This function is used to check if the handle belongs to the scan parameters
 * service
 */
extern bool ScanParamCheckHandleRange(uint16 handle);

/* This function gets the scan interval and scan window supported by the client */
extern void ScanParamGetIntervalWindow(uint16 *p_interval, uint16 *p_window);

#ifndef __NO_IDLE_TIMEOUT__

/* This function sends Scan Refresh characteristic if notications are enabled on
 * the characteristic
 */
extern void ScanParamRefreshNotify(uint16 ucid);

#endif /* __NO_IDLE_TIMEOUT__ */

#endif /* __SCAN_PARAM_SERVICE_H__ */

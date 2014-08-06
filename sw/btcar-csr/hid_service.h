/*******************************************************************************
 *  Based on CSR uEnergy SDK 2.2.2
 *
 *  FILE
 *      hid_service.h
 *
 *  DESCRIPTION
 *      Header definitions for HID service
 *
 ******************************************************************************/

#ifndef __HID_SERVICE_H__
#define __HID_SERVICE_H__

/*=============================================================================*
 *  Local Header Files
 *============================================================================*/

#include "app_gatt.h"

/*=============================================================================*
 *  SDK Header Files
 *============================================================================*/

#include <types.h>
#include <bt_event_types.h>

/*=============================================================================*
 *  Public Definitions
 *============================================================================*/

/* Control operation values can be referred from http://developer.bluetooth.org/
 * gatt/characteristics/Pages/
 * CharacteristicViewer.aspx?u=org.bluetooth.characteristic.hid_control_point.xml
 */
/* Suspend command is sent from the host when it wants to enter power saving
 * mode. Exit suspend is used by host to resume normal operations.
 */
typedef enum
{
    hid_suspend      = 0,
    hid_exit_suspend = 1,
    hid_rfu

} hid_control_point_op;

/* Protocol mode values can be referred from http://developer.bluetooth.org/
 * gatt/characteristics/Pages/
 * CharacteristicViewer.aspx?u=org.bluetooth.characteristic.protocol_mode.xml
 */
/* When the HID host switches between boot mode and report mode, the protocol
 * mode characteristic is written with these values. (AV: boot mode not supported)
 */
typedef enum
{
    hid_boot_mode   = 0,
    hid_report_mode = 1

} hid_protocol_mode;

/*=============================================================================*
 *  Public Function Prototypes
 *============================================================================*/

/* This function initializes the data structure of HID service */
extern void HidDataInit(void);

/* This function initializes HID service data structure at chip reset */
extern void HidInitChipReset(void);

/* This function handles read operation on HID service attributes maintained by
 * the application
 */
extern void HidHandleAccessRead(GATT_ACCESS_IND_T *p_ind);

/* This function handles write operation on HID service attributes maintained by
 * the application
 */
extern void HidHandleAccessWrite(GATT_ACCESS_IND_T *p_ind);

/* This function checks whether notification has been enabled on a
 * report characteristic referred by the 'report_id'
 */
extern bool HidIsNotifyEnabledOnReportId(uint8 report_id);

/* This function checks whether notifications have been enabled on all the
 * report characteristics in HID service
 */
extern bool HidIsNotificationEnabled(void);

/* This function sends reports as notifications for the report characteristics
 * of HID service
 */
extern bool HidSendInputReport(uint16 ucid, uint8 report_id, uint8 *report);

/* This function reads HID service specific data stored in NVM */
extern void HidReadDataFromNVM(bool bonded, uint16 *p_offset);

/* This function checks if the handle belongs to the HID service */
extern bool HidCheckHandleRange(uint16 handle);

/* This function checks if the HID host has entered suspend state */
extern bool HidIsStateSuspended(void);

#endif /* __HID_SERVICE_H__ */

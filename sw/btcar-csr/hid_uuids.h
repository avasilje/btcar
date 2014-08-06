/*******************************************************************************
 *  Based on CSR uEnergy SDK 2.2.2
 *
 *  FILE
 *      hid_uuids.h
 *
 *  DESCRIPTION
 *      UUID MACROs for HID service
 *
 ******************************************************************************/

#ifndef __HID_UUIDS_H__
#define __HID_UUIDS_H__

/*=============================================================================*
 *         Public Definitions
 *============================================================================*/

/* Brackets should not be used around the value of a macro. The parser 
 * which creates .c and .h files from .db file doesn't understand
 * brackets and will raise syntax errors.
 */

/* For UUID values, refer http://developer.bluetooth.org/gatt/services/Pages/
 * ServiceViewer.aspx?u=org.bluetooth.service.generic_access.xml.
 */

#define HID_SERVICE_UUID                        0x1812

#define HID_INFORMATION_UUID                    0x2a4a

#define HID_REPORT_MAP_UUID                     0x2a4b

#define HID_CONTROL_POINT_UUID                  0x2a4c

#define HID_REPORT_UUID                         0x2a4d

#define HID_EXT_REPORT_REFERENCE_UUID           0x2907

#define HID_REPORT_REFERENCE_UUID               0x2908

#define HID_PROTOCOL_MODE_UUID                  0x2a4e

#endif /* __HID_UUIDS_H__ */

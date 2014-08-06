/*******************************************************************************
 *  Based on CSR uEnergy SDK 2.2.2
 *
 *  FILE
 *      gap_uuids.h
 *
 *  DESCRIPTION
 *      UUID MACROs for GAP service
 *
 ******************************************************************************/

#ifndef __GAP_UUIDS_H__
#define __GAP_UUIDS_H__

/*=============================================================================*
 *         Public Definitions
 *============================================================================*/

/* Brackets should not be used around the value of a macro. The parser 
 * which creates .c and .h files from .db file doesn't understand  brackets 
 * and will raise syntax errors.
 */

/* For UUID values, refer http://developer.bluetooth.org/gatt/services/
 * Pages/ServiceViewer.aspx?u=org.bluetooth.service.generic_access.xml
 */

#define UUID_GAP_SERVICE                 0x1800

#define UUID_DEVICE_NAME                 0x2A00

#define UUID_APPEARANCE                  0x2A01

#define UUID_PERIPHERAL_PRIVACY_FLAG     0x2A02

#define UUID_RECONNECTION_ADDRESS        0x2A03

/* Peripheral Preferred Connection Parameters */
#define UUID_PER_PREF_CONN_PARAMS        0x2A04

#endif /* __GAP_UUIDS_H__ */

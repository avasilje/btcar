/*******************************************************************************
 *  Based on CSR uEnergy SDK 2.2.2
 *
 * FILE
 *      scan_parameters_uuids.h
 *
 * DESCRIPTION
 *      UUID MACROs for Scan parameters service
 *
 ******************************************************************************/

#ifndef __SCAN_PARAMETERS_UUIDS_H__
#define __SCAN_PARAMETERS_UUIDS_H__

/*=============================================================================*
 *         Public Definitions
 *============================================================================*/

/* Brackets should not be used around the value of a macro. The parser which
 * creates .c and .h files from .db file doesn't understand brackets and will
 * raise syntax errors.
 */

/* For UUID values, refer http://developer.bluetooth.org/gatt/services/Pages/
 * ServiceViewer.aspx?u=org.bluetooth.service.scan_parameters.xml.
 */
 
/* Scan Parameter Service UUID */
#define SCAN_PARAM_SERVICE_UUID       0x1813

/* Scan Interval Window UUID */
#define SCAN_INTERVAL_WINDOW_UUID     0x2a4f

/* Scan Refresh UUID */
#define SCAN_REFRESH_UUID             0x2a31

#endif /* __SCAN_PARAMETERS_UUIDS_H__ */

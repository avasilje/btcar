/*******************************************************************************
 *  Based on CSR uEnergy SDK 2.2.2
 *
 *  FILE
 *    appearance.h
 *
 *  DESCRIPTION
 *    This file defines macros for commonly used appearance values, which are 
 *    defined by BT SIG.
 *
 ******************************************************************************/

#ifndef __APPEARANCE_H__
#define __APPEARANCE_H__

/*=============================================================================*
 *         Public Definitions
 *============================================================================*/

/* Brackets should not be used around the value of a macro. The parser 
 * which creates .c and .h files from .db file doesn't understand  brackets 
 * and will raise syntax errors.
 */

/* For UUID values, refer http://developer.bluetooth.org/gatt/characteristics/
 * Pages/CharacteristicViewer.aspx?u=org.bluetooth.characteristic.gap.
 * appearance.xml
 */
/* HID appearance value */
#define APPEARANCE_GENERAL_HID_VALUE        0x03C0

#endif /* __APPEARANCE_H__ */

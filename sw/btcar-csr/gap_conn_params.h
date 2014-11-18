/*******************************************************************************
 *  Based on CSR uEnergy SDK 2.2.2
 *
 *  FILE
 *      gap_conn_params.h
 *
 *  DESCRIPTION
 *      MACROs for conn param update values
 *
 ******************************************************************************/

#ifndef __GAP_CONN_PARAMS_H__
#define __GAP_CONN_PARAMS_H__

/*=============================================================================*
 *  Public Definitions
 *============================================================================*/

/* Idle timer value in Connected state. At the expiry of this timer,
 * the device will disconnect itself from the Host.
 */
#define CONNECTED_IDLE_TIMEOUT_VALUE          (30 * MINUTE)

/* Advertising parameters, time is expressed in microseconds and the firmware
 * will round this down to the nearest slot. Acceptable range is 20ms to 10.24s
 * and the minimum must be no larger than the maximum.This value needs to be 
 * modified at later stage as decided GPA for specific profile.
 *
 * To enable fast connections though the recommended range is between 20 ms to
 * 30 ms, but it has been observed that it is way too energy expensive. So, we
 * have decided to use 60 ms as the fast connection advertisement interval. For
 * reduced power connections, the recommended range is between 1s to 2.5 s.
 * Vendors will need to tune these values as per their requirements.
 */
#define FC_ADVERTISING_INTERVAL_MIN           (60 * MILLISECOND)
#define FC_ADVERTISING_INTERVAL_MAX           (60 * MILLISECOND)
    
#define RP_ADVERTISING_INTERVAL_MIN           (1280 * MILLISECOND)
#define RP_ADVERTISING_INTERVAL_MAX           (1280 * MILLISECOND)

#define FAST_CONNECTION_ADVERT_TIMEOUT_VALUE  (30 * SECOND)

/* Time for which Device will trigger slow undirected advertisements. Since,
 * limited discoverable mode is being used by Device when it is not bonded,
 * the maximum value of this macro can 30.72 seconds as specified in Bluetooth
 * Core Specification version 4.0, Section 16, Appendix A in the GAP
 * specification
 */

#define SLOW_CONNECTION_ADVERT_TIMEOUT_VALUE  (30 * SECOND)

/* Brackets should not be used around the value of macros that are used in .db
 * files. The parser which creates .c and .h files from .db file doesn't
 * understand  brackets and will raise syntax errors.
 */

/* Preferred connection parameters should be within the range specified in the 
 * Bluetooth specification.
 */
/* Minimum and maximum connection interval in number of frames */
#define PREFERRED_MAX_CON_INTERVAL            0x0006 /* 12.5 ms */
#define PREFERRED_MIN_CON_INTERVAL            0x0006 /*  ms */

/* Slave latency in number of connection intervals */
#define PREFERRED_SLAVE_LATENCY               0x0000 /* 0x64 => 100 conn_intervals. */

/* Supervision timeout (ms) = PREFERRED_SUPERVISION_TIMEOUT * 10 ms */
#define PREFERRED_SUPERVISION_TIMEOUT         0x04e2 /* 12.5 seconds. */

/* Max num of conn param update that we send in one connection*/
#define MAX_NUM_CONN_PARAM_UPDATE_REQS        2

#endif /* __GAP_CONN_PARAMS_H__ */

#ifndef __IO_AVRB_DEV_CMD_H__
#define __IO_AVRB_DEV_CMD_H__

/* Set of function to be called from IO module for each device specific cmd */
void cmd_io_sign(void);
void cmd_io_mled(void);
void cmd_io_loopback(void);

#endif


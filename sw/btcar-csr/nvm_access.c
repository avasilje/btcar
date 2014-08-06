/*******************************************************************************
 *  Based on CSR uEnergy SDK 2.2.2
 *
 *  FILE
 *      nvm_access.c
 *
 *  DESCRIPTION
 *      This file defines routines used by application to access NVM.
 *
 ******************************************************************************/

/*=============================================================================*
 *  SDK Header Files
 *============================================================================*/

#include <pio.h>
#include <nvm.h>
#include <i2c.h>

/*=============================================================================*
 *  Local Header Files
 *============================================================================*/

#include "nvm_access.h"
#include "app_gatt.h"

/*=============================================================================*
 *  Public Function Implementations
 *============================================================================*/

/*-----------------------------------------------------------------------------*
 *  NAME
 *      Nvm_Disable
 *
 *  DESCRIPTION
 *      This function is used to perform things necessary to save power on NVM 
 *      once the read/write operations are done.
 *
 *  RETURNS/MODIFIES
 *      Nothing.
 *
 *----------------------------------------------------------------------------*/

extern void Nvm_Disable(void)
{
    NvmDisable();
    /* Set the SCL and SDA pins of EEPROM to strong pull down */
    PioSetI2CPullMode(pio_i2c_pull_mode_strong_pull_down);

}

/*-----------------------------------------------------------------------------*
 *  NAME
 *      Nvm_Read
 *
 *  DESCRIPTION
 *      Read words from the NVM Store after preparing the NVM to be readable. 
 *      After the read operation, perform things necessary in slave 
 *      application to save power on NVM.
 *
 *      Read words starting at the word offset, and store them in the supplied
 *      buffer.
 *
 *      \param buffer  The buffer to read words into.
 *      \param length  The number of words to read.
 *      \param offset  The word offset within the NVM Store to read from.
 *
 *  RETURNS/MODIFIES
 *      Nothing
 *
 *----------------------------------------------------------------------------*/

void Nvm_Read(uint16* buffer, uint16 length, uint16 offset)
{
    sys_status result;

    /* If the NVM has been earlier disabled, firmware automatically enables
     * NVM before reading from it.
     */
    result = NvmRead(buffer, length, offset);
    /* Disable NVM to save power after read operation */
    Nvm_Disable();

    if(sys_status_success != result)
    {
        ReportPanic(app_panic_nvm_read);
    }

}

/*-----------------------------------------------------------------------------*
 *  NAME
 *      Nvm_Write
 *
 *  DESCRIPTION
 *      Write words to the NVM store after preparing the NVM to be writable. 
 *      After the write operation, perform things necessary in slave 
 *      application to save power on NVM.
 *
 *      Write words from the supplied buffer into the NVM Store, starting at the
 *      given word offset.
 *
 *      \param buffer  The buffer to write.
 *      \param length  The number of words to write.
 *      \param offset  The word offset within the NVM Store to write to.
 *
 *  RETURNS/MODIFIES
 *      Nothing
 *
 *----------------------------------------------------------------------------*/

void Nvm_Write(uint16* buffer, uint16 length, uint16 offset)
{
    sys_status result;

    /* If the NVM has been earlier disabled, firmware automatically enables
     * NVM before writing to it.
     */
    result = NvmWrite(buffer, length, offset);
    /* Disable NVM to save power after write operation */
    Nvm_Disable();

    if(sys_status_success != result)
    {
        ReportPanic(app_panic_nvm_write);
    }

}

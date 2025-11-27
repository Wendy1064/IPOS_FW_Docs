#include "abcc_software_port.h"
#include "usbd_cdc_if.h"
#include "cmsis_os.h"
#include <stdio.h>
#include <stdarg.h>

extern USBD_HandleTypeDef hUsbDeviceFS;

static osMutexId_t usbTxMutex = NULL;

/*
 * Thread-safe CDC printf
 * returns length printed (to satisfy ABCC_PORT_printf)
 */
int UsbPrintf_ret(const char *fmt, ...)
{
    if (usbTxMutex == NULL)
        usbTxMutex = osMutexNew(NULL);

    if (hUsbDeviceFS.dev_state != USBD_STATE_CONFIGURED)
        return 0;

    osMutexAcquire(usbTxMutex, osWaitForever);

    char buf[256];
    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(buf, sizeof(buf) - 1, fmt, args);
    va_end(args);

    if (len < 0) len = 0;
    if (len >= sizeof(buf)) len = sizeof(buf) - 1;
    buf[len] = 0;

    const int CHUNK = 64;
    int sent = 0;
    while (sent < len)
    {
        int chunk = len - sent;
        if (chunk > CHUNK)
            chunk = CHUNK;

        while (CDC_Transmit_FS((uint8_t*)&buf[sent], chunk) == USBD_BUSY)
            osDelay(1);

        sent += chunk;
        osDelay(2);
    }

    osMutexRelease(usbTxMutex);

    return len;
}

#ifndef ABCC_SOFTWARE_PORT_H_
#define ABCC_SOFTWARE_PORT_H_

#include "abcc_types.h"
#include <stdarg.h>

/* Forward declaration of your USB printf */
int UsbPrintf_ret(const char *fmt, ...);

/* Redirect ABCC driver prints to USB CDC */
#define ABCC_PORT_printf(...)      UsbPrintf_ret(__VA_ARGS__)
#define ABCC_PORT_vprintf(fmt, va) UsbPrintf_ret(fmt, va)

/* Critical section stubs (leave unchanged) */
#define ABCC_PORT_UseCritical()
#define ABCC_PORT_EnterCritical()
#define ABCC_PORT_ExitCritical()

#endif

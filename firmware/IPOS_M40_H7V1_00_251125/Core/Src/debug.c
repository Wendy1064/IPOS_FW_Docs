
#include "debug.h"
#include <stdarg.h>

static UART_HandleTypeDef *dbg_huart = NULL;
static char dbg_buf[128];

void DEBUG_Init(UART_HandleTypeDef *huart) {
    dbg_huart = huart;
}

void DEBUG_Printf(const char *fmt, ...) {
    if (!dbg_huart) return;
    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(dbg_buf, sizeof(dbg_buf), fmt, args);
    va_end(args);

    if (len > 0) {
        HAL_UART_Transmit(dbg_huart, (uint8_t*)dbg_buf, len, HAL_MAX_DELAY);
    }
}



#ifndef DEBUG_H
#define DEBUG_H

#include "main.h"
#include <stdio.h>

void DEBUG_Init(UART_HandleTypeDef *huart);
void DEBUG_Printf(const char *fmt, ...);

#endif

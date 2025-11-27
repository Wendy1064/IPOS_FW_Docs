

#ifndef COMMAND_PARSER_H
#define COMMAND_PARSER_H

#include <stdint.h>

void CP_ByteReceived(uint8_t c);
void CP_ParseMessage(uint8_t *msg, uint8_t len);

void CP_HandleWrite(uint8_t varID, uint16_t value);
void CP_HandleRead(uint8_t varID);

#endif

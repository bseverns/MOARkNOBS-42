#ifndef COMMAND_QUEUE_H
#define COMMAND_QUEUE_H

#include <cstddef>

void pollSerialInput();
bool dequeueSerialCommand(char *outBuffer, size_t outBufferSize);

#if defined(UNIT_TEST)
void testOnly_resetCommandQueue();
void testOnly_enqueueSerialCommand(const char *line);
void testOnly_ingestSerialByte(char received);
#endif

#endif // COMMAND_QUEUE_H

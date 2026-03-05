#ifndef COMMAND_QUEUE_H
#define COMMAND_QUEUE_H

#include <cstddef>

void pollSerialInput();
bool dequeueSerialCommand(char *outBuffer, size_t outBufferSize);

#endif // COMMAND_QUEUE_H

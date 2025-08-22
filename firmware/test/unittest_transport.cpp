#include "unittest_transport.h"

extern "C" {
void unityOutputStart(unsigned long baudrate);
void unityOutputChar(unsigned int c);
void unityOutputFlush(void);
void unityOutputComplete(void);
}

void unittest_uart_begin() {
    // Kick off the line using same highway as unity_output.
    unityOutputStart(115200);
}

void unittest_uart_putchar(char c) { unityOutputChar(static_cast<unsigned int>(c)); }

void unittest_uart_flush() { unityOutputFlush(); }

void unittest_uart_end() { unityOutputComplete(); }

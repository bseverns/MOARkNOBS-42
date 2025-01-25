#ifndef MOCK_ARDUINO_H
#define MOCK_ARDUINO_H

#include <iostream>
#include <string>
#include <chrono>
#include <thread>

// Mock Pin Modes
#define INPUT 0
#define OUTPUT 1
#define LOW 0
#define HIGH 1

// Mock `millis` and `delay`
inline unsigned long millis() {
    static auto start = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count();
}

inline void delay(unsigned long ms) {
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

// Mock `digitalRead` and `digitalWrite`
inline int digitalRead(int pin) { return HIGH; }
inline void digitalWrite(int pin, int value) {
    std::cout << "Pin " << pin << " set to " << (value ? "HIGH" : "LOW") << std::endl;
}

// Mock Serial
class MockSerial {
public:
    void begin(int baudRate) {
        std::cout << "Serial initialized at " << baudRate << " baud" << std::endl;
    }
    void println(const std::string &message) {
        std::cout << message << std::endl;
    }
    bool available() { return false; }
    char read() { return '\n'; }
};

extern MockSerial Serial;

#endif // MOCK_ARDUINO_H

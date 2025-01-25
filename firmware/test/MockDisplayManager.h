#ifndef MOCK_DISPLAY_MANAGER_H
#define MOCK_DISPLAY_MANAGER_H

#include <iostream>

class MockDisplayManager {
    uint8_t i2cAddress;
    uint8_t cols;
    uint8_t rows;

public:
    MockDisplayManager(uint8_t address, uint8_t cols, uint8_t rows)
        : i2cAddress(address), cols(cols), rows(rows) {}

    void begin() {
        std::cout << "Mock Display Manager initialized with " << (int)cols << "x" << (int)rows
                  << " at I2C address " << (int)i2cAddress << std::endl;
    }

    void showText(const std::string &text) {
        std::cout << "Display: " << text << std::endl;
    }
};

#endif // MOCK_DISPLAY_MANAGER_H

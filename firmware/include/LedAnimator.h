#ifndef LEDANIMATOR_H
#define LEDANIMATOR_H

#include "LedMode.h"
#include "Globals.h"
#include <array>
#include <cstdint>

class LEDManager;

class LedAnimator {
  public:
    explicit LedAnimator(LEDManager &leds);

    void setMode(LedMode mode);
    LedMode getMode() const;
    void cycleMode();

    void setPotTarget(uint8_t index, uint8_t value);
    void setEnvelopeTarget(uint8_t index, uint8_t value);

    void tick(unsigned long nowMs, bool clockTick, bool diagnosticMode);

  private:
    struct ChannelState {
        uint8_t target = 0;
        uint8_t peak = 0;
        float trail = 0.0f;
        unsigned long holdUntil = 0;
    };

    uint8_t computeLevel(ChannelState &state, uint8_t raw, unsigned long nowMs);
    void paintPot(uint8_t index, uint8_t level);
    void paintEnvelope(uint8_t index, uint8_t level);

    std::array<ChannelState, NUM_POTS> potStates{};
    std::array<ChannelState, NUM_ENVELOPES> envelopeStates{};
    LEDManager &ledManager;
    LedMode mode = LedMode::Static;
    float clockPulseStrength = 0.0f;
    unsigned long diagLastCycle = 0;
};

#endif // LEDANIMATOR_H

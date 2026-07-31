#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>

#include "LFO.h"
#include "LFOClock.h"

struct ProfileData;

class MIDIHandler;

/*
Built-in modulation destinations for LFO output.
*/
enum class LFOInternalTarget : uint8_t {
    EfGainTrim = 0,
    ArpSwing,
    VelocityShift,
    NoteChance,
    ArpGate,
    JitterDepth,
    JitterSmoothness
};

/*
Bus values that are consumed by internal modules each loop.
*/
struct LFOBus {
    float efGainTrim = 0.0f;       // EF gain trim modulation (-1..1)
    float arpSwing = 0.0f;         // Arp swing modulation (-1..1)
    float velocityShift = 0.0f;    // Note velocity offset modulation (-1..1)
    float noteChance = 0.0f;       // Note chance modulation (-1..1)
    float arpGate = 0.0f;          // Arp gate percent modulation (-1..1)
    float jitterDepth = 0.0f;      // Jitter depth modulation (-1..1)
    float jitterSmoothness = 0.0f; // Jitter smoothness modulation (-1..1)
};

/*
Manages multiple LFO instances and routes their outputs.
Supports internal targets, MIDI CC (7/14-bit), and OSC callbacks.
*/
class LFOManager {
  public:
    // Maximum number of managed LFOs.
    static constexpr size_t kMaxLFOs = 2;

    /*
    Route definition for a single LFO output.
    Includes throttling state so outbound MIDI stays under 120 msgs/sec.
    */
    struct Route {
        enum class Type : uint8_t { Internal = 0, MidiCC7, MidiCC14, Osc, SlotValue };
        Type type = Type::Internal; // Route destination type
        uint8_t lfoIndex = 0;       // Which LFO to read
        float depth = 1.0f;         // Depth applied to the LFO output

        LFOInternalTarget target = LFOInternalTarget::EfGainTrim; // Internal target
        uint8_t slotIndex = 0; // Slot target for SlotValue routes

        uint8_t channel = 1;    // MIDI channel for CC routes
        uint8_t ccMsb = 0;      // CC MSB for 14-bit routes
        uint8_t ccLsb = 32;     // CC LSB for 14-bit routes
        int8_t amount = 100;    // Signed route amount (-100..100)
        uint8_t minValue = 0;   // Output floor for transport routes
        uint8_t maxValue = 127; // Output ceiling for transport routes

        unsigned long lastSendMs = 0; // Last send time for throttling
        int lastValue = -1;           // Last sent integer value to avoid repeats
    };

    // Initialize defaults for the LFO pool.
    LFOManager();

    // Attach MIDI handler for outbound CC routes.
    void attachMIDI(MIDIHandler *midi);
    // Provide a callback that emits a slot-routed LFO value.
    void setSlotValueCallback(
        std::function<void(uint8_t lfoIndex, uint8_t slotIndex, uint8_t value)> cb);
    // Backward-compatible adapter for consumers that do not need source identity.
    void setSlotValueCallback(std::function<void(uint8_t slotIndex, uint8_t value)> cb);
    // Provide an OSC callback for external routing.
    void setOscCallback(void (*cb)(uint8_t index, float value));

    // Access a specific LFO instance (mutable).
    LFO &lfo(size_t index);
    // Access a specific LFO instance (const).
    const LFO &lfo(size_t index) const;

    // Route an LFO to an internal modulation target.
    void addInternalRoute(uint8_t lfoIndex, LFOInternalTarget target, float depth = 1.0f,
                          int8_t amount = 100, uint8_t minValue = 0, uint8_t maxValue = 127);
    // Route an LFO to a 7-bit MIDI CC.
    void addMidiCC7Route(uint8_t lfoIndex, uint8_t cc, uint8_t channel, float depth = 1.0f,
                         int8_t amount = 100, uint8_t minValue = 0, uint8_t maxValue = 127);
    // Route an LFO to a 14-bit MIDI CC pair.
    void addMidiCC14Route(uint8_t lfoIndex, uint8_t ccMsb, uint8_t ccLsb, uint8_t channel,
                          float depth = 1.0f, int8_t amount = 100, uint8_t minValue = 0,
                          uint8_t maxValue = 127);
    // Route an LFO to the OSC callback.
    void addOscRoute(uint8_t lfoIndex, float depth = 1.0f, int8_t amount = 100,
                     uint8_t minValue = 0, uint8_t maxValue = 127);
    // Route an LFO through a slot's configured MIDI parameter.
    void addSlotValueRoute(uint8_t lfoIndex, uint8_t slotIndex, float depth = 1.0f,
                           int8_t amount = 100, uint8_t minValue = 0, uint8_t maxValue = 127);

    // Clear all configured routes.
    void clearRoutes();
    // Update all LFOs and flush route outputs.
    void update(unsigned long nowMs);

    // Read the current internal modulation bus.
    const LFOBus &bus() const { return bus_; }
    // Read the last normalized output for a given LFO.
    float normalizedValue(uint8_t index) const;
    // Read the oscillator as a centered, depth-scaled value (-1..1),
    // independent of its transport polarity setting.
    float signedValue(uint8_t index) const;
    // Return the number of configured routes.
    size_t routeCount() const { return routes_.size(); }
    // Fetch a route snapshot by index.
    bool getRoute(size_t index, Route &route) const;
    bool slotIsRouted(uint8_t slotIndex) const;
    // Replace all routes with the provided list.
    void setRoutes(const Route *routes, size_t count);
    // Apply profile-derived snapshot data (LFO state + routes).
    void applyProfile(const ProfileData &profile);
    // Reset phase and timing baselines after loading a new LFO profile.
    void resetTiming();

  private:
    // Apply an internal route to the modulation bus.
    void applyInternalRoute(const Route &route, float value);
    // Map normalized route output through signed amount and min/max.
    float shapeRouteNormalized(const Route &route, float normalized) const;
    // Map normalized route output to a 7-bit MIDI value.
    uint8_t routeMidiValue7(const Route &route, float normalized) const;
    // Map normalized route output to a 14-bit MIDI value.
    int routeMidiValue14(const Route &route, float normalized) const;
    // Conditionally send MIDI data with throttling.
    void maybeSendMidi(Route &route, float normalized, unsigned long nowMs);
    // Conditionally emit slot-targeted modulation with throttling.
    void maybeSendSlotValue(Route &route, float normalized, unsigned long nowMs);
    // Send OSC callback if installed.
    void maybeSendOsc(const Route &route, float normalized);

    std::array<LFO, kMaxLFOs> lfos_{}; // LFO instances
    std::vector<Route> routes_;        // Active routes
    LFOClock clock_;                   // Shared clock sync adapter
    MIDIHandler *midi_ = nullptr;      // MIDI output (not owned)
    std::function<void(uint8_t, uint8_t, uint8_t)> slotValueCallback_ = nullptr;
    void (*oscCallback_)(uint8_t, float) = nullptr; // OSC callback hook
    unsigned long lastUpdateMs_ = 0;                // Last update timestamp (ms)
    LFOBus bus_{};                                  // Current internal bus values
    std::array<float, kMaxLFOs> lastNormalized_{};  // Last normalized outputs
    std::array<float, kMaxLFOs> lastSigned_{};      // Centered outputs for slot lanes
};

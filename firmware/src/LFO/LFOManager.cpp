#include "LFO/LFOManager.h"

#include "ConfigManager.h"
#include "MIDIHandler.h"
#include "TimeUtils.h"
#include <algorithm>
#include <cmath>

namespace {
// Throttle outbound MIDI so we stay under the 120 msgs/sec target.
constexpr unsigned long kMinSendIntervalMs = 9;

// Clamp helper for route depth values.
float clampDepth(float depth) {
    if (depth < 0.0f)
        return 0.0f;
    if (depth > 1.0f)
        return 1.0f;
    return depth;
}
} // namespace

// Start with a sane timestamp so the first update computes a small dt.
LFOManager::LFOManager() {
    // Prime the timestamp so the first update has a sane delta.
    lastUpdateMs_ = now();
}

// Bind MIDI and keep the LFO clock in sync.
void LFOManager::attachMIDI(MIDIHandler *midi) {
    midi_ = midi;
    clock_.attach(midi);
}

// Install an OSC callback for external modulation mirrors.
void LFOManager::setOscCallback(void (*cb)(uint8_t, float)) { oscCallback_ = cb; }

// Mutable access to one LFO slot in the manager's fixed bank.
LFO &LFOManager::lfo(size_t index) { return lfos_.at(index); }

// Read-only access to one LFO slot in the manager's fixed bank.
const LFO &LFOManager::lfo(size_t index) const { return lfos_.at(index); }

// Route an LFO to one of the internal modulation bus entries.
void LFOManager::addInternalRoute(uint8_t lfoIndex, LFOInternalTarget target, float depth) {
    Route route;
    route.type = Route::Type::Internal;
    route.lfoIndex = lfoIndex;
    route.target = target;
    route.depth = clampDepth(depth);
    routes_.push_back(route);
}

// Route an LFO to a 7-bit CC, on the given channel.
void LFOManager::addMidiCC7Route(uint8_t lfoIndex, uint8_t cc, uint8_t channel, float depth) {
    Route route;
    route.type = Route::Type::MidiCC7;
    route.lfoIndex = lfoIndex;
    route.ccMsb = cc;
    route.channel = channel;
    route.depth = clampDepth(depth);
    routes_.push_back(route);
}

// Route an LFO to a 14-bit CC pair (MSB/LSB).
void LFOManager::addMidiCC14Route(uint8_t lfoIndex, uint8_t ccMsb, uint8_t ccLsb, uint8_t channel,
                                  float depth) {
    Route route;
    route.type = Route::Type::MidiCC14;
    route.lfoIndex = lfoIndex;
    route.ccMsb = ccMsb;
    route.ccLsb = ccLsb;
    route.channel = channel;
    route.depth = clampDepth(depth);
    routes_.push_back(route);
}

// Route an LFO to the OSC callback.
void LFOManager::addOscRoute(uint8_t lfoIndex, float depth) {
    Route route;
    route.type = Route::Type::Osc;
    route.lfoIndex = lfoIndex;
    route.depth = clampDepth(depth);
    routes_.push_back(route);
}

// Clear all routing entries.
void LFOManager::clearRoutes() { routes_.clear(); }

// Update LFO state and emit outputs for all routes.
void LFOManager::update(unsigned long nowMs) {
    // Compute delta time for free-run updates.
    float dt = 0.0f;
    if (nowMs >= lastUpdateMs_) {
        dt = static_cast<float>(nowMs - lastUpdateMs_) * 0.001f;
    }
    lastUpdateMs_ = nowMs;

    // Pull MIDI clock ticks once per update.
    uint32_t tickDelta = clock_.consumeTickDelta();

    // Reset the internal bus and collect per-LFO values.
    bus_ = {};
    std::array<float, kMaxLFOs> lfoValues{};
    std::array<bool, kMaxLFOs> lfoBipolar{};
    for (size_t idx = 0; idx < lfos_.size(); ++idx) {
        LFO &lfo = lfos_[idx];
        // Advance each LFO in sync or free-run mode.
        if (lfo.isSyncEnabled()) {
            if (tickDelta > 0) {
                uint32_t ticks = LFOClock::ticksPerCycle(lfo.getSyncRatio());
                lfo.advanceClockTicks(tickDelta, ticks);
            }
        } else if (dt > 0.0f) {
            lfo.advanceFreeRun(dt);
        }
        lfoValues[idx] = lfo.value();
        lfoBipolar[idx] = lfo.isBipolar();
        // Cache normalized values for WebSerial telemetry.
        float normalized = lfoValues[idx];
        if (lfoBipolar[idx]) {
            normalized = lfoValues[idx] * 0.5f + 0.5f;
        }
        lastNormalized_[idx] = std::clamp(normalized, 0.0f, 1.0f);
    }

    // Fan out each route using the latest LFO values.
    for (Route &route : routes_) {
        if (route.lfoIndex >= lfos_.size())
            continue;
        float value = lfoValues[route.lfoIndex] * route.depth;
        float normalized = value;
        if (lfoBipolar[route.lfoIndex]) {
            normalized = value * 0.5f + 0.5f;
        }
        normalized = std::clamp(normalized, 0.0f, 1.0f);

        switch (route.type) {
        case Route::Type::Internal:
            applyInternalRoute(route, value);
            break;
        case Route::Type::MidiCC7:
        case Route::Type::MidiCC14:
            maybeSendMidi(route, normalized, nowMs);
            break;
        case Route::Type::Osc:
            maybeSendOsc(route, normalized);
            break;
        }
    }

    // Clamp bus values to safe modulation ranges.
    bus_.efGainTrim = std::clamp(bus_.efGainTrim, -1.0f, 1.0f);
    bus_.arpSwing = std::clamp(bus_.arpSwing, -1.0f, 1.0f);
    bus_.ledBrightness = std::clamp(bus_.ledBrightness, -1.0f, 1.0f);
}

// Read the last normalized value for telemetry or external consumers.
float LFOManager::normalizedValue(uint8_t index) const {
    if (index >= lastNormalized_.size())
        return 0.0f;
    return lastNormalized_[index];
}

bool LFOManager::getRoute(size_t index, Route &route) const {
    // Copy route data if the index is valid.
    if (index >= routes_.size()) {
        return false;
    }
    route = routes_[index];
    return true;
}

void LFOManager::setRoutes(const Route *routes, size_t count) {
    // Replace the route table with a caller-provided snapshot.
    clearRoutes();
    if (!routes || count == 0) {
        return;
    }
    for (size_t i = 0; i < count; ++i) {
        routes_.push_back(routes[i]);
    }
}

void LFOManager::applyProfile(const ProfileData &profile) {
    // Apply the LFO state snapshot collected from ConfigManager/firmware_main.
    const size_t lfoLimit = std::min<size_t>(PROFILE_LFO_COUNT, kMaxLFOs);
    for (size_t i = 0; i < lfoLimit; ++i) {
        const ProfileLfoSettings &snapshot = profile.lfos[i];
        LFO &lfo = lfos_[i];
        lfo.setShape(static_cast<LFOShape>(snapshot.shape));
        lfo.setFrequencyHz(snapshot.frequencyHz);
        lfo.setDepth(snapshot.depth);
        lfo.setBipolar(snapshot.bipolar != 0);
        lfo.setSyncEnabled(snapshot.syncEnabled != 0);
        lfo.setSyncRatio(static_cast<LFOSyncRatio>(snapshot.syncRatio));
    }

    clearRoutes();
    const size_t routeLimit =
        std::min<size_t>(static_cast<size_t>(profile.routeCount), PROFILE_MAX_ROUTES);
    for (size_t i = 0; i < routeLimit; ++i) {
        const ProfileLfoRoute &route = profile.routes[i];
        switch (static_cast<LFOManager::Route::Type>(route.type)) {
        case LFOManager::Route::Type::Internal:
            addInternalRoute(route.lfoIndex, static_cast<LFOInternalTarget>(route.target),
                             route.depth);
            break;
        case LFOManager::Route::Type::MidiCC7:
            addMidiCC7Route(route.lfoIndex, route.ccMsb, route.channel, route.depth);
            break;
        case LFOManager::Route::Type::MidiCC14:
            addMidiCC14Route(route.lfoIndex, route.ccMsb, route.ccLsb, route.channel, route.depth);
            break;
        case LFOManager::Route::Type::Osc:
            addOscRoute(route.lfoIndex, route.depth);
            break;
        }
    }
}

// Accumulate the value into the appropriate internal bus lane.
void LFOManager::applyInternalRoute(const Route &route, float value) {
    switch (route.target) {
    case LFOInternalTarget::EfGainTrim:
        bus_.efGainTrim += value;
        break;
    case LFOInternalTarget::ArpSwing:
        bus_.arpSwing += value;
        break;
    case LFOInternalTarget::LedBrightness:
        bus_.ledBrightness += value;
        break;
    }
}

// Emit MIDI CC values if the route is due and the value changed.
void LFOManager::maybeSendMidi(Route &route, float normalized, unsigned long nowMs) {
    if (!midi_)
        return;
    // Throttle outbound CC traffic to avoid MIDI floods.
    if (nowMs - route.lastSendMs < kMinSendIntervalMs)
        return;

    int midiValue = 0;
    // 14-bit CC uses MSB/LSB; clamp to 0..16383.
    if (route.type == Route::Type::MidiCC14) {
        midiValue = static_cast<int>(std::lround(normalized * 16383.0f));
        if (midiValue != route.lastValue) {
            route.lastValue = midiValue;
            route.lastSendMs = nowMs;
            uint8_t msb = static_cast<uint8_t>((midiValue >> 7) & 0x7F);
            uint8_t lsb = static_cast<uint8_t>(midiValue & 0x7F);
            midi_->sendControlChange(route.ccMsb, msb, route.channel);
            midi_->sendControlChange(route.ccLsb, lsb, route.channel);
        }
        return;
    }

    // 7-bit CC maps to 0..127.
    midiValue = static_cast<int>(std::lround(normalized * 127.0f));
    if (midiValue != route.lastValue) {
        route.lastValue = midiValue;
        route.lastSendMs = nowMs;
        midi_->sendControlChange(route.ccMsb, static_cast<uint8_t>(midiValue), route.channel);
    }
}

// Dispatch an OSC callback if configured.
void LFOManager::maybeSendOsc(const Route &route, float normalized) {
    if (!oscCallback_)
        return;
    oscCallback_(route.lfoIndex, normalized);
}

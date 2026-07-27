// Central hub for sending and receiving MIDI.
// Relays activity to the DisplayManager and validates data.
#ifndef MIDIHANDLER_H
#define MIDIHANDLER_H

#ifdef ARDUINO
#include "Arduino.h"
#else
class HardwareSerial;
#endif
#include <array>
#include <cstdint>
#include "DisplayManager.h"
#include "MIDITypes.h"
#include <MIDI.h>
#include "MidiTypeShim.h"
#if defined(USB_MIDI_STUB)
#include "usb_midi.h" // snag the stub from test/ when asked
inline constexpr bool HAS_USB_MIDI = true;
#elif defined(USB_MIDI) || defined(USB_MIDI_SERIAL)
#include <usb_midi.h>
inline constexpr bool HAS_USB_MIDI = true;
#else
inline constexpr bool HAS_USB_MIDI = false;
#endif

struct SystemDiagnostics;

/*
Thin wrapper around the Arduino and USB MIDI libraries.
*/
class MIDIHandler {
  public:
    static constexpr uint16_t kMaxOutgoingSysExBytes = 64;
    // Assign a DisplayManager so MIDI traffic can be displayed.
    void setDisplayManager(DisplayManager *dm) { _displayManager = dm; }

    // Hook in a diagnostics block so we can count dropped bytes and overruns.
    void setDiagnostics(SystemDiagnostics *diag) { _diagnostics = diag; }

    // Create a new MIDI handler with no side effects.
    MIDIHandler();

    // Set up the serial and USB MIDI interfaces. Call from setup().
    void begin();

    // Send a standard Control Change message.
    void sendControlChange(uint8_t control, uint8_t value, uint8_t channel);

    // Send a MIDI Note On message.
    void sendNoteOn(uint8_t note, uint8_t velocity, uint8_t channel);

    // Send a MIDI Note Off message.
    void sendNoteOff(uint8_t note, uint8_t velocity, uint8_t channel);

    // Sling a raw NRPN sequence (CC99/98 + CC6/38).
    void sendNRPN(uint16_t param, uint16_t value, uint8_t channel);

    // Sling a Registered Parameter Number sequence (CC101/100 + CC6/38).
    void sendRPN(uint16_t param, uint16_t value, uint8_t channel);

    // Last NRPN parsed from the wire.
    uint16_t lastNRPNParam() const { return _lastNRPNParam; }
    uint16_t lastNRPNValue() const { return _lastNRPNValue; }

    // Last RPN parsed from the wire.
    uint16_t lastRPNParam() const { return _lastRPNParam; }
    uint16_t lastRPNValue() const { return _lastRPNValue; }

    // Fire off a System Exclusive packet. `data` should include F0/F7.
    void sendSysEx(const uint8_t *data, uint16_t length);

    // Snapshot of the most recent SysEx payload.
    uint16_t lastSysExLength() const { return _lastSysExLength; }
    const uint8_t *lastSysExData() const { return _lastSysEx; }
    SysExType lastSysExType() const { return _lastSysExType; }
    uint8_t lastSysExSubId1() const { return _lastSysExSubId1; }
    uint8_t lastSysExSubId2() const { return _lastSysExSubId2; }

    // Poll both serial and USB for incoming MIDI bytes.
    void processIncomingMIDI();

    // Dispatch a parsed MIDI message to the appropriate handler.
    void handleMIDI(midi::MidiType type, uint8_t channel, uint8_t data1, uint8_t data2);

    // Convenience helpers for specific message types.
    void handleNoteOn(uint8_t channel, uint8_t note, uint8_t velocity);
    void handleNoteOff(uint8_t channel, uint8_t note, uint8_t velocity);
    void handleProgramChange(uint8_t channel, uint8_t program);
    void handleAftertouch(uint8_t channel, uint8_t pressure);
    void handlePitchBend(uint8_t channel, int16_t bend);
    void sendProgramChange(uint8_t program, uint8_t channel);
    void sendAftertouch(uint8_t pressure, uint8_t channel);
    void sendModWheel(uint8_t value, uint8_t channel);
    void sendPitchBend(int16_t bend, uint8_t channel);
    void sendClock();
    void flushUsbMidi();

    // Emit an internal MIDI clock pulse so in-box features stay in sync.
    void generateClockTick();

    // Public drain for high-priority scheduler task to prevent queue starvation.
    void serviceSerialQueuePublic() { serviceSerialQueue(); }

    // Total MIDI clock pulses observed since boot.
    uint32_t clockTickCount() const { return _clockTickCount; }

    // MIDI clock helpers.
    bool isClockTick();
    void clearClockTick();
    // Return true when a MIDI Start/Continue has been seen without Stop.
    bool isClockRunning() const { return _clockRunning; }
    // Return true when fresh external MIDI clock ticks have been observed recently.
    bool hasExternalClockSignal() const;
    // Smoothed BPM estimate derived from incoming external MIDI clock ticks.
    float externalClockBpm() const;

    // How many MIDI messages we've heard and blasted.
    uint32_t getRxCount() const { return _rxCount; }
    uint32_t getTxCount() const { return _txCount; }

  private:
    enum class SerialMessageType : uint8_t {
        ControlChange,
        NoteOn,
        NoteOff,
        ProgramChange,
        Aftertouch,
        PitchBend,
        Clock
    };

    struct SerialMessage {
        SerialMessageType type = SerialMessageType::Clock;
        uint8_t channel = 0;
        uint8_t data1 = 0;
        uint8_t data2 = 0;
        int16_t pitch = 0;
        uint8_t byteCount = 0;
    };

    static constexpr size_t kSerialQueueSize = 48;
    static constexpr uint32_t kSerialByteMicros = 320; // 31.25 kbps → 320 µs per byte

    bool clockTick = false;
    uint32_t _clockTickCount = 0;
    unsigned long lastExternalClock = 0;
    unsigned long lastObservedExternalTick = 0;
    unsigned long lastInternalTick = 0;
    float externalMsPerTick = 0.0f;
    bool _clockRunning = false; // Track Start/Continue vs Stop for sync-aware modules
    DisplayManager *_displayManager = nullptr;
    SystemDiagnostics *_diagnostics = nullptr;

    enum class ParameterSelection : uint8_t { None, Nrpn, Rpn };
    struct ParameterDecodeState {
        uint16_t param = 0;
        uint16_t value = 0;
        bool paramMsbReceived = false;
        ParameterSelection selection = ParameterSelection::None;
    };

    // Parameter selection and data-entry state is isolated per MIDI channel.
    // Interleaved RPN/NRPN traffic must never borrow another channel's MSB.
    std::array<ParameterDecodeState, 16> _parameterState{};

    // Last fully received NRPN for external inspection
    uint16_t _lastNRPNParam = 0;
    uint16_t _lastNRPNValue = 0;

    // Last fully received RPN for external inspection
    uint16_t _lastRPNParam = 0;
    uint16_t _lastRPNValue = 0;

    // SysEx stash for quick testing/debugging
    uint8_t _lastSysEx[64] = {0};
    uint16_t _lastSysExLength = 0;
    SysExType _lastSysExType = SysExType::ManufacturerSpecific;
    uint8_t _lastSysExSubId1 = 0;
    uint8_t _lastSysExSubId2 = 0;

    uint32_t _rxCount = 0;
    uint32_t _txCount = 0;

    std::array<SerialMessage, kSerialQueueSize> _serialQueue{};
    size_t _serialQueueHead = 0;
    size_t _serialQueueTail = 0;
    bool _serialQueueFull = false;
    uint32_t _lastSerialSendUs = 0;

    bool enqueueSerialMessage(const SerialMessage &msg);
    bool dequeueSerialMessage(SerialMessage &msg);
    bool serialQueueEmpty() const;
    size_t serialQueueSize() const;
    bool tryCoalesceSerialMessage(const SerialMessage &msg);
    void serviceSerialQueue();
    void dispatchSerialMessage(const SerialMessage &msg);
    SerialMessage makeControlChange(uint8_t channel, uint8_t control, uint8_t value) const;
    SerialMessage makeNoteOn(uint8_t channel, uint8_t note, uint8_t velocity) const;
    SerialMessage makeNoteOff(uint8_t channel, uint8_t note, uint8_t velocity) const;
    SerialMessage makeProgramChange(uint8_t channel, uint8_t program) const;
    SerialMessage makeAftertouch(uint8_t channel, uint8_t pressure) const;
    SerialMessage makePitchBend(uint8_t channel, int16_t bend) const;
    SerialMessage makeClock() const;

    void receiveNRPN(uint8_t channel, uint16_t param, uint16_t value);
    void receiveRPN(uint8_t channel, uint16_t param, uint16_t value);
    void handleSysEx(const uint8_t *data, uint16_t length);
    void handleClockTick();
    void observeExternalClockTick(unsigned long timestampMs);
};

#endif

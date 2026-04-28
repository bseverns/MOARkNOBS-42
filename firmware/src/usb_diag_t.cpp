#include <Arduino.h>
#include <imxrt.h>

namespace {
constexpr uint32_t kBaud = 115200;
constexpr uint8_t kSlotCount = 42;
constexpr uint8_t kEnvelopeCount = 6;
constexpr uint16_t kLedCount = 51;
constexpr uint16_t kConfigVersion = 6;
constexpr uint32_t kHeartbeatMs = 1000;
constexpr uint32_t kBlinkMs = 250;
constexpr size_t kCommandBufferSize = 96;

char commandBuffer[kCommandBufferSize] = {0};
size_t commandLength = 0;
uint32_t bootMs = 0;
uint32_t lastHeartbeatMs = 0;
uint32_t lastBlinkMs = 0;
bool ledState = false;

void printManifest() {
    Serial.print(F("{\"device_name\":\"MOARkNOBS-42 USB DIAG\","));
    Serial.print(F("\"fw_version\":\"usb_diag\","));
    Serial.print(F("\"git_sha\":\"diag\","));
    Serial.print(F("\"build_time\":\""));
    Serial.print(F(__DATE__));
    Serial.print(' ');
    Serial.print(F(__TIME__));
    Serial.print(F("\",\"schema_version\":"));
    Serial.print(kConfigVersion);
    Serial.print(F(",\"slot_count\":"));
    Serial.print(kSlotCount);
    Serial.print(F(",\"pot_count\":"));
    Serial.print(kSlotCount);
    Serial.print(F(",\"envelope_count\":"));
    Serial.print(kEnvelopeCount);
    Serial.print(F(",\"arg_method_count\":14,\"led_count\":"));
    Serial.print(kLedCount);
    Serial.println(F(",\"free_ram\":0,\"free_flash\":0,\"diagnostic\":true}"));
}

void printSchema() {
    Serial.println(F("{\"type\":\"object\",\"properties\":{\"slots\":{},\"efSlots\":{},\"filter\":{"
                     "},\"arg\":{},\"led\":{}}}"));
}

void printConfig() {
    Serial.print(F("{\"pots\":["));
    for (uint8_t i = 0; i < kSlotCount; ++i) {
        if (i) {
            Serial.print(',');
        }
        Serial.print(F("{\"channel\":1,\"cc\":"));
        Serial.print(i);
        Serial.print('}');
    }
    Serial.print(F("],\"slots\":["));
    for (uint8_t i = 0; i < kSlotCount; ++i) {
        if (i) {
            Serial.print(',');
        }
        Serial.print(F("{\"type\":\"OFF\",\"midiChannel\":1,\"data1\":"));
        Serial.print(i);
        Serial.print(F(",\"efIndex\":-1,\"active\":false,\"ef\":{\"index\":-1,\"filter_index\":0,"
                       "\"filter_name\":\"LINEAR\",\"frequency\":1000,\"q\":0.707,\"oversample\":4,"
                       "\"smoothing\":0.2,\"baseline\":0,\"gain\":1},\"arg\":{\"enabled\":false,"
                       "\"method\":0,\"method_name\":\"PLUS\",\"sourceA\":0,\"sourceB\":1}}"));
    }
    Serial.print(F("],\"efSlots\":["));
    for (uint8_t i = 0; i < kEnvelopeCount; ++i) {
        if (i) {
            Serial.print(',');
        }
        Serial.print(F("{\"slots\":[]}"));
    }
    Serial.println(F("],\"filter\":{\"type\":\"LINEAR\",\"freq\":1000,\"q\":0.707},\"arg\":{"
                     "\"method\":\"PLUS\",\"a\":0,\"b\":1,\"enable\":false},\"led\":{"
                     "\"brightness\":0,\"color\":\"#000000\",\"mode\":\"STATIC\"}}"));
}

void handleCommand(const char *command) {
    if (!command || !command[0]) {
        return;
    }
    if (strcmp(command, "HELLO") == 0) {
        Serial.println(F("{\"hello\":\"mn42\"}"));
    } else if (strcmp(command, "GET_MANIFEST") == 0) {
        printManifest();
    } else if (strcmp(command, "GET_SCHEMA") == 0) {
        printSchema();
    } else if (strcmp(command, "GET_CONFIG") == 0) {
        printConfig();
    } else if (strcmp(command, "PING") == 0) {
        Serial.println(F("{\"pong\":true}"));
    } else {
        Serial.print(F("{\"type\":\"error\",\"code\":\"unknown_command\",\"command\":\""));
        Serial.print(command);
        Serial.println(F("\"}"));
    }
}

void pollCommands() {
    while (Serial.available() > 0) {
        char c = static_cast<char>(Serial.read());
        if (c == '\r') {
            continue;
        }
        if (c == '\n' || commandLength >= kCommandBufferSize - 1) {
            commandBuffer[commandLength] = '\0';
            handleCommand(commandBuffer);
            commandLength = 0;
            commandBuffer[0] = '\0';
            continue;
        }
        commandBuffer[commandLength++] = c;
    }
}

void printHeartbeat() {
    Serial.print(F("{\"type\":\"diag_heartbeat\",\"uptime_ms\":"));
    Serial.print(millis() - bootMs);
    Serial.print(F(",\"reset_cause\":"));
    Serial.print(SRC_SRSR);
    Serial.println('}');
}
} // namespace

void setup() {
    pinMode(LED_BUILTIN, OUTPUT);
    bootMs = millis();
    Serial.begin(kBaud);
    delay(250);

    Serial.println(F("MN42 USB DIAG boot"));
    Serial.print(F("Reset cause: 0x"));
    Serial.println(SRC_SRSR, HEX);
    if (CrashReport) {
        Serial.println(F("CrashReport follows:"));
        Serial.print(CrashReport);
    }
    Serial.println(F("Commands: HELLO, GET_MANIFEST, GET_SCHEMA, GET_CONFIG, PING"));
}

void loop() {
    const uint32_t nowMs = millis();
    pollCommands();

    if (nowMs - lastBlinkMs >= kBlinkMs) {
        lastBlinkMs = nowMs;
        ledState = !ledState;
        digitalWrite(LED_BUILTIN, ledState ? HIGH : LOW);
    }

    if (Serial && nowMs - lastHeartbeatMs >= kHeartbeatMs) {
        lastHeartbeatMs = nowMs;
        printHeartbeat();
    }
}

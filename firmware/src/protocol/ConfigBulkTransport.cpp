#include "protocol/ConfigBulkTransport.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <cstdint>

#include "ConfigManager.h"
#include "DiagnosticRecord.h"
#include "Log.h"
#include "Utility.h"
#include "protocol/ConfigApplyDigest.h"
#include "protocol/ConfigJsonApply.h"
#include "protocol/ProtocolErrors.h"

// Stateful SET_ALL transport boundary: chunk staging, identity/idempotency,
// timeout/abort behavior, and ACK emission. ConfigJsonApply receives only a
// complete assembled payload and owns JSON parsing plus transactional mutation.
namespace {
Utility::BulkConfigAssembler bulkConfigAssembler;
uint32_t lastAckSequence = 0;
String lastAckChecksum;
String lastAppliedChecksum;
uint32_t lastStorageGeneration = 0;
DMAMEM StaticJsonDocument<Utility::kMaxBulkConfigSize> bulkApplyDoc;

struct BulkApplyIdentity {
    uint32_t sequence = 0;
    String configId;
};

void emitBulkIngestError(const String &ingestError, uint32_t hint) {
    DiagnosticRecord::recordConfigApplyResult(DiagnosticRecord::ConfigApplyStatus::Error, nullptr);
    if (ingestError == "overflow") {
        emitBulkError("overflow", "config payload too large", hint);
    } else if (ingestError == "orphan") {
        emitBulkError("orphan", "chunk missing frame start", hint);
    } else if (ingestError == "timeout") {
        emitBulkError("timeout", "incomplete config upload expired", hint);
    } else {
        emitBulkError("ingest", "failed to stage chunk", hint);
    }
}

bool resolveBulkApplyIdentity(JsonDocument &doc, BulkApplyIdentity &identity) {
    identity.sequence = doc["seq"].as<uint32_t>();
    if (identity.sequence == 0) {
        identity.sequence = bulkConfigAssembler.sequenceHint();
    }

    const char *configId = doc["config_id"] | nullptr;
    if (!configId || configId[0] == '\0') {
        configId = doc["checksum"] | nullptr;
    }

    const String &checksumHint = bulkConfigAssembler.checksumHint();
    if ((!configId || configId[0] == '\0') && checksumHint.length() > 0) {
        identity.configId = checksumHint;
    } else if (configId) {
        identity.configId = configId;
    } else {
        identity.configId = "";
    }

    if (identity.sequence == 0) {
        identity.sequence = lastAckSequence + 1;
    }

    if (identity.configId.length() == 0) {
        DiagnosticRecord::recordConfigApplyResult(DiagnosticRecord::ConfigApplyStatus::Error,
                                                  nullptr);
        emitBulkError("checksum", "missing checksum/config_id", identity.sequence);
        bulkConfigAssembler.reset();
        return false;
    }

    return true;
}

bool emitDuplicateBulkAckIfNeeded(const BulkApplyIdentity &identity) {
    if (identity.sequence != lastAckSequence || lastAckChecksum != identity.configId) {
        return false;
    }

    LOG_PRINTLN(Utility::formatAck(identity.configId.c_str(), identity.sequence,
                                   lastAppliedChecksum.c_str(), lastStorageGeneration));
    bulkConfigAssembler.reset();
    return true;
}

void commitBulkApplyAck(const BulkApplyIdentity &identity) {
    lastAckSequence = identity.sequence;
    lastAckChecksum = identity.configId;
    lastAppliedChecksum = ConfigApplyDigest::computeAppliedStateChecksum();
    lastStorageGeneration = ConfigManager::getStorageBackend()->generation();
    DiagnosticRecord::recordConfigApplyResult(DiagnosticRecord::ConfigApplyStatus::Acked,
                                              identity.configId.c_str());
    LOG_PRINTLN(Utility::formatAck(identity.configId.c_str(), identity.sequence,
                                   lastAppliedChecksum.c_str(), lastStorageGeneration));
    bulkConfigAssembler.reset();
}

void serviceBulkConfigAssemblerTimeoutAt(uint32_t nowMs) {
    if (!bulkConfigAssembler.expired(nowMs)) {
        return;
    }
    const uint32_t staleSequence = bulkConfigAssembler.sequenceHint();
    bulkConfigAssembler.reset();
    emitBulkIngestError("timeout", staleSequence);
}
} // namespace

void handleSetAllBulkCommand(const String &command) {
    String chunk = command.substring(8);
    if (chunk.length() == 0) {
        return;
    }

    serviceBulkConfigAssemblerTimeout();

    String ingestError;
    if (!bulkConfigAssembler.ingestChunk(chunk, ingestError)) {
        emitBulkIngestError(ingestError, bulkConfigAssembler.sequenceHint());
        return;
    }

    if (!bulkConfigAssembler.complete()) {
        return;
    }

    auto &doc = bulkApplyDoc;
    if (!parseConfigJsonPayload(bulkConfigAssembler.payload(), bulkConfigAssembler.sequenceHint(),
                                doc)) {
        bulkConfigAssembler.reset();
        return;
    }

    BulkApplyIdentity identity;
    if (!resolveBulkApplyIdentity(doc, identity)) {
        return;
    }

    if (emitDuplicateBulkAckIfNeeded(identity)) {
        return;
    }

    JsonObject configObj = doc["config"].as<JsonObject>();
    if (!applyConfigJsonObject(configObj, identity.sequence)) {
        DiagnosticRecord::recordConfigApplyResult(DiagnosticRecord::ConfigApplyStatus::Error,
                                                  identity.configId.c_str());
        bulkConfigAssembler.reset();
        return;
    }

    commitBulkApplyAck(identity);
}

void handleAbortSetAllBulkCommand(const String &command) {
    (void)command;
    const bool aborted = bulkConfigAssembler.inProgress();
    const uint32_t sequence = bulkConfigAssembler.sequenceHint();
    bulkConfigAssembler.reset();
    LOG_PRINTF("{\"type\":\"response\",\"status\":\"ok\",\"command\":\"ABORT_SET_ALL\","
               "\"aborted\":%s,\"seq\":%lu}\n",
               aborted ? "true" : "false", static_cast<unsigned long>(sequence));
}

void serviceBulkConfigAssemblerTimeout() {
    serviceBulkConfigAssemblerTimeoutAt(millis());
}

#if defined(UNIT_TEST)
void testOnlyResetConfigBulkTransport() {
    bulkConfigAssembler.reset();
    lastAckSequence = 0;
    lastAckChecksum = "";
    lastAppliedChecksum = "";
    lastStorageGeneration = 0;
}

bool testOnlyConfigBulkTransportInProgress() { return bulkConfigAssembler.inProgress(); }

void testOnlySeedConfigBulkAck(uint32_t sequence, const String &configId,
                               const String &appliedChecksum, uint32_t storageGeneration) {
    lastAckSequence = sequence;
    lastAckChecksum = configId;
    lastAppliedChecksum = appliedChecksum;
    lastStorageGeneration = storageGeneration;
}

void testOnlyForceConfigBulkTimeout() {
    serviceBulkConfigAssemblerTimeoutAt(millis() + 5001U);
}
#endif

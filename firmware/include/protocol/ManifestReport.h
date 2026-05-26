#ifndef MANIFEST_REPORT_H
#define MANIFEST_REPORT_H

#include <ArduinoJson.h>

// ManifestReport is the firmware identity/capability emitter.
//
// Hosts use this payload to answer "what board is this?", "which schema does
// it expect?", and "which optional live-control lanes are safe to expose?"

void writeManifestFields(JsonObject object);

#endif // MANIFEST_REPORT_H

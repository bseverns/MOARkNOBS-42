#include "protocol/ConfigSchema.h"

#include "protocol/GeneratedConfigSchema.h"

FLASHMEM String buildConfigSchema() {
    String schema;
    schema.reserve(sizeof(ManifestContract::kConfigSchemaJson));
    schema += ManifestContract::kConfigSchemaJson;
    return schema;
}

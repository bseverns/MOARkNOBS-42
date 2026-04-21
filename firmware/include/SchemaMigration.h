// Schema migration helpers extracted from ConfigManager.
//
// This translation unit contains ConfigManager member-function
// implementations that handle EEPROM versioning, legacy slot layout
// upgrades, and profile block wipes during firmware schema transitions.
// No public header is needed because the methods are already declared
// in ConfigManager.h; this file is purely an implementation split.
//
// Included in every build_src_filter that pulls ConfigManager.cpp
// (see [core_modules] in platformio.ini).

#ifndef SCHEMA_MIGRATION_H
#define SCHEMA_MIGRATION_H

// This header exists only as a build-system sentinel so PlatformIO's
// dependency walker can see that SchemaMigration.cpp belongs to the
// firmware source tree.  All public API lives in ConfigManager.h.

#endif // SCHEMA_MIGRATION_H

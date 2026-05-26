#pragma once

// Helpers to stringify macro values
#define _STR(x) #x
#define STR(x) _STR(x)

// Default version identifiers; build system stamps real values via build flags.
#ifndef FW_VERSION
#define FW_VERSION 0.9.5
#endif

#ifndef GIT_SHA
#define GIT_SHA 0000000
#endif

#define FW_VERSION_STR STR(FW_VERSION)
#define GIT_SHA_STR STR(GIT_SHA)

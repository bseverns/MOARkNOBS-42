#pragma once

#include <Arduino.h>

// Return a static JSON blob with firmware and git info.
// Keys: "fw" for FW_VERSION and "git" for GIT_SHA.
const char* systemReportJSON();


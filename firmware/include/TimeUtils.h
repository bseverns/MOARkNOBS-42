#pragma once

// Simple wrapper around Arduino's millis() so tests can swap time sources.
unsigned long now();

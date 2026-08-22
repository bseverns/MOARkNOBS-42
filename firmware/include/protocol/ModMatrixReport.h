#ifndef MN42_PROTOCOL_MOD_MATRIX_REPORT_H
#define MN42_PROTOCOL_MOD_MATRIX_REPORT_H

#include <Arduino.h>

namespace ModMatrixReport {
// Materialize the current modulation graph. Returns false on JSON overflow.
bool build(String &payload);
} // namespace ModMatrixReport

#endif // MN42_PROTOCOL_MOD_MATRIX_REPORT_H

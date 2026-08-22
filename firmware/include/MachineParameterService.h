#ifndef MACHINE_PARAMETER_SERVICE_H
#define MACHINE_PARAMETER_SERVICE_H

#include "MidiInputTypes.h"

#include <cstdint>

namespace MachineParameterService {
bool apply(MachineParameterTarget target, uint8_t targetIndex, uint8_t value,
           MidiInputPort origin);
bool read(MachineParameterTarget target, uint8_t targetIndex, uint8_t &value);
} // namespace MachineParameterService

#endif // MACHINE_PARAMETER_SERVICE_H

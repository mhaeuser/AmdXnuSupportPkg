#include <Base.h>

#include <Library/PcdLib.h>

#include "AmdIntelEmu.h"

GLOBAL_REMOVE_IF_UNREFERENCED
CONST AMD_INTEL_EMU_MMIO_HANDLER_MAP mInternalMmioHandlerMap[] = {
  {
    FixedPcdGet32 (PcdCpuLocalApicBaseAddress),
    AmdIntelEmuInternalMmioLapic
  }
};

GLOBAL_REMOVE_IF_UNREFERENCED
CONST UINTN mInternalMmioNumHandlers = ARRAY_SIZE (mInternalMmioHandlerMap);

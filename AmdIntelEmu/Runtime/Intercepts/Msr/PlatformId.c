#include <Base.h>

#include <Register/ArchitecturalMsr.h>

#include <Library/DebugLib.h>

#include "../../AmdIntelEmuRuntime.h"

VOID
AmdIntelEmuInternalRdmsrPlatformId (
  IN OUT AMD_VMCB_SAVE_STATE_AREA_NON_ES  *SaveState,
  IN OUT AMD_INTEL_EMU_REGISTERS          *Registers,
  OUT    AMD_VMCB_CLEAN_FIELD             *CleanField
  )
{
  MSR_IA32_PLATFORM_ID_REGISTER PlatformIdMsr;

  ASSERT (SaveState != NULL);
  ASSERT (Registers != NULL);

  PlatformIdMsr.Uint64          = 0;
  PlatformIdMsr.Bits.PlatformId = 1;
  AmdIntelEmuInternalWriteMsrValue64 (
    &SaveState->RAX,
    Registers,
    PlatformIdMsr.Uint64
    );
}

VOID
AmdIntelEmuInternalWrmsrPlatformId (
  IN OUT AMD_VMCB_SAVE_STATE_AREA_NON_ES  *SaveState,
  IN OUT AMD_INTEL_EMU_REGISTERS          *Registers,
  OUT    AMD_VMCB_CLEAN_FIELD             *CleanField
  )
{
  //
  // PLATFORM_ID is read-only.
  //
}

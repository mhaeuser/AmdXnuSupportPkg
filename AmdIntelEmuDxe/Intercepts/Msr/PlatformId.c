#include <Base.h>

#include <Register/ArchitecturalMsr.h>

#include <Library/DebugLib.h>

#include "../../AmdIntelEmu.h"

VOID
AmdIntelEmuInternalRdmsrPlatformId (
  IN OUT UINT64             *Rax,
  IN OUT AMD_EMU_REGISTERS  *Registers
  )
{
  MSR_IA32_PLATFORM_ID_REGISTER PlatformIdMsr;

  ASSERT (Rax != NULL);
  ASSERT (Registers != NULL);

  PlatformIdMsr.Uint64          = 0;
  PlatformIdMsr.Bits.PlatformId = 1;
  AmdIntelEmuInternalWriteMsrValue64 (Rax, Registers, PlatformIdMsr.Uint64);
}

VOID
AmdIntelEmuInternalWrmsrPlatformId (
  IN OUT UINT64             *Rax,
  IN OUT AMD_EMU_REGISTERS  *Registers
  )
{
  ASSERT (Rax != NULL);
  ASSERT (Registers != NULL);
  //
  // PLATFORM_ID is read-only.
  //
}

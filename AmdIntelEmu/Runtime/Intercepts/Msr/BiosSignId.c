#include <Base.h>

#include <Register/ArchitecturalMsr.h>

#include <Library/BaseLib.h>
#include <Library/DebugLib.h>

#include "../../AmdIntelEmuRuntime.h"

STATIC UINT32 mMicrocodeUpdateSignature = 186;

VOID
AmdIntelEmuInternalRdmsrBiosSignId (
  IN OUT AMD_VMCB_SAVE_STATE_AREA_NON_ES  *SaveState,
  IN OUT AMD_INTEL_EMU_REGISTERS          *Registers,
  OUT    AMD_VMCB_CLEAN_FIELD             *CleanField
  )
{
  MSR_IA32_BIOS_SIGN_ID_REGISTER BiosSignIdMsr;

  ASSERT (SaveState != NULL);
  ASSERT (Registers != NULL);

  BiosSignIdMsr.Uint64                        = 0;
  BiosSignIdMsr.Bits.MicrocodeUpdateSignature = mMicrocodeUpdateSignature;
  AmdIntelEmuInternalWriteMsrValue64 (
    &SaveState->RAX,
    Registers,
    BiosSignIdMsr.Uint64
    );
}

VOID
AmdIntelEmuInternalWrmsrBiosSignId (
  IN OUT AMD_VMCB_SAVE_STATE_AREA_NON_ES  *SaveState,
  IN OUT AMD_INTEL_EMU_REGISTERS          *Registers,
  OUT    AMD_VMCB_CLEAN_FIELD             *CleanField
  )
{
  UINT64 Value;

  ASSERT (SaveState != NULL);
  ASSERT (Registers != NULL);

  Value = AmdIntelEmuInternalReadMsrValue64 (&SaveState->RAX, Registers);
  if (Value != 0) {
    mMicrocodeUpdateSignature = (UINT32)BitFieldRead64 (Value, 32, 63);
  }
}

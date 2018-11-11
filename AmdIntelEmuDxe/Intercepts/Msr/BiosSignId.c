#include <Base.h>

#include <Register/ArchitecturalMsr.h>

#include <Library/BaseLib.h>
#include <Library/DebugLib.h>

#include "../../AmdIntelEmu.h"

STATIC UINT32 mMicrocodeUpdateSignature = 186;

VOID
AmdIntelEmuInternalRdmsrBiosSignId (
  IN OUT UINT64             *Rax,
  IN OUT AMD_EMU_REGISTERS  *Registers
  )
{
  MSR_IA32_BIOS_SIGN_ID_REGISTER BiosSignIdMsr;

  ASSERT (Rax != NULL);
  ASSERT (Registers != NULL);

  BiosSignIdMsr.Uint64                        = 0;
  BiosSignIdMsr.Bits.MicrocodeUpdateSignature = mMicrocodeUpdateSignature;
  AmdIntelEmuInternalWriteMsrValue64 (Rax, Registers, BiosSignIdMsr.Uint64);
}

VOID
AmdIntelEmuInternalWrmsrBiosSignId (
  IN OUT UINT64             *Rax,
  IN OUT AMD_EMU_REGISTERS  *Registers
  )
{
  UINT64 Value;

  ASSERT (Rax != NULL);
  ASSERT (Registers != NULL);

  Value = AmdIntelEmuInternalReadMsrValue64 (Rax, Registers);
  if (Value != 0) {
    mMicrocodeUpdateSignature = BitFieldRead64 (Value, 32, 63);
  }
}

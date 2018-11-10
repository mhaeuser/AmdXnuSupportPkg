#include <Base.h>

#include <Register/ArchitecturalMsr.h>

#include <Library/BaseLib.h>

#include "../../AmdIntelEmu.h"

VOID
AmdEmuMsrUpdatePat (
  IN OUT UINT64             *Rax,
  IN OUT AMD_EMU_REGISTERS  *Registers
  );

VOID
AmdEmuInterceptWrmsr (
  IN OUT UINT64             *Rax,
  IN OUT AMD_EMU_REGISTERS  *Registers
  )
{
  switch (*Rax) {
    case MSR_IA32_PAT:
    {
      AmdEmuMsrUpdatePat (Rax, Registers);
      break;
    }

    default:
    {
      AsmWriteMsr64 (*Rax, AmdIntelEmuInternalReadMsrValue64 (Rax, Registers));
      break;
    }
  }
}

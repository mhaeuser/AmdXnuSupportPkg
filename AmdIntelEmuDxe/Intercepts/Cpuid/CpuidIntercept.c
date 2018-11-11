#include <Base.h>

#include <Library/BaseLib.h>
#include <Library/DebugLib.h>

#include "../../AmdIntelEmu.h"

VOID
AmdEmuCpuidLeaf2 (
  OUT UINT32  *Eax,
  OUT UINT32  *Ebx,
  OUT UINT32  *Ecx,
  OUT UINT32  *Edx
  );

VOID
AmdEmuInterceptCpuid (
  IN OUT UINT64             *Rax,
  IN OUT AMD_EMU_REGISTERS  *Registers
  )
{
  UINT32 CpuidIndex;

  UINT32 Eax;
  UINT32 Ebx;
  UINT32 Ecx;
  UINT32 Edx;

  ASSERT (Rax != NULL);
  ASSERT (Registers != NULL);

  CpuidIndex = BitFieldRead32 (*Rax, 0, 31);

  switch (CpuidIndex) {
    case 2:
    {
       AmdEmuCpuidLeaf2 (&Eax, &Ebx, &Ecx, &Edx);
       break;
    }

    default:
    {
      AsmCpuid (CpuidIndex, &Eax, &Ebx, &Ecx, &Edx);
      break;
    }
  }

  *Rax           = BitFieldWrite32 (*Rax,           0, 31, Eax);
  Registers->Rbx = BitFieldWrite32 (Registers->Rbx, 0, 31, Ebx);
  Registers->Rcx = BitFieldWrite32 (Registers->Rcx, 0, 31, Ecx);
  Registers->Rdx = BitFieldWrite32 (Registers->Rdx, 0, 31, Edx);
}

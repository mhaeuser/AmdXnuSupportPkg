#include <Base.h>

#include <Library/BaseLib.h>

#include "../../AmdIntelEmu.h"

VOID
AmdEmuCpuidLeaf2 (
  IN  CONST UINT32  *CpuidData,
  OUT UINT32        *Eax,
  OUT UINT32        *Ebx,
  OUT UINT32        *Ecx,
  OUT UINT32        *Edx
  );

BOOLEAN
AmdEmuInterceptCpuid (
  IN OUT UINT64             *Rax,
  IN OUT AMD_EMU_REGISTERS  *Registers
  )
{
  UINT32 Eax;
  UINT32 Ebx;
  UINT32 Ecx;
  UINT32 Edx;

  switch (*Rax) {
    case 2:
    {
       AmdEmuCpuidLeaf2 (NULL, &Eax, &Ebx, &Ecx, &Edx);
       break;
    }

    default:
    {
      return FALSE;
    }
  }

  *Rax           = BitFieldWrite32 (*Rax,           0, 31, Eax);
  Registers->Rbx = BitFieldWrite32 (Registers->Rbx, 0, 31, Ebx);
  Registers->Rcx = BitFieldWrite32 (Registers->Rcx, 0, 31, Ecx);
  Registers->Rdx = BitFieldWrite32 (Registers->Rdx, 0, 31, Edx);
  return TRUE;
}

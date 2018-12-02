#include <Base.h>

#include <Library/BaseLib.h>
#include <Library/DebugLib.h>

#include "../../AmdIntelEmuRuntime.h"

VOID
AmdIntelEmuInternalCpuidLeaf0 (
  OUT UINT32  *Eax,
  OUT UINT32  *Ebx,
  OUT UINT32  *Ecx,
  OUT UINT32  *Edx
  );

VOID
AmdIntelEmuInternalCpuidLeaf1 (
  OUT UINT32  *Eax,
  OUT UINT32  *Ebx,
  OUT UINT32  *Ecx,
  OUT UINT32  *Edx
  );

VOID
AmdIntelEmuInternalCpuidLeaf2 (
  OUT UINT32  *Eax,
  OUT UINT32  *Ebx,
  OUT UINT32  *Ecx,
  OUT UINT32  *Edx
  );

VOID
AmdIntelEmuInternalCpuidLeaf4 (
  IN  UINT32  SubIndex,
  OUT UINT32  *Eax,
  OUT UINT32  *Ebx,
  OUT UINT32  *Ecx,
  OUT UINT32  *Edx
  );

VOID
AmdEmuInterceptCpuidVmm (
  IN  UINT32  Index,
  OUT UINT32  *Eax,
  OUT UINT32  *Ebx,
  OUT UINT32  *Ecx,
  OUT UINT32  *Edx
  );

VOID
AmdEmuInterceptCpuid (
  IN OUT AMD_VMCB_SAVE_STATE_AREA_NON_ES  *SaveState,
  IN OUT AMD_INTEL_EMU_REGISTERS          *Registers
  )
{
  UINT32 CpuidIndex;

  UINT32 Eax;
  UINT32 Ebx;
  UINT32 Ecx;
  UINT32 Edx;

  ASSERT (SaveState != NULL);
  ASSERT (Registers != NULL);

  CpuidIndex = (UINT32)SaveState->RAX;

  Ecx = (UINT32)Registers->Rcx;

  switch (CpuidIndex) {
    case 0:
    {
      AmdIntelEmuInternalCpuidLeaf0 (&Eax, &Ebx, &Ecx, &Edx);
      break;
    }
    
    case 1:
    {
      AmdIntelEmuInternalCpuidLeaf1 (&Eax, &Ebx, &Ecx, &Edx);
      break;
    }
    
    case 2:
    {
      AmdIntelEmuInternalCpuidLeaf2 (&Eax, &Ebx, &Ecx, &Edx);
      break;
    }

    case 4:
    {
      AmdIntelEmuInternalCpuidLeaf4 (Ecx, &Eax, &Ebx, &Ecx, &Edx);
      break;
    }

    default:
    {
      if ((CpuidIndex < 0x40000000) || (CpuidIndex > 0x400000FF)) {
        AsmCpuidEx (CpuidIndex, Ecx, &Eax, &Ebx, &Ecx, &Edx);
      } else {
        AmdEmuInterceptCpuidVmm (CpuidIndex, &Eax, &Ebx, &Ecx, &Edx);
      }

      break;
    }
  }

  SaveState->RAX = BitFieldWrite64 (SaveState->RAX, 0, 31, Eax);
  Registers->Rbx = BitFieldWrite64 (Registers->Rbx, 0, 31, Ebx);
  Registers->Rcx = BitFieldWrite64 (Registers->Rcx, 0, 31, Ecx);
  Registers->Rdx = BitFieldWrite64 (Registers->Rdx, 0, 31, Edx);
}

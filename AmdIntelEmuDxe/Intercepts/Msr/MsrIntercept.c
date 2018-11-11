#include <Base.h>

#include <Register/ArchitecturalMsr.h>

#include <Library/BaseLib.h>
#include <Library/DebugLib.h>

#include "../../AmdIntelEmu.h"

VOID
AmdIntelEmuInternalWrmsrPat (
  IN OUT UINT64             *Rax,
  IN OUT AMD_EMU_REGISTERS  *Registers
  );

VOID
AmdIntelEmuInternalRdmsrMiscEnable (
  IN OUT UINT64             *Rax,
  IN OUT AMD_EMU_REGISTERS  *Registers
  );

VOID
AmdIntelEmuInternalWrmsrMiscEnable (
  IN OUT UINT64             *Rax,
  IN OUT AMD_EMU_REGISTERS  *Registers
  );

VOID
AmdEmuInterceptRdmsr (
  IN OUT UINT64             *Rax,
  IN OUT AMD_EMU_REGISTERS  *Registers
  )
{
  UINT32 MsrIndex;

  MsrIndex = BitFieldRead32 (Registers->Rcx, 0, 31);

  switch (MsrIndex) {
    case MSR_IA32_MISC_ENABLE:
    {
      AmdIntelEmuInternalRdmsrMiscEnable (Rax, Registers);
      break;
    }

    default:
    {
      AmdIntelEmuInternalWriteMsrValue64 (
        Rax,
        Registers,
        AsmReadMsr64 (MsrIndex)
        );
      break;
    }
  }
}

VOID
AmdEmuInterceptWrmsr (
  IN OUT UINT64             *Rax,
  IN OUT AMD_EMU_REGISTERS  *Registers
  )
{
  UINT32 MsrIndex;

  MsrIndex = BitFieldRead32 (Registers->Rcx, 0, 31);

  switch (MsrIndex) {
    case MSR_IA32_MISC_ENABLE:
    {
      AmdIntelEmuInternalWrmsrMiscEnable (Rax, Registers);
      break;
    }

    case MSR_IA32_PAT:
    {
      AmdIntelEmuInternalWrmsrPat (Rax, Registers);
      break;
    }

    default:
    {
      AsmWriteMsr64 (
        MsrIndex,
        AmdIntelEmuInternalReadMsrValue64 (Rax, Registers)
        );
      break;
    }
  }
}

UINT64
AmdIntelEmuInternalReadMsrValue64 (
  IN OUT UINT64             *Rax,
  IN OUT AMD_EMU_REGISTERS  *Registers
  )
{
  UINT64 Value;

  ASSERT (Rax != NULL);
  ASSERT (Registers != NULL);

  Value  = *Rax;
  Value |= (Registers->Rdx >> 31U);

  return Value;
}

VOID
AmdIntelEmuInternalWriteMsrValue64 (
  IN OUT UINT64             *Rax,
  IN OUT AMD_EMU_REGISTERS  *Registers,
  IN     UINT64             Value
  )
{
  ASSERT (Rax != NULL);
  ASSERT (Registers != NULL);

  *Rax           = (Value & 0xFFFFFFFFU);
  Registers->Rdx = (Value << 31U);
}

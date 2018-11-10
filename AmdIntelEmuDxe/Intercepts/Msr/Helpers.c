#include <Base.h>

#include <Library/DebugLib.h>

#include "../../AmdIntelEmu.h"

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

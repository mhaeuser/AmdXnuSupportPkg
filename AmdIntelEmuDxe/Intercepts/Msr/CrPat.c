#include <Base.h>

#include <Register/ArchitecturalMsr.h>

#include <Library/BaseLib.h>
#include <Library/DebugLib.h>

#include "../../AmdIntelEmu.h"

#define PATENTRY(Value, Type) (((Type) & 0x0FULL) << ((Value) * 8ULL))
#define PAT_UC       0x00ULL
#define PAT_WC       0x01ULL
#define PAT_WT       0x04ULL
#define PAT_WP       0x05ULL
#define PAT_WB       0x06ULL
#define PAT_UCMINUS  0x07ULL

VOID
AmdEmuMsrUpdatePat (
  IN OUT UINT64             *Rax,
  IN OUT AMD_EMU_REGISTERS  *Registers
  )
{
  UINT64 Value;

  ASSERT (Rax != NULL);
  ASSERT (Registers != NULL);
  //
  // Undocumented fix by Bronya: Force WT-by-default PAT1 and PAT5 entries into
  // WC mode.
  // TODO: Is forcing the other PAT entries into default as done in the kenrel
  //       necessary? Why does this work? Do all WTs need to be forced into WC?
  //
  Value  = AmdIntelEmuInternalReadMsrValue64 (Rax, Registers);
  Value &= ~(UINT64)(PATENTRY (1U, 0x0FU) | PATENTRY (5U, 0x0FU));
  Value |= (PATENTRY (1U, PAT_WC) | PATENTRY (5U, PAT_WC));
  AsmWriteMsr64 (MSR_IA32_PAT, Value);
}

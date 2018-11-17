#include <Base.h>

#include <Register/ArchitecturalMsr.h>

#include <Library/BaseLib.h>
#include <Library/DebugLib.h>

#include "../../AmdIntelEmu.h"

VOID
AmdIntelEmuInternalWrmsrPat (
  IN OUT AMD_VMCB_SAVE_STATE_AREA_NON_ES  *SaveState,
  IN OUT AMD_EMU_REGISTERS                *Registers
  )
{
  MSR_IA32_PAT_REGISTER PatMsr;

  ASSERT (SaveState != NULL);
  ASSERT (Registers != NULL);
  //
  // Undocumented fix by Bronya: Force WT-by-default PAT1 and PAT5 entries into
  // WC mode.
  // TODO: Is forcing the other PAT entries into default as done in the kenrel
  //       necessary? Why does this work? Do all WTs need to be forced into WC?
  //
  PatMsr.Uint64 = AmdIntelEmuInternalReadMsrValue64 (
                    &SaveState->RAX,
                    Registers
                    );
  PatMsr.Bits.PA1 = PAT_WC;
  PatMsr.Bits.PA5 = PAT_WC;
  // TODO: Write MSR for when Nested Paging is off.
  //AsmWriteMsr64 (MSR_IA32_PAT, PatMsr.Uint64);
  //
  // Inform the Guest of the new PAT for when Nested Paging is used.
  //
  SaveState->G_PAT = PatMsr.Uint64;
}

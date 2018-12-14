#include <Base.h>

#include <Register/LocalApic.h>

#include <Library/DebugLib.h>

#include "../../AmdIntelEmuRuntime.h"

VOID
AmdIntelEmuInternalRdmsrX2apicVersion (
  IN OUT AMD_VMCB_SAVE_STATE_AREA_NON_ES  *SaveState,
  IN OUT AMD_INTEL_EMU_REGISTERS          *Registers
  )
{
  ASSERT (SaveState != NULL);
  ASSERT (Registers != NULL);

  AmdIntelEmuInternalWriteMsrValue64 (
    &SaveState->RAX,
    Registers,
    BitFieldWrite32 (
      AmdIntelEmuReadLocalApicReg (XAPIC_VERSION_OFFSET),
      0,
      7,
      0x14
      )
    );
}

VOID
AmdIntelEmuInternalWrmsrX2apicVersion (
  IN OUT AMD_VMCB_SAVE_STATE_AREA_NON_ES  *SaveState,
  IN OUT AMD_INTEL_EMU_REGISTERS          *Registers
  )
{
  ASSERT (SaveState != NULL);
  ASSERT (Registers != NULL);
  //
  // X2APIC_VERSION is read-only.
  //
}

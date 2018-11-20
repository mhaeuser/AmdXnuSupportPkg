#include <Base.h>

#include <Library/DebugLib.h>

#include "../AmdIntelEmu.h"

VOID
AmdEmuInterceptCpuid (
  IN OUT AMD_VMCB_SAVE_STATE_AREA_NON_ES  *SaveState,
  IN OUT AMD_EMU_REGISTERS                *Registers
  );

VOID
AmdIntelEmuInternalInterceptRdmsr (
  IN OUT AMD_VMCB_SAVE_STATE_AREA_NON_ES  *SaveState,
  IN OUT AMD_EMU_REGISTERS                *Registers
  );

VOID
AmdIntelEmuInternalInterceptWrmsr (
  IN OUT AMD_VMCB_SAVE_STATE_AREA_NON_ES  *SaveState,
  IN OUT AMD_EMU_REGISTERS                *Registers
  );

VOID
EFIAPI
AmdInterceptionHandler (
  IN OUT AMD_VMCB_CONTROL_AREA  *Vmcb,
  IN OUT AMD_EMU_REGISTERS      *Registers
  )
{
  AMD_VMCB_SAVE_STATE_AREA_NON_ES *SaveState;
  //
  // For forward compatibility, if the hypervisor has not modified the VMCB,
  // the hypervisor may write FFFF_FFFFh to the VMCB Clean Field to indicate
  // that it has not changed any VMCB contents other than the fields described
  // below as explicitly uncached.
  //
  Vmcb->VmcbCleanBits.Uint32 = MAX_UINT32;

  SaveState = (AMD_VMCB_SAVE_STATE_AREA_NON_ES *)(UINTN)Vmcb->VmcbSaveState;

  switch (Vmcb->EXITCODE) {
    case VMEXIT_CPUID:
    {
      AmdEmuInterceptCpuid (SaveState, Registers);
      break;
    }

    case VMEXIT_MSR:
    {
      if (Vmcb->EXITINFO1 == 0) {
        AmdIntelEmuInternalInterceptRdmsr (SaveState, Registers);
      } else {
        ASSERT (Vmcb->EXITINFO1 == 1);
        AmdIntelEmuInternalInterceptWrmsr (SaveState, Registers);
      }

      break;
    }

    case VMEXIT_VMRUN:
    {
      // TODO: Inject #UD for vmrun execution within the guest.
      break;
    }

    default:
    {
      ASSERT (FALSE);
      break;
    }
  }
}

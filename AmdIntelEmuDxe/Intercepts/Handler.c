#include <Base.h>

#include <Library/DebugLib.h>

#include "../AmdIntelEmu.h"

BOOLEAN
AmdEmuInterceptUd (
  IN UINT64  *Rip
  );

VOID
AmdEmuInterceptCpuid (
  IN OUT UINT64             *Rax,
  IN OUT AMD_EMU_REGISTERS  *Registers
  );

VOID
AmdEmuInterceptRdmsr (
  IN OUT UINT64             *Rax,
  IN OUT AMD_EMU_REGISTERS  *Registers
  );

VOID
AmdEmuInterceptWrmsr (
  IN OUT UINT64             *Rax,
  IN OUT AMD_EMU_REGISTERS  *Registers
  );

VOID
EFIAPI
AmdInterceptionHandler (
  IN OUT AMD_VMCB_CONTROL_AREA  *Vmcb,
  IN OUT AMD_EMU_REGISTERS      *Registers
  )
{
  UINT32                          VmcbCleanBits;
  BOOLEAN                         RegistersModded;
  BOOLEAN                         Result;
  AMD_VMCB_SAVE_STATE_AREA_NON_ES *SaveState;
  //
  // For forward compatibility, if the hypervisor has not modified the VMCB,
  // the hypervisor may write FFFF_FFFFh to the VMCB Clean Field to indicate
  // that it has not changed any VMCB contents other than the fields described
  // below as explicitly uncached.
  //
  VmcbCleanBits   = MAX_UINT32;
  RegistersModded = FALSE;

  SaveState = (AMD_VMCB_SAVE_STATE_AREA_NON_ES *)(UINTN)Vmcb->VmcbSaveState;

  switch (Vmcb->EXITCODE) {
    case VMEXIT_EXCP_UD:
    {
      ASSERT (SaveState != NULL);
      ASSERT (SaveState->CS.Base == 0);
      ASSERT (SaveState->RIP != 0);

      Result = AmdEmuInterceptUd (&SaveState->RIP);
      if (!Result) {
        //
        // Clean the #UD bit so the guest gets to handle the exception.
        // It is not expected that normal guest operation will be resumed.
        //
        Vmcb->InterceptExceptionVectors &= ~(UINT32)AMD_VMCB_EXCEPTION_UD;
        //
        // Clear the Intercept bit.
        //
        VmcbCleanBits &= ~(UINT32)BIT0;
      }

      break;
    }

    case VMEXIT_CPUID:
    {
      AmdEmuInterceptCpuid (&SaveState->RAX, Registers);
      RegistersModded = TRUE;
      break;
    }

    case VMEXIT_MSR:
    {
      if (Vmcb->EXITINFO1 == 0) {
        AmdEmuInterceptRdmsr (&SaveState->RAX, Registers);
        RegistersModded = TRUE;
      } else {
        ASSERT (Vmcb->EXITINFO1 == 1);
        AmdEmuInterceptWrmsr (&SaveState->RAX, Registers);
      }

      break;
    }

    default:
    {
      ASSERT (FALSE);
      break;
    }
  }

  if (RegistersModded) {
    //
    // Clear all fields not known to not involve the registers referenced to by
    // Registers as of 24594 - Rev 3.30.
    //
    VmcbCleanBits &= ~(UINT32)0xFFFFF000U;
  }

  Vmcb->VmcbCleanBits = VmcbCleanBits;
}

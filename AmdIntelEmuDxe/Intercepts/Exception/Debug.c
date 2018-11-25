#include <Uefi.h>

#include <Register/ArchitecturalMsr.h>

#include <Protocol/DebugSupport.h>

#include <Library/BaseLib.h>
#include <Library/DebugLib.h>

#include "../../AmdIntelEmu.h"

VOID
AmdIntelEmuInternalSingleStepRip (
  IN OUT AMD_INTEL_EMU_THREAD_CONTEXT      *ThreadContext,
  IN     AMD_INTEL_EMU_SINGLE_STEP_RESUME  ResumeHandler,
  IN     UINTN                             ResumeContext
  )
{
  AMD_VMCB_CONTROL_AREA           *Vmcb;
  AMD_VMCB_SAVE_STATE_AREA_NON_ES *SaveState;
  IA32_EFLAGS32                   Eflags;
  MSR_IA32_DEBUGCTL_REGISTER      DebugCtlMsr;

  ASSERT (ThreadContext != NULL);
  ASSERT (ResumeHandler != NULL);

  Vmcb = ThreadContext->Vmcb;
  ASSERT (Vmcb != NULL);
  //
  // Enable the #DB exception intercept.
  //
  Vmcb->InterceptExceptionVectors |= (1UL << EXCEPT_IA32_DEBUG);
  Vmcb->VmcbCleanBits.Bits.I       = 0;

  SaveState = (AMD_VMCB_SAVE_STATE_AREA_NON_ES *)(UINTN)Vmcb->VmcbSaveState;
  ASSERT (SaveState != NULL);
  //
  // Enable single-stepping.  Set the Resume Flag in case the memory access
  // has an instruction breakpoint set.
  // RFLAGS is not cached.
  //
  Eflags.UintN        = SaveState->RFLAGS;
  ThreadContext->SsTf = (Eflags.Bits.TF != 0);
  Eflags.Bits.TF      = 1;
  Eflags.Bits.RF      = 1;
  SaveState->RFLAGS   = Eflags.UintN;
  //
  // Disable "Branch Single Step" if enabled to intercept the immediate next
  // instruction.
  //
  DebugCtlMsr.Uint64 = AsmReadMsr64 (MSR_IA32_DEBUGCTL);
  ThreadContext->Btf = (DebugCtlMsr.Bits.BTF != 0);
  if (ThreadContext->Btf) {
    DebugCtlMsr.Bits.BTF = 0;
    AsmWriteMsr64 (MSR_IA32_DEBUGCTL, DebugCtlMsr.Uint64);
  }

  ThreadContext->SingleStepResume  = ResumeHandler;
  ThreadContext->SingleStepContext = ResumeContext;
}

VOID
AmdIntelEmuInternalExceptionDebug (
  IN OUT AMD_VMCB_CONTROL_AREA  *Vmcb
  )
{
  AMD_INTEL_EMU_THREAD_CONTEXT    *ThreadContext;
  AMD_VMCB_SAVE_STATE_AREA_NON_ES *SaveState;
  IA32_EFLAGS32                   Eflags;
  MSR_IA32_DEBUGCTL_REGISTER      DebugCtlMsr;

  ASSERT (Vmcb != NULL);

  ThreadContext = AmdIntelEmuInternalGetThreadContext (Vmcb);
  ASSERT (ThreadContext != NULL);
  ASSERT (ThreadContext->SingleStepResume != NULL);

  ThreadContext->SingleStepResume (
                   Vmcb,
                   ThreadContext->SingleStepContext
                   );
  //
  // Disable the #DB exception intercept.
  //
  Vmcb->InterceptExceptionVectors &= ~(UINT32)(1UL << EXCEPT_IA32_DEBUG);
  Vmcb->VmcbCleanBits.Bits.I       = 0;

  SaveState = (AMD_VMCB_SAVE_STATE_AREA_NON_ES *)(UINTN)Vmcb->VmcbSaveState;
  ASSERT (SaveState != NULL);

  ASSERT ((SaveState->DR6 & BIT14) != 0);
  
  if (!ThreadContext->SsTf) {
    //
    // Disable single-stepping if it hasn't been disabled before.
    // RFLAGS is not cached.
    //
    Eflags.UintN      = SaveState->RFLAGS;
    Eflags.Bits.TF    = 0;
    SaveState->RFLAGS = Eflags.UintN;

    SaveState->DR6 &= ~(UINT64)BIT14;
    //
    // Re-enable BTF if it has been disabled before.
    //
    if (ThreadContext->Btf) {
      DebugCtlMsr.Uint64 = AsmReadMsr64 (MSR_IA32_DEBUGCTL);
      ThreadContext->Btf = 1;
      AsmWriteMsr64 (MSR_IA32_DEBUGCTL, DebugCtlMsr.Uint64);
    }

    // TODO: Handle data breakpoints.
  } else {
    //
    // Pass the TF #DB through.  Event injection is not cached.
    //
    Vmcb->EVENTINJ.Bits.VECTOR = EXCEPT_IA32_DEBUG;
    Vmcb->EVENTINJ.Bits.TYPE   = AmdVmcbException;
    Vmcb->EVENTINJ.Bits.V      = 1;
    Vmcb->EVENTINJ.Bits.EV     = 0;
  }
}

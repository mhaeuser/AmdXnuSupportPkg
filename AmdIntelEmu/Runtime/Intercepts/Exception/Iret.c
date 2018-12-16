#include <Base.h>

#include <Library/BaseLib.h>
#include <Library/DebugLib.h>

#include "../../AmdIntelEmuRuntime.h"

STATIC
VOID
InternalIretTfIntercept (
  IN OUT AMD_VMCB_CONTROL_AREA  *Vmcb,
  IN     UINTN                  ResumeContext
  )
{
  AMD_INTEL_EMU_THREAD_CONTEXT    *ThreadContext;
  AMD_VMCB_SAVE_STATE_AREA_NON_ES *SaveState;
  IA32_EFLAGS32                   Eflags;

  ASSERT (Vmcb != NULL);

  ThreadContext = AmdIntelEmuInternalGetThreadContext (Vmcb);
  ASSERT (ThreadContext != NULL);

  Vmcb->EVENTINJ.Uint64 = ThreadContext->QueueEvent.Uint64;
  //
  // Disable single-stepping if it had not been enabled prior to the intercept.
  // RFLAGS is not cached.
  //
  if (ResumeContext == 0) {
    SaveState = (AMD_VMCB_SAVE_STATE_AREA_NON_ES *)(UINTN)Vmcb->VmcbSaveState;
    ASSERT (SaveState != NULL);

    Eflags.UintN      = SaveState->RFLAGS;
    Eflags.Bits.TF    = 0;
    SaveState->RFLAGS = Eflags.UintN;
  }
}

VOID
AmdIntelEmuInternalExceptionIret (
  IN OUT AMD_VMCB_CONTROL_AREA  *Vmcb
  )
{
  AMD_INTEL_EMU_THREAD_CONTEXT *ThreadContext;

  ASSERT (Vmcb != NULL);

  ThreadContext = AmdIntelEmuInternalGetThreadContext (Vmcb);
  ASSERT (ThreadContext != NULL);
  //
  // Disable the iret intercept.
  //
  Vmcb->InterceptIret        = 0;
  Vmcb->VmcbCleanBits.Bits.I = 0;
  //
  // Single-step over the iret instruction so we can queue the pending event
  // after the current one finished processing.
  //
  AmdIntelEmuInternalSingleStepRip (
    ThreadContext,
    InternalIretTfIntercept,
    (ThreadContext->IretTf ? 1 : 0)
    );
}

VOID
AmdIntelEmuInternalQueueEvent (
  IN OUT AMD_VMCB_CONTROL_AREA  *Vmcb,
  IN     CONST AMD_VMCB_EVENT   *Event
  )
{
  AMD_INTEL_EMU_THREAD_CONTEXT    *ThreadContext;
  AMD_VMCB_SAVE_STATE_AREA_NON_ES *SaveState;
  IA32_EFLAGS32                   Eflags;

  ASSERT (Vmcb != NULL);
  ASSERT (Event != NULL);

  ThreadContext = AmdIntelEmuInternalGetThreadContext (Vmcb);
  ASSERT (ThreadContext != NULL);
  //
  // Enable the iret intercept to be notified about interrupt handler exit.
  //
  Vmcb->InterceptIret        = 1;
  Vmcb->VmcbCleanBits.Bits.I = 0;

  SaveState = (AMD_VMCB_SAVE_STATE_AREA_NON_ES *)(UINTN)Vmcb->VmcbSaveState;
  ASSERT (SaveState != NULL);
  //
  // Disable single-stepping.  This is done as only invoking the #DB handler
  // would otherwise trigger the CPU to unset the TF flag on its own.
  // RFLAGS is not cached.
  //
  Eflags.UintN          = SaveState->RFLAGS;
  ThreadContext->IretTf = (Eflags.Bits.TF != 0);
  Eflags.Bits.TF        = 0;
  SaveState->RFLAGS     = Eflags.UintN;

  ThreadContext->QueueEvent.Uint64 = Event->Uint64;
}

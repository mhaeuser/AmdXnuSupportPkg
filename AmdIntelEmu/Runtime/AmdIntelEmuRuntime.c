#include <Base.h>

#include <Library/DebugLib.h>
#include <Library/PcdLib.h>

#include "AmdIntelEmuRuntime.h"

STATIC UINTN                        mInternalNumThreadContexts = 0;
STATIC AMD_INTEL_EMU_THREAD_CONTEXT *mInternalThreadContexts   = NULL;

GLOBAL_REMOVE_IF_UNREFERENCED BOOLEAN mAmdIntelEmuInternalNrip = FALSE;
GLOBAL_REMOVE_IF_UNREFERENCED BOOLEAN mAmdIntelEmuInternalNp   = FALSE;

VOID
EFIAPI
AmdIntelEmuInternalVmrun (
  IN OUT AMD_VMCB_CONTROL_AREA  *Vmcb,
  IN     VOID                   *HostStack
  );

AMD_INTEL_EMU_THREAD_CONTEXT *
AmdIntelEmuInternalGetThreadContext (
  IN CONST AMD_VMCB_CONTROL_AREA  *Vmcb
  )
{
  UINTN                        Index;
  AMD_INTEL_EMU_THREAD_CONTEXT *ThreadContext;

  ASSERT (Vmcb != NULL);
  ASSERT (mInternalNumThreadContexts > 0);
  ASSERT (mInternalThreadContexts != NULL);

  for (
    ThreadContext = mInternalThreadContexts, Index = 0;
    ThreadContext->Vmcb != Vmcb;
    ThreadContext = GET_NEXT_THREAD_CONTEXT (ThreadContext), ++Index
    ) {
    ASSERT (Index < mInternalNumThreadContexts);
  }

  return ThreadContext;
}

VOID
EFIAPI
_ModuleEntryPoint (
  IN  CONST AMD_INTEL_EMU_RUNTIME_CONTEXT     *Context,
  OUT AMD_INTEL_EMU_ENABLE_VM                 *EnableVm,
  OUT UINTN                                   *NumMsrIntercepts,
  OUT CONST AMD_INTEL_EMU_MSR_INTERCEPT_INFO  **MsrIntercepts
  )
{
  UINTN                        Index;
  AMD_INTEL_EMU_THREAD_CONTEXT *ThreadContext;

  ASSERT (Context != NULL);
  ASSERT (EnableVm != NULL);
  ASSERT (NumMsrIntercepts != NULL);
  ASSERT (MsrIntercepts != NULL);

  mInternalNumThreadContexts = Context->NumThreads;
  mInternalThreadContexts    = Context->ThreadContexts;
  mAmdIntelEmuInternalNrip   = Context->NripSupport;
  mAmdIntelEmuInternalNp     = Context->NpEnabled;
  AmdIntelEmuInternalMmioLapicSetPage (Context->LapicPage);

  *EnableVm         = AmdIntelEmuInternalVmrun;
  *NumMsrIntercepts = mAmdIntelEmuInternalNumMsrIntercepts;
  *MsrIntercepts    = mAmdIntelEmuInternalMsrIntercepts;
  //
  // Initialize the MMIO intercept handlers.
  //
  ASSERT (mAmdIntelEmuInternalLapicAddress != NULL);
  *mAmdIntelEmuInternalLapicAddress = AmdIntelEmuGetLocalApicBaseAddress ();

  for (
    Index = 0, ThreadContext = mInternalThreadContexts;
    Index < mInternalNumThreadContexts;
    ++Index, ThreadContext = GET_NEXT_THREAD_CONTEXT (ThreadContext)
    ) {
    AmdIntelEmuInternalInitializeMmioInfo (ThreadContext->MmioInfo);
  }
}

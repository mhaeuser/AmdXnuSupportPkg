#include <Base.h>

#include <Library/DebugLib.h>

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

  for (
    ThreadContext = mInternalThreadContexts, Index = 0;
    ThreadContext->Vmcb != Vmcb;
    ++ThreadContext, ++Index
    ) {
    ASSERT (Index < mInternalNumThreadContexts);
  }

  return ThreadContext;
}

STATIC
VOID
EFIAPI
InternalEnableVm (
  IN OUT AMD_INTEL_EMU_THREAD_CONTEXT  *ThreadContext,
  IN     VOID                          *HostStack
  )
{
  UINTN                   Index;
  AMD_INTEL_EMU_MMIO_INFO *MmioInfo;

  ASSERT (ThreadContext != NULL);
  ASSERT (HostStack != NULL);

  ASSERT (ThreadContext->Vmcb != NULL);
  //
  // Initialize the MMIO intercept handlers.
  //
  for (Index = 0; Index < ThreadContext->NumMmioInfo; ++Index) {
    MmioInfo = &ThreadContext->MmioInfo[Index];
    MmioInfo->GetPage = AmdIntelEmuInternalGetMmioHandler (MmioInfo->Address);
  }

  AmdIntelEmuInternalVmrun (ThreadContext->Vmcb, HostStack);
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
  ASSERT (Context != NULL);
  ASSERT (EnableVm != NULL);

  mInternalNumThreadContexts = Context->NumThreads;
  mInternalThreadContexts    = Context->ThreadContexts;
  mAmdIntelEmuInternalNrip   = Context->NripSupport;
  mAmdIntelEmuInternalNp     = Context->NpEnabled;
  AmdIntelEmuInternalMmioLapicSetPage (Context->LapicPage);

  *EnableVm         = InternalEnableVm;
  *NumMsrIntercepts = mAmdIntelEmuInternalNumMsrIntercepts;
  *MsrIntercepts    = mAmdIntelEmuInternalMsrIntercepts;
}

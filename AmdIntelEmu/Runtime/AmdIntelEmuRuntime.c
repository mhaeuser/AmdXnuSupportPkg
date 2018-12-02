#include <Base.h>

#include <Library/DebugLib.h>

#include "AmdIntelEmuRuntime.h"

STATIC UINTN                        mInternalNumThreadContexts = 0;
STATIC AMD_INTEL_EMU_THREAD_CONTEXT *mInternalThreadContexts = NULL;

GLOBAL_REMOVE_IF_UNREFERENCED BOOLEAN mAmdIntelEmuInternalNrip = FALSE;
GLOBAL_REMOVE_IF_UNREFERENCED BOOLEAN mAmdIntelEmuInternalNp = FALSE;

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

VOID
EFIAPI
_ModuleEntryPoint (
  IN  CONST AMD_INTEL_EMU_RUNTIME_CONTEXT     *Context,
  OUT AMD_INTEL_EMU_ENABLE_VM                 *EnableVm,
  OUT UINTN                                   *NumMmioInfo,
  OUT AMD_INTEL_EMU_MMIO_INFO                 **MmioInfo,
  OUT UINTN                                   *NumMsrIntercepts,
  OUT CONST AMD_INTEL_EMU_MSR_INTERCEPT_INFO  **MsrIntercepts
  )
{
  ASSERT (Context != NULL);
  ASSERT (EnableVm != NULL);
  ASSERT (NumMmioInfo != NULL);
  ASSERT (MmioInfo != NULL);

  mInternalNumThreadContexts = Context->NumThreadContexts;
  mInternalThreadContexts    = Context->ThreadContexts;
  mAmdIntelEmuInternalNrip   = Context->NripSupport;
  mAmdIntelEmuInternalNp     = Context->NpEnabled;
  AmdIntelEmuInternalMmioLapicSetPage (Context->LapicPage);

  *EnableVm         = AmdIntelEmuInternalEnableVm;
  *NumMmioInfo      = mAmdIntelEmuInternalNumMmioInfo;
  *MmioInfo         = mAmdIntelEmuInternalMmioInfo;
  *NumMsrIntercepts = mAmdIntelEmuInternalNumMsrIntercepts;
  *MsrIntercepts    = mAmdIntelEmuInternalMsrIntercepts;
}

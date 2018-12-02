#include <Uefi.h>

#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/PcdLib.h>

#include "../../AmdIntelEmuRuntime.h"

UINT64
AmdIntelEmuInternalMmioLapic (
  IN UINT64  Address
  );

GLOBAL_REMOVE_IF_UNREFERENCED
AMD_INTEL_EMU_MMIO_INFO mAmdIntelEmuInternalMmioInfo[] = {
  {
    FixedPcdGet32 (PcdCpuLocalApicBaseAddress),
    AmdIntelEmuInternalMmioLapic,
    NULL
  }
};

GLOBAL_REMOVE_IF_UNREFERENCED
CONST UINTN
mAmdIntelEmuInternalNumMmioInfo = ARRAY_SIZE (mAmdIntelEmuInternalMmioInfo);

STATIC
VOID
InternalExceptionNpfPostStep (
  IN OUT AMD_VMCB_CONTROL_AREA  *Vmcb,
  IN     UINTN                  ResumeContext
  )
{
  AMD_INTEL_EMU_MMIO_INFO *MmioInfo;
  PAGE_TABLE_4K_ENTRY     *Pte;
  UINT64                  Address;

  ASSERT (Vmcb != NULL);
  ASSERT (ResumeContext != 0);

  MmioInfo = (AMD_INTEL_EMU_MMIO_INFO *)ResumeContext;
  Pte      = MmioInfo->Pte;
  ASSERT (Pte != NULL);
  //
  // Unmap the MMIO intercept page.
  //
  Address = MmioInfo->Address;
  Address = BitFieldRead64 (Address, 12, 51);

  Pte->Bits.Present              = 0;
  Pte->Bits.PageTableBaseAddress = Address;
  Vmcb->VmcbCleanBits.Bits.NP    = 0;
}

STATIC
AMD_INTEL_EMU_MMIO_INFO *
InternalGetMmioInfo (
  IN UINT64  Address
  )
{
  UINTN                   Index;
  AMD_INTEL_EMU_MMIO_INFO *MmioInfo;

  for (Index = 0; Index < mAmdIntelEmuInternalNumMmioInfo; ++Index) {
    MmioInfo = &mAmdIntelEmuInternalMmioInfo[Index];
    if ((Address >= MmioInfo->Address)
     && (Address < (MmioInfo->Address + SIZE_4KB))) {
      return MmioInfo;
    }
  }

  return NULL;
}

VOID
AmdIntelEmuInternalExceptionNpf (
  IN OUT AMD_VMCB_CONTROL_AREA  *Vmcb
  )
{
  AMD_INTEL_EMU_THREAD_CONTEXT *ThreadContext;
  AMD_VMCB_EXITINFO1_NPF       ExitInfo1;
  UINT64                       Address;
  AMD_INTEL_EMU_MMIO_INFO      *MmioInfo;
  PAGE_TABLE_4K_ENTRY          *Pte;

  ASSERT (Vmcb != NULL);

  ThreadContext = AmdIntelEmuInternalGetThreadContext (Vmcb);
  ASSERT (ThreadContext != NULL);

  ExitInfo1.Uint64 = Vmcb->EXITINFO1;
  //
  // Sanity-check the NPF error information.
  //
  DEBUG_CODE (
    if ((ExitInfo1.Bits.FaultFinalAddress == 0)
     || (ExitInfo1.Bits.FaultGuestTlb     != 0)
     || (ExitInfo1.Bits.P   != 0)
     || (ExitInfo1.Bits.RW  == 0)
     || (ExitInfo1.Bits.RSV != 0)
     || (ExitInfo1.Bits.US  == 0)) {
      ASSERT (FALSE);
      return;
    }
    );

  Address  = Vmcb->EXITINFO2;
  MmioInfo = InternalGetMmioInfo (Address);
  if (MmioInfo != NULL) {
    Pte = MmioInfo->Pte;
    ASSERT (Pte != NULL);
    //
    // Map the MMIO intercept page.
    //
    Address = MmioInfo->GetPage (Vmcb->EXITINFO2);
    Address = BitFieldRead64 (Address, 12, 51);

    Pte->Bits.Present              = 1;
    Pte->Bits.PageTableBaseAddress = Address;
    Vmcb->VmcbCleanBits.Bits.NP    = 0;

    AmdIntelEmuInternalSingleStepRip (
      ThreadContext,
      InternalExceptionNpfPostStep,
      (UINTN)MmioInfo
      );
  } else {
    ASSERT (FALSE);
  }
}

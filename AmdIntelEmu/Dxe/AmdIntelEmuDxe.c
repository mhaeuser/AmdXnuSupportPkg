#include <PiDxe.h>

#include <Register/Amd/Cpuid.h>
#include <Register/Msr.h>

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/CpuExceptionHandlerLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/MpInitLib.h>
#include <Library/PcdLib.h>
#include <Library/UefiBootServicesTableLib.h>

#include "AmdIntelEmuDxe.h"


#define NUM_STACK_PAGES  1U

#define GET_PAGE(Address, Index)  \
  ((VOID *)((UINTN)(Address) + ((Index) * SIZE_4KB)))

typedef struct {
  AMD_INTEL_EMU_THREAD_CONTEXT *ThreadContext;
  AMD_VMCB_CONTROL_AREA        *HostVmcb;
  VOID                         *HostStack;
  AMD_INTEL_EMU_ENABLE_VM      EnableVm;
  UINT64                       TscStamp;
} AMD_INTEL_EMU_THREAD_PRIVATE;

typedef struct {
  UINTN                        Address;
  UINTN                        Size;
  AMD_INTEL_EMU_THREAD_CONTEXT *CurrentThreadContext;
} AMD_INTEL_EMU_PT_INIT_CONTEXT;

STATIC CONST UINT64 mInternalMmioAddresses[] = {
  FixedPcdGet32 (PcdCpuLocalApicBaseAddress)
};

STATIC BOOLEAN mInternalNripSupport = FALSE;
STATIC BOOLEAN mInternalNpSupport   = FALSE;

STATIC
BOOLEAN
InternalIsSvmAvailable (
	OUT BOOLEAN  *NripSupport,
	OUT BOOLEAN  *NpSupport
  )
{
  CPUID_AMD_EXTENDED_CPU_SIG_ECX           ExtendedCpuSigEcx;
  CPUID_AMD_SVM_FEATURE_IDENTIFICATION_EDX SvmFeatureIdEdx;
  MSR_VM_CR_REGISTER                       VmCrRegister;

  ASSERT (NripSupport != NULL);
  ASSERT (NpSupport != NULL);
  
  AsmCpuid (
    CPUID_EXTENDED_CPU_SIG,
    NULL,
    NULL,
    &ExtendedCpuSigEcx.Uint32,
    NULL
    );
  if (ExtendedCpuSigEcx.Bits.SVM == 0) {
    DEBUG ((DEBUG_ERROR, "AMD-V is not supported on this platform.\n"));
    return FALSE;
  }

  AsmCpuid (
    CPUID_AMD_SVM_FEATURE_IDENTIFICATION,
    NULL,
    NULL,
    NULL,
    &SvmFeatureIdEdx.Uint32
    );
  //
  // Disable SVMDIS if the register is unlocked.
  //
  VmCrRegister.Uint64 = AsmReadMsr64 (MSR_VM_CR);
  if (VmCrRegister.Bits.SVMDIS != 0) {
    //
    // SVMDIS is read-only when locking is unsupported.
    //
    if ((SvmFeatureIdEdx.Bits.SVML != 0) && (VmCrRegister.Bits.LOCK == 0)) {
      DEBUG ((DEBUG_WARN, "AMD-V is disabled but unlocked, enabling... "));

      VmCrRegister.Bits.SVMDIS = 0;
      AsmWriteMsr64 (MSR_VM_CR, VmCrRegister.Uint64);
      DEBUG_CODE (
        VmCrRegister.Uint64 = AsmReadMsr64 (MSR_VM_CR);
        ASSERT (VmCrRegister.Bits.SVMDIS == 0);
        );

      DEBUG ((DEBUG_WARN, "Done.\n"));
    } else {
      DEBUG ((DEBUG_ERROR, "AMD-V is disabled and locked.\n"));
      return FALSE;
    }
  }
  //
  // Cache the used AMD SVM capabilities.
  //
  *NripSupport = (SvmFeatureIdEdx.Bits.NRIPS != 0);
  *NpSupport   = (SvmFeatureIdEdx.Bits.NP != 0);

  DEBUG ((
    DEBUG_INFO,
    "AMD-V is enabled. NRIP: %d, NP: %d\n",
    *NripSupport,
    *NpSupport
    ));

  return TRUE;
}

STATIC
UINT32
InternalSegmentGetBase (
  IN CONST IA32_SEGMENT_DESCRIPTOR  *Segment
  )
{
  UINT32 Base;

  ASSERT (Segment != NULL);

  Base  = Segment->Bits.BaseLow;
  Base |= (Segment->Bits.BaseMid  << 16U);
  Base |= (Segment->Bits.BaseHigh << 24U);

  return Base;
}

STATIC
UINT32
InternalSegmentGetLimit (
  IN CONST IA32_SEGMENT_DESCRIPTOR  *Segment
  )
{
  ASSERT (Segment != NULL);
  return (Segment->Bits.LimitLow | (Segment->Bits.LimitHigh << 16U));
}

STATIC
UINT16
InternalSegmentGetAttributes (
  IN CONST IA32_SEGMENT_DESCRIPTOR  *Segment
  )
{
  IA32_SEGMENT_ATTRIBUTES Attributes;

  ASSERT (Segment != NULL);

  Attributes.Uint16    = 0;
  Attributes.Bits.Type = Segment->Bits.Type;
  Attributes.Bits.S    = Segment->Bits.S;
  Attributes.Bits.DPL  = Segment->Bits.DPL;
  Attributes.Bits.P    = Segment->Bits.P;
  Attributes.Bits.AVL  = Segment->Bits.AVL;
  Attributes.Bits.L    = Segment->Bits.L;
  Attributes.Bits.DB   = Segment->Bits.DB;
  Attributes.Bits.G    = Segment->Bits.G;

  return Attributes.Uint16;
}

STATIC
UINT16
InternalSelectorGetOffset (
  IN UINT16  Selector
  )
{
  //
  // Index starts at bit 3 and needs to be multiplied by 8 to be converted to
  // an offset.  The same is achieved by stripping [2:0].
  //
  return (Selector & ~0x07U);
}

STATIC
CONST IA32_SEGMENT_DESCRIPTOR *
InternalGetDescriptorFromTable (
  IN CONST IA32_DESCRIPTOR  *Table,
  IN UINT16                 Selector
  )
{
  UINT16                        Offset;
  CONST IA32_SEGMENT_DESCRIPTOR *Entry;

  ASSERT (Table != NULL);

  Offset = InternalSelectorGetOffset (Selector);
  ASSERT ((Offset + sizeof (*Entry)) <= (Table->Limit + 1));

  Entry = (IA32_SEGMENT_DESCRIPTOR *)(Table->Base + Offset);
  return Entry;
}

STATIC
VOID
InternalInitializeSaveStateSelector (
  IN  UINT16                        Selector,
  OUT AMD_VMCB_SAVE_STATE_SELECTOR  *SaveState,
  IN  CONST IA32_DESCRIPTOR         *Gdt,
  IN  CONST IA32_DESCRIPTOR         *Ldt
  )
{
  CONST IA32_DESCRIPTOR         *Table;
  UINT16                        Offset;
  CONST IA32_SEGMENT_DESCRIPTOR *Entry;

  ASSERT (Gdt != NULL);
  ASSERT (Ldt != NULL);
  ASSERT (SaveState != NULL);
  //
  // Check the Table Indicator for which table to use.  [0] = GDT, [1] = LDT
  //
  Table = Gdt;
  if ((Selector & BIT2) != 0) {
    Table = Ldt;
  }

  Offset = InternalSelectorGetOffset (Selector);
  ASSERT ((Offset + sizeof (*Entry)) <= (Table->Limit + 1));

  Entry = InternalGetDescriptorFromTable (Table, Selector);
  SaveState->Selector   = Selector;
  SaveState->Attributes = InternalSegmentGetAttributes (Entry);
  SaveState->Limit      = InternalSegmentGetLimit (Entry);
  SaveState->Base       = InternalSegmentGetBase (Entry);
}

STATIC
VOID
InternalLaunchVmEnvironment (
  IN CONST AMD_INTEL_EMU_THREAD_PRIVATE  *Private
  )
{
  AMD_INTEL_EMU_THREAD_CONTEXT    *ThreadContext;
  AMD_VMCB_CONTROL_AREA           *GuestVmcb;
  AMD_VMCB_SAVE_STATE_AREA_NON_ES *SaveState;
  MSR_VM_CR_REGISTER              VmCrMsr;
  MSR_AMD_EFER_REGISTER           EferMsr;
  IA32_DESCRIPTOR                 Gdtr;
  CONST IA32_SEGMENT_DESCRIPTOR   *LdtrDesc;
  IA32_DESCRIPTOR                 Ldtr;
  IA32_DESCRIPTOR                 Idtr;
  UINTN                           Cr0;
  MSR_IA32_PAT_REGISTER           PatMsr;
  UINT32                          Limit;

  ASSERT (Private != NULL);

  ASSERT (Private->HostVmcb != NULL);
  ASSERT (Private->HostStack != NULL);

  AsmWriteMsr64 (MSR_VM_HSAVE_PA, (UINTN)Private->HostVmcb);

  ThreadContext = Private->ThreadContext;
  ASSERT (ThreadContext != NULL);

  GuestVmcb = ThreadContext->Vmcb;
  ASSERT (GuestVmcb != NULL);

  SaveState = (AMD_VMCB_SAVE_STATE_AREA_NON_ES *)(
                (UINTN)GuestVmcb->VmcbSaveState
                );
  //
  // Disable interrupts to guarantee an atomic state switch.  vmrun will
  // re-enable it once the guest has launched.
  //
  DisableInterrupts ();
  //
  // Enable SVM.
  //
  VmCrMsr.Uint64 = AsmReadMsr64 (MSR_VM_CR);
  //
  // The VMRUN, VMLOAD, VMSAVE, CLGI, VMMCALL, and INVLPGA instructions can be
  // used when the EFER.SVME is set to 1
  //
  EferMsr.Uint64 = AsmReadMsr64 (MSR_IA32_EFER);
  if (EferMsr.Bits.SVME == 0) {
    ASSERT (VmCrMsr.Bits.SVMDIS == 0);
    EferMsr.Bits.SVME = 1;
    AsmWriteMsr64 (MSR_IA32_EFER, EferMsr.Uint64);
  }
  //
  // The effect of turning off EFER.SVME while a guest is running is undefined;
  // therefore, the VMM should always prevent guests from writing EFER.SVM
  // extensions can be disabled by setting VM_CR.SVME_DISABLE.  For more
  // information, see descriptions of LOCK and SMVE_DISABLE bits in Section
  // 15.30.1, "VM_CR MSR (C001_0114h)" on page 526.
  //
  if (VmCrMsr.Bits.LOCK == 0) {
    VmCrMsr.Bits.LOCK = 1;
    AsmWriteMsr64 (MSR_VM_CR, VmCrMsr.Uint64);
  }
  //
  // Set up the Save State structure.
  //
  AsmReadGdtr (&Gdtr);
  AsmReadIdtr (&Idtr);

  LdtrDesc = InternalGetDescriptorFromTable (&Gdtr, AsmReadLdtr ());
  Limit    = InternalSegmentGetLimit (LdtrDesc);
  ASSERT (Limit == (UINT16)Limit);

  Ldtr.Base  = InternalSegmentGetBase (LdtrDesc);
  Ldtr.Limit = (UINT16)Limit;
  //
  // The following VMCB Save State members do not need to be initialized as
  // they are only used with vmsave/vmload:
  //   * FS, GS, TR, LDTR (including all hidden state)
  //   * KernelGsBase
  //   * STAR, LSTAR, CSTAR, SFMASK
  //   * SYSENTER_CS, SYSENTER_ESP, SYSENTER_EIP
  //
  SaveState->GDTR.Base  = Gdtr.Base;
  SaveState->GDTR.Limit = Gdtr.Limit;
  SaveState->IDTR.Base  = Idtr.Base;
  SaveState->IDTR.Limit = Idtr.Limit;
  InternalInitializeSaveStateSelector (
    AsmReadEs (),
    &SaveState->ES,
    &Gdtr,
    &Ldtr
    );
  InternalInitializeSaveStateSelector (
    AsmReadCs (),
    &SaveState->CS,
    &Gdtr,
    &Ldtr
    );
  InternalInitializeSaveStateSelector (
    AsmReadSs (),
    &SaveState->SS,
    &Gdtr,
    &Ldtr
    );
  InternalInitializeSaveStateSelector (
    AsmReadDs (),
    &SaveState->DS,
    &Gdtr,
    &Ldtr
    );
  //
  // RIP and RSP are set in NASM, RAX may be 0 as it is non-volatile (EFIAPI).
  //
  Cr0              = AsmReadCr0 ();
  SaveState->EFER  = EferMsr.Uint64;
  SaveState->CR4   = AsmReadCr4 ();
  SaveState->CR3   = AsmReadCr3 ();
  if (GuestVmcb->NP_ENABLE != 0) {
    SaveState->G_PAT = AsmReadMsr64 (MSR_IA32_PAT);
  }
  SaveState->CR0   = Cr0;
  SaveState->DR7   = AsmReadDr7 ();
  SaveState->DR6   = AsmReadDr6 ();
  SaveState->CR2   = AsmReadCr2 ();
  //
  // Enable caching on the host, this means the guest has control of the CD
  // setting.  This will be rolled back due to the guest context switch when
  // returning from AmdIntelEmuInternalEnableVm() if caching has been disabled
  // before.
  //
  if ((Cr0 & CR0_CD) != 0) {
    AsmWriteCr0 (Cr0 & ~(UINTN)CR0_CD);
  }
  //
  // Set all host PAT Types to WB as this type ensures the best compatibility
  // across all Guest PAT Types when combining as per 24593, Table 15-19.
  //
  PatMsr.Uint64   = 0;
  PatMsr.Bits.PA0 = PAT_WB;
  PatMsr.Bits.PA1 = PAT_WB;
  PatMsr.Bits.PA2 = PAT_WB;
  PatMsr.Bits.PA3 = PAT_WB;
  PatMsr.Bits.PA4 = PAT_WB;
  PatMsr.Bits.PA5 = PAT_WB;
  PatMsr.Bits.PA6 = PAT_WB;
  PatMsr.Bits.PA7 = PAT_WB;
  AsmWriteMsr64 (MSR_IA32_PAT, PatMsr.Uint64);
  //
  // Disable single-stepping for the host.
  // The registers will be restored by entering the VM below.
  //
  SaveState->RFLAGS = AmdIntelEmuInternalDisableTf ();
  //
  // Virtualize the current execution environment.  This call will return here.
  //
  ASSERT (Private->EnableVm != NULL);
  Private->EnableVm (
             ThreadContext,
             (VOID *)((UINTN)Private->HostStack + (NUM_STACK_PAGES * SIZE_4KB))
             );
}

STATIC
VOID
EFIAPI
InternalVirtualizeAp (
  IN OUT VOID  *Buffer
  )
{
  AMD_INTEL_EMU_THREAD_PRIVATE *ThreadPrivate;

  ASSERT (Buffer != NULL);

  ThreadPrivate = (AMD_INTEL_EMU_THREAD_PRIVATE *)Buffer;
  //
  // Sync AP TSC stamp counter with the BSP's.
  //
  AsmWriteMsr64 (MSR_IA32_TIME_STAMP_COUNTER, ThreadPrivate->TscStamp);
  InternalLaunchVmEnvironment (ThreadPrivate);
}

STATIC
BOOLEAN
InternalSplitAndUnmapPageWorker (
  IN PHYSICAL_ADDRESS  Address,
  IN UINTN             Size,
  IN UINTN             Start,
  IN UINTN             End
  )
{
  //
  // Verify the page passed is contained in the VMM data range
  // or the VMM data range is contained in the page passed.
  //
  if (((Address >= Start) && ((Address + Size) <= End))
   || ((Start >= Address) && (End <= (Address + Size)))) {
    return TRUE;
  }
  //
  // ASSERT there is no inter-page overlap.
  //
  ASSERT ((End <= Address) || (Start >= (Address + Size)));

  return FALSE;
}

STATIC
BOOLEAN
InternalSplitAndUnmapPage (
  IN CONST VOID           *Context,
  IN PHYSICAL_ADDRESS     Address,
  IN UINTN                Size,
  IN PAGE_TABLE_4K_ENTRY  *PageTableEntry4K  OPTIONAL
  )
{
  BOOLEAN                             Result;

  CONST AMD_INTEL_EMU_PT_INIT_CONTEXT *Private;
  AMD_INTEL_EMU_THREAD_CONTEXT        *ThreadContext;
  UINTN                               Start;
  UINTN                               End;
  UINTN                               Index;
  AMD_INTEL_EMU_MMIO_INFO             *MmioInfo;

  ASSERT (Context != NULL);
  ASSERT (Size > 0);

  Private = (AMD_INTEL_EMU_PT_INIT_CONTEXT *)Context;
  Start   = Private->Address;
  End     = (Start + Private->Size);

  Result = InternalSplitAndUnmapPageWorker (
             Address,
             Size,
             Start,
             End
             );
  if (Result) {
    return TRUE;
  }

  ThreadContext = Private->CurrentThreadContext;
  ASSERT (ThreadContext != NULL);

  for (Index = 0; Index < ThreadContext->NumMmioInfo; ++Index) {
    MmioInfo = &ThreadContext->MmioInfo[Index];
    Result = InternalSplitAndUnmapPageWorker (
                Address,
                Size,
                MmioInfo->Address,
                (MmioInfo->Address + SIZE_4KB)
                );
    if (Result) {
      MmioInfo->Pte = PageTableEntry4K;
      return TRUE;
    }
  }

  return FALSE;
}

STATIC
VOID
InternalInitMsrPm (
  IN     UINTN                                   NumMsrIntercepts,
  IN     CONST AMD_INTEL_EMU_MSR_INTERCEPT_INFO  *MsrIntercepts,
  IN OUT AMD_VMCB_CONTROL_AREA                   *Vmcb
  )
{
  UINT8                                  *MsrPm;
  UINTN                                  Index;
  CONST AMD_INTEL_EMU_MSR_INTERCEPT_INFO *MsrIntercept;
  UINT64                                 MsrTotalBit;
  UINT32                                 MsrIndex;
  UINT32                                 MsrBit;
  UINT32                                 MsrOffset;

  ASSERT (Vmcb != NULL);

  if (NumMsrIntercepts == 0) {
    return;
  }

  ASSERT (MsrIntercepts != NULL);

  MsrPm = (UINT8 *)(UINTN)Vmcb->MSRPM_BASE_PA;
  ASSERT (MsrPm != NULL);

  Vmcb->InterceptMsrProt = 1;

  for (Index = 0; Index < NumMsrIntercepts; ++Index) {
    MsrIntercept = &MsrIntercepts[Index];

    MsrIndex = MsrIntercept->MsrIndex;
    //
    // Rebase MsrIndex so that MSRs indices handled by subsequent MSR PM
    // vectors are numbered continuously.
    //
    if ((MsrIndex >= 0xC0000000) && (MsrIndex <= 0xC0001FFF)) {
      MsrIndex -= (0xC0000000 - 0x2000);
    } else if ((MsrIndex >= 0xC0010000) && (MsrIndex <= 0xC0011FFF)) {
      MsrIndex -= (0xC0010000 - 0x4000);
    } else if (MsrIndex > 0x1FFF) {
      //
      // The MSR is not covered by the MSR PM as of 24593 Rev 3.30.
      //
      continue;
    }

    MsrTotalBit = ((UINT64)MsrIndex * 2);
    MsrOffset   = (UINT32)(MsrTotalBit / 8);
    MsrBit      = (UINT32)(MsrTotalBit % 8);

    ASSERT (MsrOffset < SIZE_2KB);
    ASSERT ((MsrIntercept->Read != NULL) || (MsrIntercept->Write != NULL));

    if (MsrIntercept->Read != NULL) {
      MsrPm[MsrOffset] |= (1U << MsrBit);
    }

    if (MsrIntercept->Write != NULL) {
      MsrPm[MsrOffset] |= (1U << (MsrBit + 1));
    }
  }
}

STATIC
EFI_STATUS
InternalPrepareAps (
  OUT UINTN  *NumThreads,
  OUT UINTN  *BspNum
  )
{
  EFI_STATUS Status;

  UINTN      NumProcessors;
  UINTN      NumEnabledProcessors;
  UINTN      BspIndex;
  UINTN      Index;

  ASSERT (NumThreads != NULL);
  ASSERT (BspNum != NULL);

  if (PcdGetBool (PcdAmdIntelEmuVirtualizeAps)) {
    Status = MpInitLibInitialize ();
    ASSERT_EFI_ERROR (Status);
    if (EFI_ERROR (Status)) {
      return Status;
    }

    Status = MpInitLibGetNumberOfProcessors (
               &NumProcessors,
               &NumEnabledProcessors
               );
    ASSERT_EFI_ERROR (Status);
    if (EFI_ERROR (Status)) {
      return Status;
    }

    Status = MpInitLibWhoAmI (&BspIndex);
    ASSERT_EFI_ERROR (Status);
    if (EFI_ERROR (Status)) {
      return Status;
    }

    if (NumEnabledProcessors != NumProcessors) {
      for (Index = 0; Index < NumProcessors; ++Index) {
        if (Index != BspIndex) {
          Status = MpInitLibEnableDisableAP (Index, TRUE, NULL);
          if (EFI_ERROR (Status)) {
            DEBUG ((DEBUG_ERROR, "Failed to enable AP %lu.\n", (UINT64)Index));
            return Status;
          }
        }
      }

      DEBUG ((DEBUG_INFO, "Successfully enabled %lu APs.\n", (UINT64)Index));
    }

    *NumThreads = NumProcessors;
    *BspNum     = BspIndex;
  } else {
    *NumThreads = 1;
    *BspNum     = 0;
  }

  return EFI_SUCCESS;
}

STATIC
VOID
AmdIntelEmuVirtualizeSystem (
  VOID
  )
{
  VOID                                   *Memory;
  AMD_INTEL_EMU_THREAD_PRIVATE           ThreadPrivate;
  AMD_INTEL_EMU_RUNTIME_CONTEXT          RuntimeContext;
  AMD_INTEL_EMU_PT_INIT_CONTEXT          PtInitContext;
  UINTN                                  NumMsrIntercepts;
  CONST AMD_INTEL_EMU_MSR_INTERCEPT_INFO *MsrIntercepts;
  AMD_INTEL_EMU_RUNTIME_ENTRY            RuntimeEntry;
  UINTN                                  NumPages;
  UINTN                                  NumProcessors;
  UINTN                                  BspNum;
  UINTN                                  MmioInfoSize;
  VOID                                   *IoPm;
  VOID                                   *MsrPm;
  VOID                                   *LapicMmioPage;
  VOID                                   *HostStacks;
  VOID                                   *HostVmcbs;
  VOID                                   *GuestVmcbs;
  AMD_INTEL_EMU_THREAD_CONTEXT           *ThreadContexts;
  AMD_INTEL_EMU_THREAD_CONTEXT           *ThreadContext;
  AMD_INTEL_EMU_THREAD_CONTEXT           *BspThreadContext;
  AMD_VMCB_CONTROL_AREA                  *GuestVmcb;
  AMD_VMCB_CONTROL_AREA                  *VmcbWalker;
  EFI_STATUS                             Status;
  UINTN                                  Index;
  UINTN                                  Index2;
  EFI_PROCESSOR_INFORMATION              ProcessorInfo;

  Status = InternalPrepareAps (&NumProcessors, &BspNum);
  if (EFI_ERROR (Status)) {
    ASSERT_EFI_ERROR (Status);
    DEBUG ((DEBUG_ERROR, "Failed to prepare APs - %r.\n", Status));
    return;
  }

  //
  // TODO: Allocate the Runtime image and assign its entry point here.
  //
  RuntimeEntry = NULL;

  //
  // AmdIntelEmuVirtualizeSystem() assumes this size setup.
  // Allocate at a 2 MB boundary so they will be covered by a single 2 MB Page
  // Table entry.
  // MSR map: 1 page, IO map: 3 pages, LAPIC MMIO: 1 page, VMCBs: 2 per thread.
  //
  NumPages     = (1 + 3 + 1 + (NumProcessors * (NUM_STACK_PAGES + 2)));
  MmioInfoSize = (ARRAY_SIZE (mInternalMmioAddresses) * sizeof (AMD_INTEL_EMU_MMIO_INFO));
  NumPages    += EFI_SIZE_TO_PAGES (
                   NumProcessors
                     * (sizeof (AMD_INTEL_EMU_THREAD_CONTEXT) + MmioInfoSize)
                   );
  Memory = AllocateAlignedReservedPages (NumPages, SIZE_2MB);
  if (Memory == NULL) {
    DEBUG ((DEBUG_ERROR, "Failed to allocate the HV runtime memory.\n"));
    return;
  }

  if (PcdGetBool (PcdAmdIntelEmuInitCpuExceptionHandler)) {
    InitializeCpuExceptionHandlers (NULL);
  }

  //
  // VMRUN is available only at CPL-0.
  // - Assumption: UEFI always runs at CPL-0.
  //
  // Furthermore, the processor must be in protected mode
  // - This library currently only supports long mode, which implicitely is
  //   protected.
  //

  MsrPm         = GET_PAGE (Memory, 0);
  IoPm          = GET_PAGE (MsrPm,  1);
  LapicMmioPage = GET_PAGE (IoPm,   3);
  //
  // Place the stack before the VMCBs and after the LAPIC MMIO page, so if it
  // shall overflow, it does not corrupt any critical VM information.
  // The LAPIC MMIO page's high memory is unused.
  //
  HostStacks     = GET_PAGE (LapicMmioPage, 1);
  HostVmcbs      = GET_PAGE (HostStacks, (NumProcessors * NUM_STACK_PAGES));
  GuestVmcbs     = GET_PAGE (HostVmcbs,  NumProcessors);
  ThreadContexts = GET_PAGE (GuestVmcbs, NumProcessors);
  //
  // Zero MsrPm, IoPm and GuestVmcbs.
  //
  ZeroMem (MsrPm,      (4 * SIZE_4KB));
  ZeroMem (GuestVmcbs, (NumProcessors * SIZE_4KB));
  //
  // Set up the generic VMCB used on all cores.
  //
  GuestVmcb = (AMD_VMCB_CONTROL_AREA *)GuestVmcbs;
  GuestVmcb->InterceptCpuid = 1;
  //
  // The current implementation requires that the VMRUN intercept always be set
  // in the VMCB.
  //
  GuestVmcb->InterceptVmrun = 1;
  GuestVmcb->IOPM_BASE_PA   = (UINT64)(UINTN)IoPm;
  GuestVmcb->MSRPM_BASE_PA  = (UINT64)(UINTN)MsrPm;
  GuestVmcb->GuestAsid      = 1;
  GuestVmcb->VmcbSaveState  = ((UINT64)(UINTN)GuestVmcb + 0x400);
  //
  // Perform NP initialization based on Runtime-returned information.
  //
  if (mInternalNpSupport) {
    GuestVmcb->NP_ENABLE = 1;
  }
  //
  // Initialize the runtime environment.
  //
  RuntimeContext.NumThreads     = NumProcessors;
  RuntimeContext.NpEnabled      = mInternalNpSupport;
  RuntimeContext.NripSupport    = mInternalNripSupport;
  RuntimeContext.ThreadContexts = ThreadContexts;
  RuntimeContext.LapicPage      = LapicMmioPage;
  RuntimeEntry (
    &RuntimeContext,
    &ThreadPrivate.EnableVm,
    &NumMsrIntercepts,
    &MsrIntercepts
    );

  InternalInitMsrPm (NumMsrIntercepts, MsrIntercepts, GuestVmcb);
  //
  // Copy the template to all VMCBs.
  //
  for (Index = 1; Index < NumProcessors; ++Index) {
    VmcbWalker = GET_PAGE (GuestVmcbs, Index);
    CopyMem (VmcbWalker, GuestVmcb, SIZE_4KB);
    VmcbWalker->VmcbSaveState = ((UINT64)(UINTN)VmcbWalker + 0x400);
  }
  //
  // Initialize Thread Contexts.
  //
  for (
    Index = 0, ThreadContext = ThreadContexts;
    Index < NumProcessors;
    ++Index, ThreadContext = GET_NEXT_THREAD_CONTEXT (ThreadContext)
    ) {
    ThreadContext->NumMmioInfo = ARRAY_SIZE (mInternalMmioAddresses);

    for (Index2 = 0; Index2 < ThreadContext->NumMmioInfo; ++Index2) {
      ThreadContext->MmioInfo[Index2].Address = mInternalMmioAddresses[Index2];
    }
  }

  PtInitContext.Address = (UINTN)Memory;
  PtInitContext.Size    = EFI_PAGES_TO_SIZE (NumPages);

  if (PcdGetBool (PcdAmdIntelEmuVirtualizeAps)) {
    //
    // Enable AP virtualization.
    //
    BspThreadContext = NULL;
    for (
      Index = 0, ThreadContext = ThreadContexts;
      Index < NumProcessors;
      ++Index, ThreadContext = GET_NEXT_THREAD_CONTEXT (ThreadContext)
      ) {
      GuestVmcb = GET_PAGE (GuestVmcbs, Index);
      ThreadContext->Vmcb = GuestVmcb;
      //
      // Every Guest VMCB needs its own Page Table so that MMIO interceptions
      // can operate in parallel.
      //
      if (GuestVmcb->NP_ENABLE != 0) {
        PtInitContext.CurrentThreadContext = ThreadContext;
        GuestVmcb->N_CR3 = CreateIdentityMappingPageTables (
                             &PtInitContext,
                             InternalSplitAndUnmapPage
                             );
      }

      if (Index == BspNum) {
        BspThreadContext = ThreadContext;
        continue;
      }

      Status = MpInitLibGetProcessorInfo (
                 Index,
                 &ProcessorInfo,
                 NULL
                 );
      ASSERT_EFI_ERROR (Status);
      if (EFI_ERROR (Status)
       || ((ProcessorInfo.StatusFlag & PROCESSOR_ENABLED_BIT) == 0)) {
        ASSERT (FALSE);
        continue;
      }
      //
      // Pages are identity-mapped across all cores and the APs are only going to
      // read data, hence using stack memory is safe.
      //
      ThreadPrivate.ThreadContext = ThreadContext;
      ThreadPrivate.HostVmcb      = GET_PAGE (HostVmcbs, Index);
      ThreadPrivate.HostStack     = GET_PAGE (
                                      HostStacks,
                                      (Index * NUM_STACK_PAGES)
                                      );
      ThreadPrivate.TscStamp = AsmReadTsc ();
      Status = MpInitLibStartupThisAP (
                 InternalVirtualizeAp,
                 Index,
                 NULL,
                 0,
                 &ThreadPrivate,
                 NULL
                 );
      ASSERT_EFI_ERROR (Status);
    }
  } else {
    BspThreadContext       = ThreadContexts;
    GuestVmcb              = GET_PAGE (GuestVmcbs, BspNum);
    BspThreadContext->Vmcb = GuestVmcb;
    //
    // Every Guest VMCB needs its own Page Table so that MMIO interceptions
    // can operate in parallel.
    //
    if (GuestVmcb->NP_ENABLE != 0) {
      PtInitContext.CurrentThreadContext = BspThreadContext;
      GuestVmcb->N_CR3 = CreateIdentityMappingPageTables (
                           &PtInitContext,
                           InternalSplitAndUnmapPage
                           );
    }
  }
  //
  // Enable BSP virtualization.
  //
  ASSERT (BspThreadContext != NULL);
  ThreadPrivate.ThreadContext = BspThreadContext;
  ThreadPrivate.HostVmcb      = GET_PAGE (HostVmcbs, BspNum);
  ThreadPrivate.HostStack     = GET_PAGE (
                                  HostStacks,
                                  (BspNum * NUM_STACK_PAGES)
                                  );
  InternalLaunchVmEnvironment (&ThreadPrivate);

  if (PcdGetBool (PcdAmdIntelEmuTestIntercepts)) {
    AmdIntelEmuRunTestIntercepts ();
  }
}

VOID
EFIAPI
InternalExitBootServices (
  IN EFI_EVENT  Event,
  IN VOID       *Context
  )
{
  ASSERT (Event != NULL);
  AmdIntelEmuVirtualizeSystem ();
}

EFI_STATUS
EFIAPI
AmdIntelEmuDxeEntryPoint (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS Status;
  EFI_EVENT  Event;

  if (!InternalIsSvmAvailable (&mInternalNripSupport, &mInternalNpSupport)) {
    return EFI_UNSUPPORTED;
  }

  if (PcdGetBool (PcdAmdIntelEmuImmediatelyVirtualize)) {
    //
    // Immediately virtualize the system in DEBUG builds to easen debugging.
    //
    AmdIntelEmuVirtualizeSystem ();
    return EFI_SUCCESS;
  }

  Status = gBS->CreateEvent (
                  EVT_SIGNAL_EXIT_BOOT_SERVICES,
                  TPL_CALLBACK,
                  InternalExitBootServices,
                  NULL,
                  &Event
                  );
  ASSERT_EFI_ERROR (Status);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Failed to create the HV ExitBS event.\n"));
    return Status;
  }

  return EFI_SUCCESS;
}

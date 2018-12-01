#include <PiDxe.h>

#include <Register/Amd/Cpuid.h>
#include <Register/Msr.h>

#include <Protocol/MpService.h>

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>

#include "AmdIntelEmu.h"

#define NUM_STACK_PAGES  1U

#define GET_PAGE(Address, Index)  \
  ((VOID *)((UINTN)(Address) + ((Index) * SIZE_4KB)))

typedef struct {
  AMD_VMCB_CONTROL_AREA *GuestVmcb;
  AMD_VMCB_CONTROL_AREA *HostVmcb;
  VOID                  *HostStack;
  UINT64                TscStamp;
} AMD_INTEL_EMU_THREAD_PRIVATE;

typedef struct {
  UINTN                    Address;
  UINTN                    Size;
  EFI_MP_SERVICES_PROTOCOL *MpServices;
  UINTN                    NumProcessors;
  UINTN                    NumEnabledProcessors;
  UINTN                    BspNum;
} AMD_INTEL_EMU_PRIVATE;

STATIC AMD_INTEL_EMU_THREAD_CONTEXT *mInternalThreadContexts   = NULL;
STATIC UINTN                        mInternalNumThreadContexts = 0;

GLOBAL_REMOVE_IF_UNREFERENCED BOOLEAN mAmdIntelEmuInternalNrip = FALSE;
GLOBAL_REMOVE_IF_UNREFERENCED BOOLEAN mAmdIntelEmuInternalNp   = FALSE;

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
BOOLEAN
InternalIsSvmAvailable (
	VOID
  )
{
  CPUID_AMD_EXTENDED_CPU_SIG_ECX           ExtendedCpuSigEcx;
  CPUID_AMD_SVM_FEATURE_IDENTIFICATION_EDX SvmFeatureIdEdx;
  MSR_VM_CR_REGISTER                       VmCrRegister;
  
  AsmCpuid (
    CPUID_EXTENDED_CPU_SIG,
    NULL,
    NULL,
    &ExtendedCpuSigEcx.Uint32,
    NULL
    );
  if (ExtendedCpuSigEcx.Bits.SVM == 0) {
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
      VmCrRegister.Bits.SVMDIS = 0;
      AsmWriteMsr64 (MSR_VM_CR, VmCrRegister.Uint64);
      DEBUG_CODE (
        VmCrRegister.Uint64 = AsmReadMsr64 (MSR_VM_CR);
        ASSERT (VmCrRegister.Bits.SVMDIS == 0);
        );
    } else {
      return FALSE;
    }
  }
  //
  // Cache the used AMD SVM capabilities.
  //
  if (SvmFeatureIdEdx.Bits.NRIPS != 0) {
    mAmdIntelEmuInternalNrip = TRUE;
  }

  if (SvmFeatureIdEdx.Bits.NP != 0) {
    mAmdIntelEmuInternalNp = TRUE;
  }

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
  // an offset.  The same is achieved by stripping the least significant byte.
  //
  return (Selector & ~0xFFU);
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

  ASSERT (Private->GuestVmcb != NULL);
  ASSERT (Private->HostVmcb != NULL);
  ASSERT (Private->HostStack != NULL);

  AsmWriteMsr64 (MSR_VM_HSAVE_PA, (UINTN)Private->HostVmcb);

  GuestVmcb = Private->GuestVmcb;
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
  AmdIntelEmuInternalEnableVm (GuestVmcb, Private->HostStack);
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
  IN CONST AMD_INTEL_EMU_PRIVATE *Private,
  IN PHYSICAL_ADDRESS            Address,
  IN UINTN                       Size,
  IN UINTN                       Start,
  IN UINTN                       End
  )
{
  if (Private->Size > Size) {
    //
    // Verify the page passed is contained in the VMM data range.
    //
    if ((Address >= Start) && ((Address + Size) <= End)) {
      return TRUE;
    }
  } else {
    //
    // Verify the VMM data range is contained in the page passed.
    //
    if ((Start >= Address) && (End <= (Address + Size))) {
      return TRUE;
    }
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
  BOOLEAN                     Result;

  CONST AMD_INTEL_EMU_PRIVATE *Private;
  UINTN                       Start;
  UINTN                       End;
  UINTN                       Index;
  AMD_INTEL_EMU_MMIO_INFO     *MmioInfo;

  ASSERT (Context != NULL);
  ASSERT (Size > 0);

  Private = (AMD_INTEL_EMU_PRIVATE *)Context;
  Start   = Private->Address;
  End     = (Start + Private->Size);

  Result = InternalSplitAndUnmapPageWorker (
             Private,
             Address,
             Size,
             Start,
             End
             );
  if (Result) {
    return TRUE;
  }

  if (PageTableEntry4K != NULL) {
    for (Index = 0; Index < mAmdIntelEmuInternalNumMmioInfo; ++Index) {
      MmioInfo = &mAmdIntelEmuInternalMmioInfo[Index];
      Result = InternalSplitAndUnmapPageWorker (
                 Private,
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
  }

  return FALSE;
}

STATIC
VOID
AmdEmuVirtualizeSystem (
  IN OUT VOID  *Memory
  )
{
  AMD_INTEL_EMU_PRIVATE        Private;
  AMD_INTEL_EMU_THREAD_PRIVATE ThreadPrivate;
  VOID                         *IoPm;
  VOID                         *MsrPm;
  VOID                         *LapicMmioPage;
  VOID                         *HostStacks;
  VOID                         *HostVmcbs;
  VOID                         *GuestVmcbs;
  AMD_INTEL_EMU_THREAD_CONTEXT *ThreadContexts;
  AMD_VMCB_CONTROL_AREA        *GuestVmcb;
  AMD_VMCB_CONTROL_AREA        *VmcbWalker;
  EFI_STATUS                   Status;
  UINTN                        Index;
  UINTN                        NumFinished;
  EFI_PROCESSOR_INFORMATION    ProcessorInfo;
  UINTN                        BspOffset;

  CopyMem (&Private, Memory, sizeof (Private));

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
  HostVmcbs      = GET_PAGE (HostStacks,    Private.NumEnabledProcessors);
  GuestVmcbs     = GET_PAGE (HostVmcbs,     Private.NumEnabledProcessors);
  ThreadContexts = GET_PAGE (GuestVmcbs,    Private.NumEnabledProcessors);

  mInternalThreadContexts    = ThreadContexts;
  mInternalNumThreadContexts = Private.NumEnabledProcessors;

  AmdIntelEmuInternalMmioLapicSetPage (LapicMmioPage);
  //
  // Zero MsrPm, IoPm and GuestVmcbs.
  //
  ZeroMem (MsrPm,      (4 * SIZE_4KB));
  ZeroMem (GuestVmcbs, (Private.NumEnabledProcessors * SIZE_4KB));
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
  if (mAmdIntelEmuInternalNp) {
    GuestVmcb->NP_ENABLE = 1;
    GuestVmcb->N_CR3     = CreateIdentityMappingPageTables (
                             &Private,
                             InternalSplitAndUnmapPage
                             );
  }
  GuestVmcb->VmcbSaveState  = ((UINT64)(UINTN)GuestVmcb + 0x400);
  AmdIntelEmuInternalInitMsrPm (GuestVmcb);
  //
  // Copy the template to all VMCBs.
  //
  for (Index = 1; Index < Private.NumEnabledProcessors; ++Index) {
    VmcbWalker = GET_PAGE (GuestVmcb, Index);
    CopyMem (VmcbWalker, GuestVmcb, SIZE_4KB);
    VmcbWalker->VmcbSaveState = ((UINT64)(UINTN)VmcbWalker + 0x400);
  }
  //
  // Enable AP virtualization.
  //
  NumFinished = 0;
  BspOffset   = 0;
  for (Index = 0; Index < Private.NumProcessors; ++Index) {
    GuestVmcb = GET_PAGE (GuestVmcbs, NumFinished);
    ThreadContexts[Index].Vmcb = GuestVmcb;

    if (Index == Private.BspNum) {
      BspOffset = NumFinished;
      ++NumFinished;
      continue;
    }

    Status = Private.MpServices->GetProcessorInfo (
                                   Private.MpServices,
                                   Index,
                                   &ProcessorInfo
                                   );
    ASSERT_EFI_ERROR (Status);
    if (EFI_ERROR (Status)
     || ((ProcessorInfo.StatusFlag & PROCESSOR_ENABLED_BIT) == 0)) {
      continue;
    }

    DEBUG_CODE (
      if (NumFinished == Private.NumEnabledProcessors) {
        ASSERT (FALSE);
        break;
      }
      );
    //
    // Pages are identity-mapped across all cores and the APs are only going to
    // read data, hence using stack memory is safe.
    //
    ThreadPrivate.GuestVmcb = GuestVmcb;
    ThreadPrivate.HostVmcb  = GET_PAGE (HostVmcbs, NumFinished);
    ThreadPrivate.HostStack = GET_PAGE (HostStacks, NumFinished);
    ThreadPrivate.TscStamp  = AsmReadTsc ();
    Status = Private.MpServices->StartupThisAP (
                                   Private.MpServices,
                                   InternalVirtualizeAp,
                                   Index,
                                   NULL,
                                   0,
                                   &ThreadPrivate,
                                   NULL
                                   );
    ASSERT_EFI_ERROR (Status);

    ++NumFinished;

    if (NumFinished == Private.NumEnabledProcessors) {
      DEBUG_CODE (
        continue;
        );
      break;
    }
  }
  //
  // Enable BSP virtualization.
  //
  ThreadPrivate.GuestVmcb = GET_PAGE (GuestVmcbs, BspOffset);
  ThreadPrivate.HostVmcb  = GET_PAGE (HostVmcbs,  BspOffset);
  ThreadPrivate.HostStack = GET_PAGE (HostStacks, BspOffset);
  InternalLaunchVmEnvironment (&ThreadPrivate);
}

VOID
EFIAPI
InternalExitBootServices (
  IN EFI_EVENT  Event,
  IN VOID       *Context
  )
{
  ASSERT (Event != NULL);
  ASSERT (Context != NULL);
  AmdEmuVirtualizeSystem (Context);
}

EFI_STATUS
EFIAPI
AmdIntelEmuEntryPoint (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  UINTN                    NumPages;
  AMD_INTEL_EMU_PRIVATE    *Memory;
  EFI_STATUS               Status;
  EFI_MP_SERVICES_PROTOCOL *MpServices;
  UINTN                    NumProcessors;
  UINTN                    NumEnabledProcessors;
  UINTN                    BspNum;
  UINTN                    Index;
  EFI_EVENT                Event;

  if (!InternalIsSvmAvailable ()) {
    return EFI_UNSUPPORTED;
  }

  Status = gBS->LocateProtocol (
                  &gEfiMpServiceProtocolGuid,
                  NULL,
                  (VOID **)&MpServices
                  );
  ASSERT_EFI_ERROR (Status);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = MpServices->GetNumberOfProcessors (
                         MpServices,
                         &NumProcessors,
                         &NumEnabledProcessors
                         );
  ASSERT_EFI_ERROR (Status);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = MpServices->WhoAmI (MpServices, &BspNum);
  ASSERT_EFI_ERROR (Status);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  NumEnabledProcessors = NumProcessors;

  for (Index = 0; Index < NumProcessors; ++Index) {
    if (Index != BspNum) {
      Status = MpServices->EnableDisableAP (MpServices, Index, TRUE, NULL);
      if (EFI_ERROR (Status)) {
        --NumEnabledProcessors;
      }
    }
  }
  //
  // AmdEmuVirtualizeSystem() assumes this size setup.
  // Allocate at a 2 MB boundary so they will be covered by a single 2 MB Page
  // Table entry.
  // MSR map: 1 page, IO map: 3 pages, LAPIC MMIO: 1 page, VMCBs: 2 per thread.
  //
  NumPages  = (1 + 3 + 1 + (NumEnabledProcessors * (NUM_STACK_PAGES + 2)));
  NumPages += EFI_SIZE_TO_PAGES (
                NumEnabledProcessors * sizeof (AMD_INTEL_EMU_THREAD_CONTEXT)
                );
  Memory = AllocateAlignedReservedPages (NumPages, SIZE_2MB);
  if (Memory == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  Memory->Address              = (UINTN)Memory;
  Memory->Size                 = EFI_PAGES_TO_SIZE (NumPages);
  Memory->MpServices           = MpServices;
  Memory->NumProcessors        = NumProcessors;
  Memory->NumEnabledProcessors = NumEnabledProcessors;
  Memory->BspNum               = BspNum;
  Status = gBS->CreateEvent (
                  EVT_SIGNAL_EXIT_BOOT_SERVICES,
                  TPL_CALLBACK,
                  InternalExitBootServices,
                  Memory,
                  &Event
                  );
  ASSERT_EFI_ERROR (Status);
  if (EFI_ERROR (Status)) {
    FreeAlignedPages (Memory, NumPages);
    return Status;
  }

  return EFI_SUCCESS;
}

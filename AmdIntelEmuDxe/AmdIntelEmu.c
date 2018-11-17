#include <PiDxe.h>

#include <Register/Amd/Cpuid.h>
#include <Register/Msr.h>

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>

#include <Protocol/MpService.h>

#include "AmdIntelEmu.h"

#define AMD_EMU_STACK_PAGES  1U

#define GET_PAGE(TYPE, Address, Index)  \
  ((TYPE *)((UINTN)(Address) + (Index * SIZE_4KB)))

typedef struct {
  AMD_VMCB_CONTROL_AREA  *GuestVmcb;
  AMD_VMCB_CONTROL_AREA  *HostVmcb;
  VOID                   *HostStack;
} AMD_EMU_THREAD_PRIVATE;

typedef struct {
  UINTN                    Size;
  EFI_MP_SERVICES_PROTOCOL *MpServices;
  UINTN                    NumProcessors;
  UINTN                    NumEnabledProcessors;
  UINTN                    BspNum;
  AMD_EMU_THREAD_PRIVATE   Thread;
} AMD_EMU_PRIVATE;

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
  //
  // Disable SVMDIS if the register is unlocked.
  //
  VmCrRegister.Uint64 = AsmReadMsr64 (MSR_VM_CR);
  if (VmCrRegister.Bits.SVMDIS != 0) {
    AsmCpuid (
      CPUID_AMD_SVM_FEATURE_IDENTIFICATION,
      NULL,
      NULL,
      NULL,
      &SvmFeatureIdEdx.Uint32
      );
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
  IN  UINTN                         Selector,
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
  IN OUT   AMD_VMCB_CONTROL_AREA  *GuestVmcb,
  IN CONST AMD_VMCB_CONTROL_AREA  *HostVmcb,
  IN       VOID                   *HostStack
  )
{
  AMD_VMCB_SAVE_STATE_AREA_NON_ES *SaveState;
  MSR_VM_CR_REGISTER              VmCrMsr;
  MSR_AMD_EFER_REGISTER           EferMsr;
  IA32_DESCRIPTOR                 Gdtr;
  CONST IA32_SEGMENT_DESCRIPTOR   *LdtrDesc;
  IA32_DESCRIPTOR                 Ldtr;
  IA32_DESCRIPTOR                 Idtr;
  UINTN                           Cr0;
  MSR_IA32_PAT_REGISTER           PatMsr;

  SaveState = (AMD_VMCB_SAVE_STATE_AREA_NON_ES *)(
                (UINTN)GuestVmcb->VmcbSaveState
                );
  AsmWriteMsr64 (MSR_VM_HSAVE_PA, (UINTN)HostVmcb);
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
  LdtrDesc   = InternalGetDescriptorFromTable (&Gdtr, AsmReadLdtr ());
  Ldtr.Base  = InternalSegmentGetBase (LdtrDesc);
  Ldtr.Limit = InternalSegmentGetLimit (LdtrDesc);
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
  // G_PAT is not set because nested paging is not used.
  //
  Cr0              = AsmReadCr0 ();
  SaveState->EFER  = EferMsr.Uint64;
  SaveState->CR4   = AsmReadCr4 ();
  SaveState->CR3   = AsmReadCr3 ();
  SaveState->G_PAT = AsmReadMsr64 (MSR_IA32_PAT);
  SaveState->CR0   = Cr0;
  SaveState->DR7   = AsmReadDr7 ();
  SaveState->DR6   = AsmReadDr6 ();
  SaveState->CR2   = AsmReadCr2 ();
  //
  // Disable debugging for the host.
  // The registers will be restored by entering the VM below.
  //
  SaveState->RFLAGS = AmdIntelEmuInternalDisableTf ();
  AsmWriteDr6 (0);
  AsmWriteDr7 (0);
  //
  // Enable caching on the host, this means the guest has control of the CD
  // setting.  This will be rolled back due to the guest context switch when
  // returning from AmdEnableVm() if caching has been disabled before.
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
  // Virtualize the current execution environment.  This call will return here.
  //
  AmdEnableVm (GuestVmcb, HostStack);
}

STATIC
VOID
EFIAPI
InternalVirtualizeAp (
  IN OUT VOID  *Buffer
  )
{
  CONST AMD_EMU_THREAD_PRIVATE *Private;

  ASSERT (Buffer != NULL);

  Private = (AMD_EMU_THREAD_PRIVATE *)Buffer;
  InternalLaunchVmEnvironment (
    Private->GuestVmcb,
    Private->HostVmcb,
    Private->HostStack
    );
}

STATIC
BOOLEAN
InternalSplitAndUnmapPage (
  IN CONST VOID            *Context,
  IN EFI_PHYSICAL_ADDRESS  Address,
  IN UINTN                 Size
  )
{
  CONST AMD_EMU_PRIVATE *Private;
  UINTN                 Start;
  UINTN                 End;

  ASSERT (Context != NULL);
  ASSERT (Size > 0);

  Private = (AMD_EMU_PRIVATE *)Context;
  Start   = (UINTN)Private;
  End     = (Start + Private->Size);

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
VOID
AmdEmuVirtualizeSystem (
  IN OUT AMD_EMU_PRIVATE  *Context
  )
{
  VOID                      *IoPm;
  VOID                      *MsrPm;
  VOID                      *HostStacks;
  VOID                      *HostVmcbs;
  VOID                      *GuestVmcbs;
  AMD_VMCB_CONTROL_AREA     *GuestVmcb;
  AMD_VMCB_CONTROL_AREA     *VmcbWalker;
  EFI_STATUS                Status;
  EFI_MP_SERVICES_PROTOCOL  *MpServices;
  UINTN                     Index;
  UINTN                     NumFinished;
  EFI_PROCESSOR_INFORMATION ProcessorInfo;
  UINTN                     BspOffset;

  //
  // VMRUN is available only at CPL-0.
  // - Assumption: UEFI always runs at CPU-0.
  //
  // Furthermore, the processor must be in protected mode
  // - This library currently only supports long mode, which implicitely is
  //   protected.
  //

  MsrPm = GET_PAGE (VOID, Context, 0);
  IoPm  = GET_PAGE (VOID, MsrPm,   1);
  //
  // Place the stack before the VMCBs and after the Protection Maps, so if it
  // shall overflow, it does not corrupt any critical VM information.
  // The PMs' high memory is reserved.
  //
  HostStacks = GET_PAGE (VOID, IoPm,       3);
  HostVmcbs  = GET_PAGE (VOID, HostStacks, Context->NumEnabledProcessors);
  GuestVmcbs = GET_PAGE (VOID, HostVmcbs,  Context->NumEnabledProcessors);
  //
  // Zero MsrPm, IoPm and GuestVmcbs.
  //
  ZeroMem (MsrPm,      (4 * SIZE_4KB));
  ZeroMem (GuestVmcbs, (Context->NumEnabledProcessors * SIZE_4KB));
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
  GuestVmcb->NP_ENABLE      = 1;
  GuestVmcb->N_CR3          = CreateIdentityMappingPageTables (
                                Context,
                                InternalSplitAndUnmapPage
                                );
  GuestVmcb->VmcbSaveState  = ((UINT64)(UINTN)GuestVmcb + 0x400);
  AmdIntelEmuInternalInitMsrPm (GuestVmcb);
  //
  // Copy the template to all VMCBs.
  //
  for (Index = 1; Index < Context->NumEnabledProcessors; ++Index) {
    VmcbWalker = GET_PAGE (AMD_VMCB_CONTROL_AREA, GuestVmcb, Index);
    CopyMem (VmcbWalker, GuestVmcb, SIZE_4KB);
    VmcbWalker->VmcbSaveState = ((UINT64)(UINTN)VmcbWalker + 0x400);
  }
  //
  // Enable AP virtualization.
  //
  MpServices  = Context->MpServices;
  NumFinished = 0;
  BspOffset   = 0;
  for (Index = 0; Index < Context->NumProcessors; ++Index) {
    if (Index == Context->BspNum) {
      BspOffset = NumFinished;
      ++NumFinished;
      continue;
    }

    Status = MpServices->GetProcessorInfo (
                           MpServices,
                           Index,
                           &ProcessorInfo
                           );
    ASSERT_EFI_ERROR (Status);
    if (EFI_ERROR (Status)
     || ((ProcessorInfo.StatusFlag & PROCESSOR_ENABLED_BIT) == 0)) {
      continue;
    }

    DEBUG_CODE (
      if (NumFinished == Context->NumEnabledProcessors) {
        ASSERT (FALSE);
        break;
      }
      );

    Context->Thread.GuestVmcb = GET_PAGE (
                                  AMD_VMCB_CONTROL_AREA,
                                  GuestVmcbs,
                                  NumFinished
                                  );
    Context->Thread.HostVmcb = GET_PAGE (
                                 AMD_VMCB_CONTROL_AREA,
                                 HostVmcbs,
                                 NumFinished
                                 );
    Context->Thread.HostStack = GET_PAGE (VOID, HostStacks, NumFinished);
    Status = MpServices->StartupThisAP (
                           MpServices,
                           InternalVirtualizeAp,
                           Index,
                           NULL,
                           0,
                           &Context->Thread,
                           NULL
                           );
    ASSERT_EFI_ERROR (Status);

    ++NumFinished;

    if (NumFinished == Context->NumEnabledProcessors) {
      DEBUG_CODE (
        continue;
        );
      break;
    }
  }
  //
  // Enable BSP virtualization.
  //
  InternalLaunchVmEnvironment (
    GET_PAGE (AMD_VMCB_CONTROL_AREA, GuestVmcbs, BspOffset),
    GET_PAGE (AMD_VMCB_CONTROL_AREA, HostVmcbs, BspOffset),
    GET_PAGE (VOID, HostStacks, BspOffset)
    );
}

VOID
EFIAPI
InternalExitBootServices (
  IN EFI_EVENT  Event,
  IN VOID       *Context
  )
{
  ASSERT (Context != NULL);
  AmdEmuVirtualizeSystem ((AMD_EMU_PRIVATE *)Context);
}

BOOLEAN
AmdEmuEntryPoint (
  VOID
  )
{
  UINTN                    Size;
  AMD_EMU_PRIVATE          *Memory;
  EFI_STATUS               Status;
  EFI_MP_SERVICES_PROTOCOL *MpServices;
  UINTN                    NumProcessors;
  UINTN                    NumEnabledProcessors;
  UINTN                    BspNum;
  UINTN                    Index;
  EFI_EVENT                Event;

  if (!InternalIsSvmAvailable ()) {
    return FALSE;
  }

  Status = gBS->LocateProtocol (
                  &gEfiMpServiceProtocolGuid,
                  NULL,
                  (VOID **)&MpServices
                  );
  ASSERT_EFI_ERROR (Status);
  if (EFI_ERROR (Status)) {
    return FALSE;
  }

  Status = MpServices->GetNumberOfProcessors (
                         MpServices,
                         &NumProcessors,
                         &NumEnabledProcessors
                         );
  ASSERT_EFI_ERROR (Status);
  if (EFI_ERROR (Status)) {
    return FALSE;
  }

  Status = MpServices->WhoAmI (MpServices, &BspNum);
  ASSERT_EFI_ERROR (Status);
  if (EFI_ERROR (Status)) {
    return FALSE;
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
  //
  Size   = (3 + 1 + (NumEnabledProcessors * (AMD_EMU_STACK_PAGES + 2)));
  Memory = AllocateAlignedReservedPages (Size, SIZE_2MB);
  if (Memory == NULL) {
    return FALSE;
  }

  Memory->Size                 = Size;
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
    return FALSE;
  }

  return TRUE;
}

#include <Base.h>

#include <Protocol/DebugSupport.h>

#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/LocalApicLib.h>

#include "../AmdIntelEmuRuntime.h"

VOID
AmdEmuInterceptCpuid (
  IN OUT AMD_VMCB_CONTROL_AREA    *Vmcb,
  IN OUT AMD_INTEL_EMU_REGISTERS  *Registers
  );

VOID
AmdIntelEmuInternalInterceptRdmsr (
  IN OUT AMD_VMCB_CONTROL_AREA    *Vmcb,
  IN OUT AMD_INTEL_EMU_REGISTERS  *Registers
  );

VOID
AmdIntelEmuInternalInterceptWrmsr (
  IN OUT AMD_VMCB_CONTROL_AREA    *Vmcb,
  IN OUT AMD_INTEL_EMU_REGISTERS  *Registers
  );

VOID
AmdIntelEmuInternalExceptionUd (
  IN OUT AMD_VMCB_CONTROL_AREA    *Vmcb,
  IN OUT AMD_INTEL_EMU_REGISTERS  *Registers
  );

VOID
AmdIntelEmuInternalExceptionIret (
  IN OUT AMD_VMCB_CONTROL_AREA  *Vmcb
  );

VOID
AmdIntelEmuInternalExceptionDebug (
  IN OUT AMD_VMCB_CONTROL_AREA  *Vmcb
  );

VOID
AmdIntelEmuInternalExceptionNpf (
  IN OUT AMD_VMCB_CONTROL_AREA  *Vmcb
  );

///
/// Byte packed structure for Control Register 4 (CR4).
/// 32-bits on IA-32.
/// 64-bits on x64.  The upper 32-bits on x64 are reserved.
///
typedef union {
  struct {
    UINT32  VME:1;          ///< Virtual-8086 Mode Extensions.
    UINT32  PVI:1;          ///< Protected-Mode Virtual Interrupts.
    UINT32  TSD:1;          ///< Time Stamp Disable.
    UINT32  DE:1;           ///< Debugging Extensions.
    UINT32  PSE:1;          ///< Page Size Extensions.
    UINT32  PAE:1;          ///< Physical Address Extension.
    UINT32  MCE:1;          ///< Machine Check Enable.
    UINT32  PGE:1;          ///< Page Global Enable.
    UINT32  PCE:1;          ///< Performance Monitoring Counter
                            ///< Enable.
    UINT32  OSFXSR:1;       ///< Operating System Support for
                            ///< FXSAVE and FXRSTOR instructions
    UINT32  OSXMMEXCPT:1;   ///< Operating System Support for
                            ///< Unmasked SIMD Floating Point
                            ///< Exceptions.
    UINT32  Reserved_0:5;   ///< Reserved.
    UINT32  FSGSBASE:1;     ///< Enable RDFSBASE, RDGSBASE, WRFSBASE, and
                            ///< WRGSBASE instructions.
    UINT32  Reserved_1:1;   ///< Reserved.
    UINT32  OSXSAVE:1;      ///< XSAVE and Processor Extended States Enable
                            ///< Bit.
    UINT32  Reserved_2:1;   ///< Reserved.
    UINT32  SMEP:1;         ///< Supervisor Mode Execution Prevention.
    UINT32  SMAP:1;         ///< Supervisor Mode Access Prevention.
    UINT32  Reserved_3:11;  ///< Reserved.
  } Bits;
  UINTN     UintN;
} AMD_IA32_CR4;

typedef struct {
  UINT64      ExitCode;
  CONST CHAR8 *ExitName;
} INTERNAL_VMEXIT_MAP;

STATIC CONST INTERNAL_VMEXIT_MAP mVmExitMap[] = {
  { VMEXIT_EXCP_DB, "EXCP_DB" },
  { VMEXIT_EXCP_UD, "EXCP_UD" },
  { VMEXIT_EXCP_SX, "EXCP_SX" },
  { VMEXIT_INIT,    "INIT"    },
  { VMEXIT_CPUID,   "CPUID"   },
  { VMEXIT_IRET,    "IRET"    },
  { VMEXIT_MSR,     "MSR"     },
  { VMEXIT_VMRUN,   "VMRUN"   },
  { VMEXIT_NPF,     "NPF"     },
};

VOID
AmdIntelEmuInternalGetRipInstruction (
  IN  CONST AMD_VMCB_SAVE_STATE_AREA_NON_ES  *SaveState,
  OUT hdes                                   *Instruction
  )
{
  UINTN                   Address;
  IA32_SEGMENT_ATTRIBUTES SegAttrib;
  //
  // TODO: Virtual -> Physical RIP.
  //
  ASSERT (FALSE);

  ASSERT (SaveState != NULL);
  ASSERT (Instruction != NULL);

  Address = SaveState->RIP;
  //
  // CS Base is ignored in Long Mode.
  //
  SegAttrib.Uint16 = SaveState->CS.Attributes;
  if (SegAttrib.Bits.L == 0) {
    Address += (SaveState->CS.Base << 4U);
    hde32_disasm ((VOID *)Address, Instruction);
    return;
  }

  hde64_disasm ((VOID *)Address, Instruction);
}

STATIC
VOID
InternalRaiseRipNonJmp (
  IN OUT AMD_VMCB_CONTROL_AREA  *Vmcb
  )
{
  AMD_VMCB_SAVE_STATE_AREA_NON_ES *SaveState;
  hdes                            Instruction;

  ASSERT (Vmcb != NULL);

  SaveState = (AMD_VMCB_SAVE_STATE_AREA_NON_ES *)(UINTN)Vmcb->VmcbSaveState;
  ASSERT (SaveState != NULL);

  if (mAmdIntelEmuInternalNrip) {
    SaveState->RIP = Vmcb->nRIP;
  } else {
    AmdIntelEmuInternalGetRipInstruction (SaveState, &Instruction);
    ASSERT ((Instruction.flags & F_ERROR) == 0);
    SaveState->RIP += Instruction.len;
  }
}

STATIC
VOID
InternalHandleEvents (
  IN OUT AMD_VMCB_CONTROL_AREA  *Vmcb
  )
{
  STATIC CONST UINT8 ExceptionPriorities[] =
    { 18, 1, 2, 32, 1, 13, 14, 6, 7, 13, 0, 10, 11, 12, 13, 14, 16, 17, 19 };

  UINTN          Index;
  UINT8          Priority;
  AMD_VMCB_EVENT QueueEvent;

  ASSERT (Vmcb != NULL);
  //
  // Software Interrupts are discarded.
  //
  if ((Vmcb->EXITINTINFO.Bits.V != 0)
   && (Vmcb->EXITINTINFO.Bits.TYPE != AmdVmcbSoftwareInterrupt)) {
    if (Vmcb->EVENTINJ.Bits.V == 0) {
      Vmcb->EVENTINJ.Uint64 = Vmcb->EXITINTINFO.Uint64;
      return;
    }
    
    if ((Vmcb->EVENTINJ.Bits.TYPE    == AmdVmcbException)
     && (Vmcb->EXITINTINFO.Bits.TYPE == AmdVmcbException)) {
      //
      // Combine into #DF if necessary.
      //
      switch (Vmcb->EXITINTINFO.Bits.VECTOR) {
        case EXCEPT_IA32_DIVIDE_ERROR:
        case EXCEPT_IA32_INVALID_TSS:
        case EXCEPT_IA32_SEG_NOT_PRESENT:
        case EXCEPT_IA32_STACK_FAULT:
        case EXCEPT_IA32_GP_FAULT:
        {
          switch (Vmcb->EVENTINJ.Bits.VECTOR) {
            case EXCEPT_IA32_INVALID_TSS:
            case EXCEPT_IA32_SEG_NOT_PRESENT:
            case EXCEPT_IA32_STACK_FAULT:
            case EXCEPT_IA32_GP_FAULT:
            {
              AmdIntelEmuInternalInjectDf (Vmcb);
              return;
            }

            default:
            {
              break;
            }
          }
        }

        case EXCEPT_IA32_PAGE_FAULT:
        {
          switch (Vmcb->EVENTINJ.Bits.VECTOR) {
            case EXCEPT_IA32_PAGE_FAULT:
            case EXCEPT_IA32_INVALID_TSS:
            case EXCEPT_IA32_SEG_NOT_PRESENT:
            case EXCEPT_IA32_STACK_FAULT:
            case EXCEPT_IA32_GP_FAULT:
            {
              AmdIntelEmuInternalInjectDf (Vmcb);
              return;
            }

            default:
            {
              break;
            }
          }
        }

        default:
        {
          break;
        }
      }
    }
    //
    // Software Interrupts are discarded as they are supposed to be reinvoked
    // when returning to the faulting instruction.  Queue the Software
    // Interrupt to be executed after the VMEXIT interrupt.
    //
    if (Vmcb->EVENTINJ.Bits.TYPE == AmdVmcbSoftwareInterrupt) {
      QueueEvent.Uint64 = Vmcb->EVENTINJ.Uint64;
    } else {
      //
      // Inject the higher priority event and either discard or queue the lower
      // priority event based on source.
      //
      for (Index = 0; Index < ARRAY_SIZE (ExceptionPriorities); ++Index) {
        Priority = ExceptionPriorities[Index];

        if ((Vmcb->EVENTINJ.Bits.VECTOR == Priority)
         || ((Priority == 32)
          && (Vmcb->EVENTINJ.Bits.VECTOR > 32)
          && (Vmcb->EVENTINJ.Bits.VECTOR <= 255))) {
          //
          // Discard internal events.
          //
          if (Vmcb->EXITINTINFO.Bits.TYPE == AmdVmcbException) {
            return;
          }

          QueueEvent.Uint64 = Vmcb->EXITINTINFO.Uint64;

          break;
        }

        if ((Vmcb->EXITINTINFO.Bits.VECTOR == Priority)
         || ((Priority == 32)
          && (Vmcb->EXITINTINFO.Bits.VECTOR > 32)
          && (Vmcb->EXITINTINFO.Bits.VECTOR <= 255))) {
          Vmcb->EVENTINJ.Uint64 = Vmcb->EXITINTINFO.Uint64;
          //
          // Discard internal events.
          //
          if (Vmcb->EVENTINJ.Bits.TYPE == AmdVmcbException) {
            Vmcb->EVENTINJ.Uint64 = Vmcb->EXITINTINFO.Uint64;
            return;
          }

          QueueEvent.Uint64     = Vmcb->EVENTINJ.Uint64;
          Vmcb->EVENTINJ.Uint64 = Vmcb->EXITINTINFO.Uint64;
          break;
        }
      }
    }

    AmdIntelEmuInternalQueueEvent (Vmcb, &QueueEvent);
  }
}

STATIC
VOID
InternalVmcbSanityCheck (
  IN CONST AMD_VMCB_CONTROL_AREA  *Vmcb
  )
{
  CONST AMD_VMCB_SAVE_STATE_AREA_NON_ES *SaveState;
  MSR_AMD_EFER_REGISTER                 EferRegister;
  IA32_CR0                              Cr0;
  AMD_IA32_CR4                          Cr4;
  IA32_SEGMENT_ATTRIBUTES               CsAttributes;

  ASSERT (Vmcb != NULL);

  SaveState = (CONST AMD_VMCB_SAVE_STATE_AREA_NON_ES *)(UINTN)Vmcb->VmcbSaveState;
  ASSERT (SaveState != NULL);

  EferRegister.Uint64 = SaveState->EFER;
  Cr0.UintN           = (UINTN)SaveState->CR0;
  Cr4.UintN           = (UINTN)SaveState->CR4;
  CsAttributes.Uint16 = SaveState->CS.Attributes;

  ASSERT (EferRegister.Bits.SVME != 0);

  ASSERT (
      (Cr4.Bits.Reserved_0 == 0)
   && (Cr4.Bits.Reserved_1 == 0)
   && (Cr4.Bits.Reserved_2 == 0)
   && (Cr4.Bits.Reserved_3 == 0)
    );
  ASSERT (BitFieldRead64 (SaveState->DR6, 32, 63) == 0);
  ASSERT (BitFieldRead64 (SaveState->DR7, 32, 63) == 0);
  ASSERT (
      (EferRegister.Bits.Reserved1 == 0)
   && (EferRegister.Bits.Reserved2 == 0)
   && (EferRegister.Bits.Reserved3 == 0)
   && (EferRegister.Bits.Reserved4 == 0)
    );
  ASSERT (
      (Cr4.Bits.PAE != 0)
   || ((EferRegister.Bits.LME == 0) || (Cr0.Bits.PG == 0))
    );
  ASSERT (
      (Cr0.Bits.PE != 0)
   || ((EferRegister.Bits.LME == 0) || (Cr0.Bits.PG == 0))
    );
  ASSERT (
      (EferRegister.Bits.LME == 0)
   || (Cr0.Bits.PG == 0)
   || (Cr4.Bits.PAE == 0)
   || (CsAttributes.Bits.L == 0)
   || (CsAttributes.Bits.DB == 0)
    );
  ASSERT (Vmcb->InterceptVmrun != 0);
  ASSERT (Vmcb->GuestAsid != 0);
}

STATIC
CONST CHAR8 *
InternalGetVmexitName (
  IN UINT64  ExitCode
  )
{
  UINTN Index;

  for (Index = 0; Index < ARRAY_SIZE (mVmExitMap); ++Index) {
    if (mVmExitMap[Index].ExitCode == ExitCode) {
      return mVmExitMap[Index].ExitName;
    }
  }

  return "UNKNOWN";
}

AMD_VMCB_CONTROL_AREA *
EFIAPI
AmdIntelEmuInternalInterceptionHandler (
  IN OUT AMD_VMCB_CONTROL_AREA    *Vmcb,
  IN OUT AMD_INTEL_EMU_REGISTERS  *Registers
  )
{
  ASSERT (Vmcb != NULL);
  ASSERT (Registers != NULL);

  /*DEBUG ((
    DEBUG_VERBOSE,
    "Interception: %a 0x%lx 0x%lx\n",
    InternalGetVmexitName (Vmcb->EXITCODE),
    Vmcb->EXITINFO1,
    Vmcb->EXITINFO2
    ));*/

  Vmcb->EVENTINJ.Uint64 = 0;
  //
  // For forward compatibility, if the hypervisor has not modified the VMCB,
  // the hypervisor may write FFFF_FFFFh to the VMCB Clean Field to indicate
  // that it has not changed any VMCB contents other than the fields described
  // below as explicitly uncached.
  //
  Vmcb->VmcbCleanBits.Uint32 = MAX_UINT32;

  switch (Vmcb->EXITCODE) {
    case VMEXIT_EXCP_UD:
    {
      AmdIntelEmuInternalExceptionUd (Vmcb, Registers);
      break;
    }

    case VMEXIT_IRET:
    {
      AmdIntelEmuInternalExceptionIret (Vmcb);
      break;
    }
    
    case VMEXIT_EXCP_DB:
    {
      AmdIntelEmuInternalExceptionDebug (Vmcb);
      break;
    }

    case VMEXIT_NPF:
    {
      AmdIntelEmuInternalExceptionNpf (Vmcb);
      break;
    }

    case VMEXIT_CPUID:
    {
      AmdEmuInterceptCpuid (Vmcb, Registers);
      InternalRaiseRipNonJmp (Vmcb);
      break;
    }

    case VMEXIT_MSR:
    {
      if (Vmcb->EXITINFO1 == 0) {
        AmdIntelEmuInternalInterceptRdmsr (Vmcb, Registers);
      } else {
        ASSERT (Vmcb->EXITINFO1 == 1);
        AmdIntelEmuInternalInterceptWrmsr (Vmcb, Registers);
      }

      InternalRaiseRipNonJmp (Vmcb);

      break;
    }

    case VMEXIT_VMRUN:
    {
      AmdIntelEmuInternalInjectUd (Vmcb);
      break;
    }

    default:
    {
      DEBUG ((DEBUG_ERROR, "Unhandled VMEXIT!\n"));
      ASSERT (FALSE);
      break;
    }
  }

  InternalHandleEvents (Vmcb);

  if (Vmcb->EVENTINJ.Bits.V != 0) {
    DEBUG ((DEBUG_VERBOSE, "EVENTINJ: 0x%lx\n", Vmcb->EVENTINJ.Uint64));
  }

  InternalVmcbSanityCheck (Vmcb);

  //DEBUG ((DEBUG_VERBOSE, "Returning...\n"));
  //
  // Return Vmcb so it is loaded into rax when returning to the vmrun loop.
  //
  return Vmcb;
}

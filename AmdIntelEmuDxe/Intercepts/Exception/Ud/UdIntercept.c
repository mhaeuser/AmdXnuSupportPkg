#include <PiDxe.h>

#include <Library/BaseLib.h>
#include <Library/DebugLib.h>

#include "../../../AmdIntelEmu.h"

VOID
AmdIntelEmuInternalUdSysenter (
  IN OUT AMD_INTEL_EMU_THREAD_CONTEXT  *ThreadContext,
  IN     CONST hde64s                  *Instruction
  );

VOID
AmdIntelEmuInternalUdSysexit (
  IN OUT AMD_INTEL_EMU_THREAD_CONTEXT  *ThreadContext,
  IN     CONST hde64s                  *Instruction
  );

VOID
AmdIntelEmuInternalInjectUd (
  IN OUT AMD_VMCB_CONTROL_AREA  *Vmcb
  )
{
  ASSERT (Vmcb != NULL);
  //
  // Even injections are not cached.
  //
  Vmcb->EVENTINJ.Bits.VECTOR = EXCEPT_IA32_INVALID_OPCODE;
  Vmcb->EVENTINJ.Bits.TYPE   = AmdVmcbException;
  Vmcb->EVENTINJ.Bits.V      = 1;
  Vmcb->EVENTINJ.Bits.EV     = 0;
}

VOID
AmdIntelEmuInternalInjectGp (
  IN OUT AMD_VMCB_CONTROL_AREA  *Vmcb,
  IN     UINT8                  ErrorCode
  )
{
  ASSERT (Vmcb != NULL);
  //
  // Even injections are not cached.
  //
  Vmcb->EVENTINJ.Bits.VECTOR    = EXCEPT_IA32_GP_FAULT;
  Vmcb->EVENTINJ.Bits.TYPE      = AmdVmcbException;
  Vmcb->EVENTINJ.Bits.V         = 1;
  Vmcb->EVENTINJ.Bits.EV        = 1;
  Vmcb->EVENTINJ.Bits.ERRORCODE = ErrorCode;
}

VOID
AmdIntelEmuInternalExceptionDb (
  IN OUT AMD_INTEL_EMU_THREAD_CONTEXT  *ThreadContext
  )
{
  hde64s Instruction;

  ASSERT (ThreadContext != NULL);

  AmdIntelEmuInternalGetRipInstruction (ThreadContext->Vmcb, &Instruction);
  if ((Instruction.flags & F_ERROR) == 0) {
    if (Instruction.opcode == 0x0F) {
      switch (Instruction.opcode2) {
        case 34:
        {
          AmdIntelEmuInternalUdSysenter (ThreadContext, &Instruction);
          return;
        }

        case 35:
        {
          AmdIntelEmuInternalUdSysexit (ThreadContext, &Instruction);
          return;
        }

        default:
        {
          break;
        }
      }
    }
  }
  //
  // Transparently pass-through the #UD if the opcode is not emulated or cannot
  // be parsed.
  //
  AmdIntelEmuInternalInjectUd (ThreadContext->Vmcb);
}

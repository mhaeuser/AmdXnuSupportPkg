#ifndef AMD_INTEL_EMU_RUNTIME_H_
#define AMD_INTEL_EMU_RUNTIME_H_

#include "../AmdIntelEmu.h"
#include "hde/hde64.h"

VOID
AmdIntelEmuInternalSingleStepRip (
  IN OUT AMD_INTEL_EMU_THREAD_CONTEXT      *ThreadContext,
  IN     AMD_INTEL_EMU_SINGLE_STEP_RESUME  ResumeHandler,
  IN     UINTN                             ResumeContext
  );

UINT64
AmdIntelEmuInternalReadMsrValue64 (
  IN CONST UINT64                   *Rax,
  IN CONST AMD_INTEL_EMU_REGISTERS  *Registers
  );

VOID
AmdIntelEmuInternalWriteMsrValue64 (
  IN OUT UINT64                   *Rax,
  IN OUT AMD_INTEL_EMU_REGISTERS  *Registers,
  IN     UINT64                   Value
  );

VOID
AmdIntelEmuInternalGetRipInstruction (
  IN  CONST AMD_VMCB_SAVE_STATE_AREA_NON_ES  *SaveState,
  OUT hde64s                                 *Instruction
  );

VOID
AmdIntelEmuInternalInjectUd (
  IN OUT AMD_VMCB_CONTROL_AREA  *Vmcb
  );

VOID
AmdIntelEmuInternalInjectDf (
  IN OUT AMD_VMCB_CONTROL_AREA  *Vmcb
  );

VOID
AmdIntelEmuInternalInjectGp (
  IN OUT AMD_VMCB_CONTROL_AREA  *Vmcb,
  IN     UINT8                  ErrorCode
  );

AMD_INTEL_EMU_THREAD_CONTEXT *
AmdIntelEmuInternalGetThreadContext (
  IN CONST AMD_VMCB_CONTROL_AREA  *Vmcb
  );

VOID
AmdIntelEmuInternalQueueEvent (
  IN OUT AMD_VMCB_CONTROL_AREA  *Vmcb,
  IN     CONST AMD_VMCB_EVENT   *Event
  );

VOID
AmdIntelEmuInternalMmioLapicSetPage (
  IN VOID  *Page
  );

AMD_INTEL_EMU_GET_MMIO_PAGE
AmdIntelEmuInternalGetMmioHandler (
  IN UINT64  Address
  );

/**
  Read from a local APIC register.

  This function reads from a local APIC register either in xAPIC or x2APIC mode.
  It is required that in xAPIC mode wider registers (64-bit or 256-bit) must be
  accessed using multiple 32-bit loads or stores, so this function only performs
  32-bit read.

  @param  MmioOffset  The MMIO offset of the local APIC register in xAPIC mode.
                      It must be 16-byte aligned.

  @return 32-bit      Value read from the register.

**/
UINT32
AmdIntelEmuReadLocalApicReg (
  IN UINTN  MmioOffset
  );

extern BOOLEAN mAmdIntelEmuInternalNrip;
extern BOOLEAN mAmdIntelEmuInternalNp;

extern CONST AMD_INTEL_EMU_MSR_INTERCEPT_INFO mAmdIntelEmuInternalMsrIntercepts[];
extern CONST UINTN                            mAmdIntelEmuInternalNumMsrIntercepts;

#endif // AMD_INTEL_EMU_RUNTIME_H_

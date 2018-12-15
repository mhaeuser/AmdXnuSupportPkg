#include <Base.h>

#include <Register/Msr.h>

#include <Library/BaseLib.h>
#include <Library/DebugLib.h>

#include "../../AmdIntelEmuRuntime.h"

VOID
AmdIntelEmuInternalRdmsrPat (
  IN OUT AMD_VMCB_SAVE_STATE_AREA_NON_ES  *SaveState,
  IN OUT AMD_INTEL_EMU_REGISTERS          *Registers
  );

VOID
AmdIntelEmuInternalWrmsrPat (
  IN OUT AMD_VMCB_SAVE_STATE_AREA_NON_ES  *SaveState,
  IN OUT AMD_INTEL_EMU_REGISTERS          *Registers
  );

VOID
AmdIntelEmuInternalRdmsrMiscEnable (
  IN OUT AMD_VMCB_SAVE_STATE_AREA_NON_ES  *SaveState,
  IN OUT AMD_INTEL_EMU_REGISTERS          *Registers
  );

VOID
AmdIntelEmuInternalWrmsrMiscEnable (
  IN OUT AMD_VMCB_SAVE_STATE_AREA_NON_ES  *SaveState,
  IN OUT AMD_INTEL_EMU_REGISTERS          *Registers
  );

VOID
AmdIntelEmuInternalRdmsrPlatformId (
  IN OUT AMD_VMCB_SAVE_STATE_AREA_NON_ES  *SaveState,
  IN OUT AMD_INTEL_EMU_REGISTERS          *Registers
  );

VOID
AmdIntelEmuInternalWrmsrPlatformId (
  IN OUT AMD_VMCB_SAVE_STATE_AREA_NON_ES  *SaveState,
  IN OUT AMD_INTEL_EMU_REGISTERS          *Registers
  );

VOID
AmdIntelEmuInternalRdmsrBiosSignId (
  IN OUT AMD_VMCB_SAVE_STATE_AREA_NON_ES  *SaveState,
  IN OUT AMD_INTEL_EMU_REGISTERS          *Registers
  );

VOID
AmdIntelEmuInternalWrmsrBiosSignId (
  IN OUT AMD_VMCB_SAVE_STATE_AREA_NON_ES  *SaveState,
  IN OUT AMD_INTEL_EMU_REGISTERS          *Registers
  );

VOID
AmdIntelEmuInternalRdmsrCoreThreadCount (
  IN OUT AMD_VMCB_SAVE_STATE_AREA_NON_ES  *SaveState,
  IN OUT AMD_INTEL_EMU_REGISTERS          *Registers
  );

VOID
AmdIntelEmuInternalWrmsrCoreThreadCount (
  IN OUT AMD_VMCB_SAVE_STATE_AREA_NON_ES  *SaveState,
  IN OUT AMD_INTEL_EMU_REGISTERS          *Registers
  );

VOID
AmdIntelEmuInternalRdmsrX2apicVersion (
  IN OUT AMD_VMCB_SAVE_STATE_AREA_NON_ES  *SaveState,
  IN OUT AMD_INTEL_EMU_REGISTERS          *Registers
  );

VOID
AmdIntelEmuInternalWrmsrX2apicVersion (
  IN OUT AMD_VMCB_SAVE_STATE_AREA_NON_ES  *SaveState,
  IN OUT AMD_INTEL_EMU_REGISTERS          *Registers
  );

GLOBAL_REMOVE_IF_UNREFERENCED
CONST AMD_INTEL_EMU_MSR_INTERCEPT_INFO mAmdIntelEmuInternalMsrIntercepts[] = {
  {
    MSR_IA32_PAT,
    AmdIntelEmuInternalRdmsrPat,
    AmdIntelEmuInternalWrmsrPat
  },
  {
    MSR_IA32_MISC_ENABLE,
    AmdIntelEmuInternalRdmsrMiscEnable,
    AmdIntelEmuInternalWrmsrMiscEnable
  },
  {
    MSR_IA32_PLATFORM_ID,
    AmdIntelEmuInternalRdmsrPlatformId,
    AmdIntelEmuInternalWrmsrPlatformId
  },
  {
    MSR_IA32_BIOS_SIGN_ID,
    AmdIntelEmuInternalRdmsrBiosSignId,
    AmdIntelEmuInternalWrmsrBiosSignId
  },
  {
    MSR_HASWELL_E_CORE_THREAD_COUNT,
    AmdIntelEmuInternalRdmsrCoreThreadCount,
    AmdIntelEmuInternalWrmsrCoreThreadCount
  },
  {
    MSR_IA32_X2APIC_VERSION,
    AmdIntelEmuInternalRdmsrX2apicVersion,
    AmdIntelEmuInternalWrmsrX2apicVersion
  }
};

GLOBAL_REMOVE_IF_UNREFERENCED
CONST UINTN mAmdIntelEmuInternalNumMsrIntercepts =
  ARRAY_SIZE (mAmdIntelEmuInternalMsrIntercepts);

VOID
AmdIntelEmuInternalInterceptRdmsr (
  IN OUT AMD_VMCB_SAVE_STATE_AREA_NON_ES  *SaveState,
  IN OUT AMD_INTEL_EMU_REGISTERS          *Registers
  )
{
  UINT32                                 MsrIndex;
  UINTN                                  Index;
  CONST AMD_INTEL_EMU_MSR_INTERCEPT_INFO *MsrIntercept;

  MsrIndex = (UINT32)Registers->Rcx;

  for (Index = 0; Index < mAmdIntelEmuInternalNumMsrIntercepts; ++Index) {
    MsrIntercept = &mAmdIntelEmuInternalMsrIntercepts[Index];
    if (MsrIntercept->MsrIndex == MsrIndex) {
      if (MsrIntercept->Read != NULL) {
        MsrIntercept->Read (SaveState, Registers);
        return;
      }

      break;
    }
  }

  AmdIntelEmuInternalWriteMsrValue64 (
    &SaveState->RAX,
    Registers,
    AsmReadMsr64 (MsrIndex)
    );
}

VOID
AmdIntelEmuInternalInterceptWrmsr (
  IN OUT AMD_VMCB_SAVE_STATE_AREA_NON_ES  *SaveState,
  IN OUT AMD_INTEL_EMU_REGISTERS          *Registers
  )
{
  UINT32                                 MsrIndex;
  UINTN                                  Index;
  CONST AMD_INTEL_EMU_MSR_INTERCEPT_INFO *MsrIntercept;

  MsrIndex = (UINT32)Registers->Rcx;

  for (Index = 0; Index < mAmdIntelEmuInternalNumMsrIntercepts; ++Index) {
    MsrIntercept = &mAmdIntelEmuInternalMsrIntercepts[Index];
    if (MsrIntercept->MsrIndex == MsrIndex) {
      if (MsrIntercept->Write != NULL) {
        MsrIntercept->Write (SaveState, Registers);
        return;
      }

      break;
    }
  }

  AsmWriteMsr64 (
    MsrIndex,
    AmdIntelEmuInternalReadMsrValue64 (&SaveState->RAX, Registers)
    );
}

UINT64
AmdIntelEmuInternalReadMsrValue64 (
  IN CONST UINT64                   *Rax,
  IN CONST AMD_INTEL_EMU_REGISTERS  *Registers
  )
{
  UINT64 Value;

  ASSERT (Rax != NULL);
  ASSERT (Registers != NULL);

  Value  = (BitFieldRead64 (*Rax,           0, 31));
  Value |= (BitFieldRead64 (Registers->Rdx, 0, 31) << 32U);

  return Value;
}

VOID
AmdIntelEmuInternalWriteMsrValue64 (
  IN OUT UINT64                   *Rax,
  IN OUT AMD_INTEL_EMU_REGISTERS  *Registers,
  IN     UINT64                   Value
  )
{
  ASSERT (Rax != NULL);
  ASSERT (Registers != NULL);

  *Rax           = BitFieldRead64 (Value, 0, 31);
  Registers->Rdx = BitFieldRead64 (Value, 32, 63);
}

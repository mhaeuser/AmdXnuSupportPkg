#include <Base.h>

#include <Register/Msr.h>

#include <Library/BaseLib.h>
#include <Library/DebugLib.h>

#include "../../AmdIntelEmu.h"

typedef
VOID
(*INTERNAL_MSR_INTERCEPT) (
  IN OUT AMD_VMCB_SAVE_STATE_AREA_NON_ES  *SaveState,
  IN OUT AMD_EMU_REGISTERS                *Registers
  );

VOID
AmdIntelEmuInternalWrmsrPat (
  IN OUT AMD_VMCB_SAVE_STATE_AREA_NON_ES  *SaveState,
  IN OUT AMD_EMU_REGISTERS                *Registers
  );

VOID
AmdIntelEmuInternalRdmsrMiscEnable (
  IN OUT AMD_VMCB_SAVE_STATE_AREA_NON_ES  *SaveState,
  IN OUT AMD_EMU_REGISTERS                *Registers
  );

VOID
AmdIntelEmuInternalWrmsrMiscEnable (
  IN OUT AMD_VMCB_SAVE_STATE_AREA_NON_ES  *SaveState,
  IN OUT AMD_EMU_REGISTERS                *Registers
  );

VOID
AmdIntelEmuInternalRdmsrPlatformId (
  IN OUT AMD_VMCB_SAVE_STATE_AREA_NON_ES  *SaveState,
  IN OUT AMD_EMU_REGISTERS                *Registers
  );

VOID
AmdIntelEmuInternalWrmsrPlatformId (
  IN OUT AMD_VMCB_SAVE_STATE_AREA_NON_ES  *SaveState,
  IN OUT AMD_EMU_REGISTERS                *Registers
  );

VOID
AmdIntelEmuInternalRdmsrBiosSignId (
  IN OUT AMD_VMCB_SAVE_STATE_AREA_NON_ES  *SaveState,
  IN OUT AMD_EMU_REGISTERS                *Registers
  );

VOID
AmdIntelEmuInternalWrmsrBiosSignId (
  IN OUT AMD_VMCB_SAVE_STATE_AREA_NON_ES  *SaveState,
  IN OUT AMD_EMU_REGISTERS                *Registers
  );

VOID
AmdIntelEmuInternalRdmsrCoreThreadCount (
  IN OUT AMD_VMCB_SAVE_STATE_AREA_NON_ES  *SaveState,
  IN OUT AMD_EMU_REGISTERS                *Registers
  );

VOID
AmdIntelEmuInternalWrmsrCoreThreadCount (
  IN OUT AMD_VMCB_SAVE_STATE_AREA_NON_ES  *SaveState,
  IN OUT AMD_EMU_REGISTERS                *Registers
  );

VOID
AmdIntelEmuInternalRdmsrX2apicVersion (
  IN OUT AMD_VMCB_SAVE_STATE_AREA_NON_ES  *SaveState,
  IN OUT AMD_EMU_REGISTERS                *Registers
  );

VOID
AmdIntelEmuInternalWrmsrX2apicVersion (
  IN OUT AMD_VMCB_SAVE_STATE_AREA_NON_ES  *SaveState,
  IN OUT AMD_EMU_REGISTERS                *Registers
  );

typedef struct {
  UINT32                 MsrIndex;
  INTERNAL_MSR_INTERCEPT Read;
  INTERNAL_MSR_INTERCEPT Write;
} INTERNAL_MSR_INTERCEPT_INFO;

STATIC CONST INTERNAL_MSR_INTERCEPT_INFO mMsrInterceptMap[] = {
  {
    MSR_IA32_PAT,
    NULL,
    (INTERNAL_MSR_INTERCEPT)AmdIntelEmuInternalWrmsrPat
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

VOID
AmdIntelEmuInternalInitMsrPm (
  IN OUT AMD_VMCB_CONTROL_AREA  *Vmcb
  )
{
  UINT8                             *MsrPm;
  UINTN                             Index;
  CONST INTERNAL_MSR_INTERCEPT_INFO *MsrIntercept;
  UINT32                            MsrIndex;
  UINT32                            MsrBit;
  UINT32                            MsrOffset;

  ASSERT (Vmcb != NULL);

  MsrPm = (UINT8 *)(UINTN)Vmcb->MSRPM_BASE_PA;
  ASSERT (MsrPm != NULL);

  for (Index = 0; Index < ARRAY_SIZE (mMsrInterceptMap); ++Index) {
    MsrIntercept = &mMsrInterceptMap[Index];

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
      Vmcb->InterceptMsrProt = 1;
      continue;
    }

    MsrBit    = (MsrIndex * 2);
    MsrOffset = (MsrBit / 8);
    MsrBit    = (MsrBit % 8);

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

VOID
AmdIntelEmuInternalInterceptRdmsr (
  IN OUT AMD_VMCB_SAVE_STATE_AREA_NON_ES  *SaveState,
  IN OUT AMD_EMU_REGISTERS                *Registers
  )
{
  UINT32                            MsrIndex;
  UINTN                             Index;
  CONST INTERNAL_MSR_INTERCEPT_INFO *MsrIntercept;
  //
  // The rdmsr and wrmsr opcodes are both two bytes long.
  //
  SaveState->RIP += 2;

  MsrIndex = BitFieldRead32 (Registers->Rcx, 0, 31);

  for (Index = 0; Index < ARRAY_SIZE (mMsrInterceptMap); ++Index) {
    MsrIntercept = &mMsrInterceptMap[Index];
    if (MsrIntercept->MsrIndex == MsrIndex) {
      if (MsrIntercept->Read != NULL) {
        MsrIntercept->Read (SaveState, Registers);
      }

      return;
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
  IN OUT AMD_EMU_REGISTERS                *Registers
  )
{
  UINT32                            MsrIndex;
  UINTN                             Index;
  CONST INTERNAL_MSR_INTERCEPT_INFO *MsrIntercept;

  MsrIndex = BitFieldRead32 (Registers->Rcx, 0, 31);

  for (Index = 0; Index < ARRAY_SIZE (mMsrInterceptMap); ++Index) {
    MsrIntercept = &mMsrInterceptMap[Index];
    if (MsrIntercept->MsrIndex == MsrIndex) {
      if (MsrIntercept->Write != NULL) {
        MsrIntercept->Write (SaveState, Registers);
      }

      return;
    }
  }

  AsmWriteMsr64 (
    MsrIndex,
    AmdIntelEmuInternalReadMsrValue64 (&SaveState->RAX, Registers)
    );
}

UINT64
AmdIntelEmuInternalReadMsrValue64 (
  IN CONST UINT64             *Rax,
  IN CONST AMD_EMU_REGISTERS  *Registers
  )
{
  UINT64 Value;

  ASSERT (Rax != NULL);
  ASSERT (Registers != NULL);

  Value  = *Rax;
  Value |= (Registers->Rdx >> 31U);

  return Value;
}

VOID
AmdIntelEmuInternalWriteMsrValue64 (
  IN OUT UINT64             *Rax,
  IN OUT AMD_EMU_REGISTERS  *Registers,
  IN     UINT64             Value
  )
{
  ASSERT (Rax != NULL);
  ASSERT (Registers != NULL);

  *Rax           = (Value & 0xFFFFFFFFU);
  Registers->Rdx = (Value << 31U);
}

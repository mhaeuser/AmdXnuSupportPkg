#include <Base.h>

#include <Register/ArchitecturalMsr.h>

#include <Library/BaseLib.h>
#include <Library/DebugLib.h>

#include "../../AmdIntelEmu.h"

VOID
AmdIntelEmuInternalRdmsrMiscEnable (
  IN OUT UINT64             *Rax,
  IN OUT AMD_EMU_REGISTERS  *Registers
  )
{
  MSR_IA32_MISC_ENABLE_REGISTER       MiscEnableMsr;

  MSR_AMD_CPUID_EXT_FEATURES_REGISTER ExtFeaturesMsr;
  MSR_AMD_HWCR_REGISTER               HwcrMsr;

  ASSERT (Rax != NULL);
  ASSERT (Registers != NULL);

  MiscEnableMsr.Uint64 = 0;

  ExtFeaturesMsr.Uint64 = AsmReadMsr64 (MSR_AMD_CPUID_EXT_FEATURES);
  MiscEnableMsr.Bits.XD = ExtFeaturesMsr.Bits.NX;

  HwcrMsr.Uint64             = AsmReadMsr64 (MSR_AMD_HWCR);
  MiscEnableMsr.Bits.MONITOR = (HwcrMsr.Bits.MonWaitDis ? 0 : 1);
  //
  // TODO: Implement EIST via PowerNOW!
  //
  AmdIntelEmuInternalWriteMsrValue64 (Rax, Registers, MiscEnableMsr.Uint64);
}

VOID
AmdIntelEmuInternalWrmsrMiscEnable (
  IN OUT UINT64             *Rax,
  IN OUT AMD_EMU_REGISTERS  *Registers
  )
{
  MSR_IA32_MISC_ENABLE_REGISTER       MiscEnableMsr;

  MSR_AMD_CPUID_EXT_FEATURES_REGISTER ExtFeaturesMsr;
  MSR_AMD_HWCR_REGISTER               HwcrMsr;

  ASSERT (Rax != NULL);
  ASSERT (Registers != NULL);

  MiscEnableMsr.Uint64 = AmdIntelEmuInternalReadMsrValue64 (Rax, Registers);

  ExtFeaturesMsr.Uint64  = AsmReadMsr64 (MSR_AMD_CPUID_EXT_FEATURES);
  ExtFeaturesMsr.Bits.NX = MiscEnableMsr.Bits.XD;
  AsmWriteMsr64 (MSR_AMD_CPUID_EXT_FEATURES, ExtFeaturesMsr.Uint64);

  HwcrMsr.Uint64          = AsmReadMsr64 (MSR_AMD_HWCR);
  HwcrMsr.Bits.MonWaitDis = (MiscEnableMsr.Bits.MONITOR ? 0 : 1);
  AsmWriteMsr64 (MSR_AMD_HWCR, HwcrMsr.Uint64);
  //
  // TODO: Implement EIST via PowerNOW!
  //
}

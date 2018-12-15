#include <Base.h>

#include <Register/Msr.h>

#include <Library/BaseLib.h>
#include <Library/DebugLib.h>

STATIC
VOID
InternalTestMsr (
  IN UINT32  Index
  )
{
  UINT64 OldValue;
  UINT64 NewValue;

  OldValue = AsmReadMsr64 (Index);
  AsmWriteMsr64 (Index, OldValue);
  NewValue = AsmReadMsr64 (Index);

  if (OldValue != NewValue) {
    DEBUG ((
      DEBUG_VERBOSE,
      "Index: %u, OldValue: %lu, NewValue: %lu.\n",
      Index,
      OldValue,
      NewValue
      ));
    ASSERT (FALSE);
  }
}

VOID
AmdIntelEmuRunTestIntercepts (
  VOID
  )
{
  UINT32 Value1;
  UINT32 Value2;
  UINT32 Index;

  DEBUG ((DEBUG_VERBOSE, "CPUID Leaf 0.\n"));
  AsmCpuid (0, NULL, NULL, NULL, NULL);
  DEBUG ((DEBUG_VERBOSE, "CPUID Leaf 1.\n"));
  AsmCpuid (1, NULL, NULL, NULL, NULL);
  DEBUG ((DEBUG_VERBOSE, "CPUID Leaf 2.\n"));
  AsmCpuid (2, NULL, NULL, NULL, NULL);
  DEBUG ((DEBUG_VERBOSE, "CPUID Leaf 4.\n"));
  AsmCpuid (4, NULL, NULL, NULL, NULL);
  DEBUG ((DEBUG_VERBOSE, "CPUID Leaf 40000000.\n"));
  AsmCpuid (0x40000000, NULL, NULL, NULL, NULL);
  DEBUG ((DEBUG_VERBOSE, "CPUID Leaf 40000010.\n"));
  AsmCpuid (0x40000010, NULL, NULL, NULL, NULL);
  //
  // Avoid the verification as the write interception alters the data.
  //
  DEBUG ((DEBUG_VERBOSE, "MSR_IA32_PAT.\n"));
  AsmWriteMsr64 (MSR_IA32_PAT, AsmReadMsr64 (MSR_IA32_PAT));
  DEBUG ((DEBUG_VERBOSE, "MSR_IA32_MISC_ENABLE.\n"));
  InternalTestMsr (MSR_IA32_MISC_ENABLE);
  DEBUG ((DEBUG_VERBOSE, "MSR_IA32_PLATFORM_ID.\n"));
  InternalTestMsr (MSR_IA32_PLATFORM_ID);
  DEBUG ((DEBUG_VERBOSE, "MSR_IA32_BIOS_SIGN_ID.\n"));
  InternalTestMsr (MSR_IA32_BIOS_SIGN_ID);
  DEBUG ((DEBUG_VERBOSE, "MSR_HASWELL_E_CORE_THREAD_COUNT.\n"));
  InternalTestMsr (MSR_HASWELL_E_CORE_THREAD_COUNT);
  DEBUG ((DEBUG_VERBOSE, "MSR_IA32_X2APIC_VERSION.\n"));
  InternalTestMsr (MSR_IA32_X2APIC_VERSION);

  DEBUG ((DEBUG_VERBOSE, "LAPIC MMIO test.\n"));

  Value1 = *(volatile UINT32 *)(UINTN)0xFEE00030U;

  DEBUG ((DEBUG_VERBOSE, "Value to test against: 0x%x... ", Value1));

  for (Index = 0; Index < 50; ++Index) {
    Value2 = *(volatile UINT32 *)(UINTN)0xFEE00030U;
    if (Value1 != Value2) {
      DEBUG ((DEBUG_VERBOSE, "ERROR!!!\n"));
      return;
    }
  }

  DEBUG ((DEBUG_VERBOSE, "Done.\n"));

  DEBUG ((DEBUG_VERBOSE, "Extra VMM CPUID verification... "));

  for (Index = 0x40000000; Index <= 0x40000010U; ++Index) {
    AsmCpuid (Index, NULL, NULL, NULL, NULL);
  }

  DEBUG ((DEBUG_VERBOSE, "Done.\n"));
}

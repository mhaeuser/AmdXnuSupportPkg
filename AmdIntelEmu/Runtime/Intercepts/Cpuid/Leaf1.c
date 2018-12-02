#include <Base.h>

#include <Register/Cpuid.h>

#include <Library/BaseLib.h>
#include <Library/DebugLib.h>

VOID
AmdIntelEmuInternalCpuidLeaf1 (
  OUT UINT32  *Eax,
  OUT UINT32  *Ebx,
  OUT UINT32  *Ecx,
  OUT UINT32  *Edx
  )
{
  CPUID_VERSION_INFO_EAX VersionEax;

  ASSERT (Eax != NULL);
  ASSERT (Ebx != NULL);
  ASSERT (Ecx != NULL);
  ASSERT (Edx != NULL);
  //
  // Return Haswell S C0 information.
  //
  VersionEax.Uint32                = 0;
  VersionEax.Bits.SteppingId       = 0x03;
  VersionEax.Bits.Model            = 0x0C;
  VersionEax.Bits.FamilyId         = 0x06;
  VersionEax.Bits.ProcessorType    = CPUID_VERSION_INFO_EAX_PROCESSOR_TYPE_ORIGINAL_OEM_PROCESSOR;
  VersionEax.Bits.ExtendedModelId  = 0x03;
  VersionEax.Bits.ExtendedFamilyId = 0x00;
  *Eax = VersionEax.Uint32;
  //
  // Ebx to Edx are compatible, Intel-only fields will return 0 as intended.
  //
  AsmCpuid (CPUID_VERSION_INFO, NULL, Ebx, Ecx, Edx);
  //
  // This bit signals hypervisor presence.
  //
  *Ecx |= BIT31;
}

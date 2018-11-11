#include <Base.h>

#include <Register/Cpuid.h>

#include <Library/BaseLib.h>
#include <Library/DebugLib.h>

VOID
AmdIntelEmuInternalCpuidLeaf7 (
  OUT UINT32  *Eax,
  OUT UINT32  *Ebx,
  OUT UINT32  *Ecx,
  OUT UINT32  *Edx
  )
{
  CPUID_STRUCTURED_EXTENDED_FEATURE_FLAGS_EBX ExtFeatureFlagsEbx;

  ASSERT (Eax != NULL);
  ASSERT (Ebx != NULL);
  ASSERT (Ecx != NULL);
  ASSERT (Edx != NULL);

  AsmCpuid (
    CPUID_STRUCTURED_EXTENDED_FEATURE_FLAGS,
    Eax,
    &ExtFeatureFlagsEbx.Uint32,
    Ecx,
    Edx
    );
  //
  // Show SMAP as unsupported to prevent early OS crashes.
  //
  ExtFeatureFlagsEbx.Bits.SMAP = 0;
  *Ebx = ExtFeatureFlagsEbx.Uint32;
}

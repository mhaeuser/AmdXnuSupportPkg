#include <Base.h>

#include <Library/BaseLib.h>
#include <Library/DebugLib.h>

VOID
AmdIntelEmuInternalCpuidLeaf4 (
  IN  UINT32  SubIndex,
  OUT UINT32  *Eax,
  OUT UINT32  *Ebx,
  OUT UINT32  *Ecx,
  OUT UINT32  *Edx
  )
{
  ASSERT (Eax != NULL);
  ASSERT (Ebx != NULL);
  ASSERT (Ecx != NULL);
  ASSERT (Edx != NULL);
  //
  // Intel Leaf 4 and AMD Leaf 0x8000001D are compatible.
  //
  AsmCpuidEx (0x8000001D, SubIndex, Eax, Ebx, Ecx, Edx);
}

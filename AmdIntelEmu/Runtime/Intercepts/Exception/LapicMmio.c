#include <Base.h>

#include <Register/LocalApic.h>

#include <Library/BaseLib.h>
#include <Library/DebugLib.h>

#include "../../AmdIntelEmuRuntime.h"

STATIC CONST VOID *mLapicVersionPage = NULL;

VOID
AmdIntelEmuInternalMmioLapicSetPage (
  IN VOID  *Page
  )
{
  ASSERT (Page != NULL);
  ASSERT (((UINTN)Page % SIZE_4KB) == 0);

  *(UINT32 *)((UINTN)Page + XAPIC_VERSION_OFFSET) = BitFieldWrite32 (
                                                      AmdIntelEmuReadLocalApicReg (
                                                        XAPIC_VERSION_OFFSET
                                                        ),
                                                      0,
                                                      7,
                                                      0x14
                                                      );

  mLapicVersionPage = Page;
}

UINT64
AmdIntelEmuInternalMmioLapic (
  IN UINT64  BaseAddress,
  IN UINT64  FaultAddress
  )
{
  ASSERT (mLapicVersionPage != NULL);

  if ((FaultAddress >= (BaseAddress + XAPIC_VERSION_OFFSET))
   && (FaultAddress <  (BaseAddress + XAPIC_VERSION_OFFSET + 4))) {
    return (UINT64)(UINTN)mLapicVersionPage;
  }

  return FaultAddress;
}

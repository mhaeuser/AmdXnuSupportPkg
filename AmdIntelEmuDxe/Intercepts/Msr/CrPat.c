#include <Base.h>

#include <Library/BaseLib.h>
#include <Library/DebugLib.h>

#include "../../AmdIntelEmu.h"

#define MSR_IA32_CR_PAT  0x0277U

#define PATENTRY(N, Type) ((Type) << ((N) * 8U))
#define PAT_UC       0x00ULL
#define PAT_WC       0x01ULL
#define PAT_WT       0x04ULL
#define PAT_WP       0x05ULL
#define PAT_WB       0x06ULL
#define PAT_UCMINUS  0x07ULL

#define AMD_PAT                                         \
  (                                                     \
    PATENTRY (0U, PAT_WB)      | PATENTRY (1U, PAT_WC)  \
  | PATENTRY (2U, PAT_UCMINUS) | PATENTRY (3U, PAT_UC)  \
  | PATENTRY (4U, PAT_WB)      | PATENTRY (5U, PAT_WC)  \
  | PATENTRY (6U, PAT_UCMINUS) | PATENTRY (7U, PAT_UC)  \
  )

VOID
AmdEmuMsrUpdatePat (
  IN OUT UINT64             *Rax,
  IN OUT AMD_EMU_REGISTERS  *Registers
  )
{
  UINT64 CurrentPat;

  ASSERT (Rax != NULL);
  ASSERT (Registers != NULL);

  CurrentPat = AsmReadMsr64 (MSR_IA32_CR_PAT);
  if ((CurrentPat & (0x0FULL << 48U)) != (0x01ULL << 48U)) {

  }
}

VOID
AmdEmuMsrReadPat (
  IN OUT UINT64             *Rax,
  IN OUT AMD_EMU_REGISTERS  *Registers
  )
{
  UINT64 CurrentPat;

  ASSERT (Rax != NULL);
  ASSERT (Registers != NULL);

  CurrentPat = AsmReadMsr64 (MSR_IA32_CR_PAT);
  if (CurrentPat != AMD_PAT) {

  }
}

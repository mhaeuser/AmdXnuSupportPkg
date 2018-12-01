#include <Base.h>

#include <Library/BaseLib.h>
#include <Library/DebugLib.h>

#define TLB_INST   0
#define TLB_DATA   1
#define TLB_SMALL  0
#define TLB_LARGE  1

///
/// Leaf 2 cache descriptor encodings.
///
typedef enum {
  _NULL_,   ///< NULL (empty) descriptor
  CACHE,    ///< Cache
  TLB,      ///< TLB
  STLB,     ///< Shared second-level unified TLB
  PREFETCH  ///< Prefetch size
} XNU_CPUID_LEAF2_DESC_TYPE;

typedef enum {
  NA,              ///< Not Applicable
  FULLY,           ///< Fully-associative  
  TRACE,           ///< Trace Cache (P4 only)
  INST,            ///< Instruction TLB
  DATA,            ///< Data TLB
  DATA0,           ///< Data TLB, 1st level
  DATA1,           ///< Data TLB, 2nd level
  L1,              ///< L1 (unified) cache
  L1_INST,         ///< L1 Instruction cache
  L1_DATA,         ///< L1 Data cache
  L2,              ///< L2 (unified) cache
  L3,              ///< L3 (unified) cache
  L2_2LINESECTOR,  ///< L2 (unified) cache with 2 lines per sector
  L3_2LINESECTOR,  ///< L3(unified) cache with 2 lines per sector
  SMALL,           ///< Small page TLB
  LARGE,           ///< Large page TLB
  BOTH             ///< Small and Large page TLB
} XNU_CPUID_LEAF2_QUALIFIER;

typedef struct {
  UINT8  Value;    ///< descriptor code
  UINT16 Type;     ///< cpuid_leaf2_desc_type_t
  UINT16 Level;    ///< level of cache/TLB hierachy
  UINT16 Ways;     ///< wayness of cache
  UINT16 Size;     ///< cachesize or TLB pagesize
  UINT16 Entries;  ///< number of TLB entries or linesize
} CPUID_CACHE_DESCRIPTOR;

STATIC CONST CPUID_CACHE_DESCRIPTOR mCacheDescriptorMap[] = {
  // TODO: populate the list with the map from the Intel SDM.
};

STATIC
UINT8
InternalCacheInfoToDescriptor (
  IN UINT16  Type,
  IN UINT16  Level,
  IN UINT16  Ways,
  IN UINT16  Size,
  IN UINT16  Entries
  )
{
  CONST CPUID_CACHE_DESCRIPTOR *Descriptor;

  UINTN                        Index;
  CONST CPUID_CACHE_DESCRIPTOR *Walker;

  Descriptor = NULL;

  for (Index = 0; Index < ARRAY_SIZE (mCacheDescriptorMap); ++Index) {
    Walker = &mCacheDescriptorMap[Index];
    if ((Walker->Type  == Type)
     && (Walker->Level == Level)
     && (Walker->Ways  == Ways)) {
      switch (Type) {
        case CACHE:
        {
          //
          // The number of cache lines is stable across AMD and Intel.
          // The size may vary, so we allow smaller ones.
          //
          if ((Walker->Size > Size)
           || (Walker->Entries != Entries)) {
            continue;
          }

          break;
        }

        case TLB:
        {
          //
          // The TLB sizes are stable across AMD and Intel.
          // The number of entries may vary, so we allow smaller ones.
          //
          if ((Walker->Size != Size)
           || (Walker->Entries > Entries)) {
            continue;
          }

          break;
        }

        default:
        {
          if ((Walker->Size != Size)
           || (Walker->Entries != Entries)) {
            continue;
          }

          break;
        }
      }

      if ((Walker->Size == Size) && (Walker->Entries == Entries)) {
        Descriptor = Walker;
        break;
      }

      if ((Descriptor == NULL)
       || (Walker->Size > Descriptor->Size)
       || (Walker->Entries > Descriptor->Entries)) {
        Descriptor = Walker;
      }
    }
  }

  if (Descriptor != NULL) {
    return Descriptor->Value;
  }

  return _NULL_;
}

VOID
AmdIntelEmuInternalCpuidLeaf2 (
  OUT UINT32  *Eax,
  OUT UINT32  *Ebx,
  OUT UINT32  *Ecx,
  OUT UINT32  *Edx
  )
{
  UINT32 CpuidEax;
  UINT32 CpuidEbx;
  UINT32 CpuidEcx;
  UINT32 CpuidEdx;

  UINT8  *Bytes;

  UINT8  L1DTlb2and4MAssoc;
  UINT8  L1ITlb2and4MSize;

  UINT8  L1ITlb2and4MAssoc;
  UINT8  L1DTlb2and4MSize;

  UINT8  L1DTlb4KAssoc;
  UINT8  L1DTlb4KSize;

  UINT8  L1ITlb4KAssoc;
  UINT8  L1ITlb4KSize;

  UINT8  L1DcSize;
  UINT8  L1DcAssoc;
  UINT8  L1DcLineSize;

  UINT8  L1IcSize;
  UINT8  L1IcAssoc;
  UINT8  L1IcLineSize;

  UINT8  L2DTlb2and4MAssoc;
  UINT16 L2DTlb2and4MSize;

  UINT8  L2ITlb2and4MAssoc;
  UINT16 L2ITlb2and4MSize;

  UINT8  L2DTlb4KAssoc;
  UINT16 L2DTlb4KSize;

  UINT8  L2ITlb4KAssoc;
  UINT16 L2ITlb4KSize;

  UINT16 L2Size;
  UINT8  L2Assoc;
  UINT8  L2LinesPerTag;
  UINT8  L2LineSize;

  UINT16 L3Size;
  UINT8  L3Assoc;
  UINT8  L3LinesPerTag;
  UINT8  L3LineSize;

  ASSERT (Eax != NULL);
  ASSERT (Ebx != NULL);
  ASSERT (Ecx != NULL);
  ASSERT (Edx != NULL);
  //
  // Retrieve the AMD CPU Cache data.
  //
  AsmCpuid (0x80000005, &CpuidEax, &CpuidEbx, &CpuidEcx, &CpuidEdx);

  L1DTlb2and4MAssoc = (UINT8)BitFieldRead32 (CpuidEax, 24, 31);
  L1ITlb2and4MSize  = (UINT8)BitFieldRead32 (CpuidEax, 0, 7);
  if (L1DTlb2and4MAssoc == 0xFF) {
    L1DTlb2and4MAssoc = FULLY;
  }

  L1ITlb2and4MAssoc = (UINT8)BitFieldRead32 (CpuidEax, 8, 15);
  L1DTlb2and4MSize  = (UINT8)BitFieldRead32 (CpuidEax, 16, 23);
  if (L1ITlb2and4MAssoc == 0xFF) {
    L1ITlb2and4MAssoc = FULLY;
  }

  L1DTlb4KAssoc = (UINT8)BitFieldRead32 (CpuidEbx, 24, 31);
  L1DTlb4KSize  = (UINT8)BitFieldRead32 (CpuidEbx, 16, 23);
  if (L1DTlb4KAssoc == 0xFF) {
    L1DTlb4KAssoc = FULLY;
  }

  L1ITlb4KAssoc = (UINT8)BitFieldRead32 (CpuidEbx, 8, 15);
  L1ITlb4KSize  = (UINT8)BitFieldRead32 (CpuidEbx, 0,  7);
  if (L1ITlb4KAssoc == 0xFF) {
    L1ITlb4KAssoc = FULLY;
  }

  L1DcSize     = (UINT8)BitFieldRead32 (CpuidEcx, 24, 31);
  L1DcAssoc    = (UINT8)BitFieldRead32 (CpuidEcx, 16, 23);
  L1DcLineSize = (UINT8)BitFieldRead32 (CpuidEcx, 0,  7);
  if (L1DcAssoc == 0xFF) {
    L1DcAssoc = FULLY;
  }
 
  L1IcSize     = (UINT8)BitFieldRead32 (CpuidEdx, 24, 31);
  L1IcAssoc    = (UINT8)BitFieldRead32 (CpuidEdx, 16, 23);
  L1IcLineSize = (UINT8)BitFieldRead32 (CpuidEdx, 0,  7);
  if (L1IcAssoc == 0xFF) {
    L1IcAssoc = FULLY;
  }
    
  AsmCpuid (0x80000006, &CpuidEax, &CpuidEbx, &CpuidEcx, &CpuidEdx);
  //
  // NOTE: The following TLBs do not have 0x0F defined as fully associative,
  //       however TLBs have a pattern of following the associated cache, which
  //       do.
  //
  L2DTlb2and4MAssoc = (UINT8)BitFieldRead32  (CpuidEax, 28, 31);
  L2DTlb2and4MSize  = (UINT16)BitFieldRead32 (CpuidEax, 16, 27);
  if (L2DTlb2and4MAssoc == 0x0F) {
    L2DTlb2and4MAssoc = FULLY;
  }

  L2ITlb2and4MAssoc = (UINT8)BitFieldRead32  (CpuidEax, 12, 15);
  L2ITlb2and4MSize  = (UINT16)BitFieldRead32 (CpuidEax, 0, 11);
  if (L2ITlb2and4MAssoc == 0x0F) {
    L2ITlb2and4MAssoc = FULLY;
  }

  L2DTlb4KAssoc = (UINT8)BitFieldRead32  (CpuidEbx, 28, 31);
  L2DTlb4KSize  = (UINT16)BitFieldRead32 (CpuidEbx, 16, 27);
  if (L2DTlb4KAssoc == 0x0F) {
    L2DTlb4KAssoc = FULLY;
  }

  L2ITlb4KAssoc = (UINT8)BitFieldRead32  (CpuidEbx, 12, 15);
  L2ITlb4KSize  = (UINT16)BitFieldRead32 (CpuidEbx, 0, 11);
  if (L2ITlb4KAssoc == 0x0F) {
    L2ITlb4KAssoc = FULLY;
  }
  
  L2Size        = (UINT16)BitFieldRead32 (CpuidEcx, 16, 31);
  L2Assoc       = (UINT8)BitFieldRead32  (CpuidEcx, 12, 15);
  L2LinesPerTag = (UINT8)BitFieldRead32  (CpuidEcx, 8,  11);
  L2LineSize    = (UINT8)BitFieldRead32  (CpuidEcx, 0,  7);
  if (L2Assoc == 0x0F) {
    L2Assoc = FULLY;
  }

  L3Size        = (UINT16)BitFieldRead32 (CpuidEdx, 18, 31);
  L3Assoc       = (UINT8)BitFieldRead32  (CpuidEdx, 12, 15);
  L3LinesPerTag = (UINT8)BitFieldRead32  (CpuidEdx, 8,  11);
  L3LineSize    = (UINT8)BitFieldRead32  (CpuidEdx, 0,  7);
  if (L3Assoc == 0x0F) {
    L3Assoc = FULLY;
  }
  //
  // Translate the AMD CPU Cache data to the Intel format.
  //
  Bytes = (UINT8 *)Eax;
  //
  // al must be 1.   XNU and Linux treat it as the number of cpuid calls
  // required to retrieve all descriptors, however this is undocumented.
  //
  Bytes[0] = 0x01;
  Bytes[1] = InternalCacheInfoToDescriptor (
               PREFETCH,
               NA,
               NA,
               64,
               NA
               );
  Bytes[2] = InternalCacheInfoToDescriptor (
               TLB,
               DATA,
               L1DTlb2and4MAssoc,
               LARGE,
               L1DTlb2and4MSize
               );
  Bytes[3] = InternalCacheInfoToDescriptor (
               TLB,
               INST,
               L1ITlb2and4MAssoc,
               LARGE,
               L1ITlb2and4MSize
               );

  Bytes = (UINT8 *)Ebx;
  Bytes[0] = InternalCacheInfoToDescriptor (
               TLB,
               DATA,
               L1DTlb4KAssoc,
               SMALL,
               L1DTlb4KSize
               );
  Bytes[1] = InternalCacheInfoToDescriptor (
               TLB,
               INST,
               L1ITlb4KAssoc,
               SMALL,
               L1ITlb4KSize
               );
  Bytes[2] = InternalCacheInfoToDescriptor (
               CACHE,
               L1_DATA,
               L1DcAssoc,
               L1DcSize,
               L1DcLineSize
               );
  Bytes[3] = InternalCacheInfoToDescriptor (
               CACHE,
               L1_INST,
               L1IcAssoc,
               L1IcSize,
               L1IcLineSize
               );

  Bytes = (UINT8 *)Ecx;
  Bytes[0] = InternalCacheInfoToDescriptor (
               TLB,
               DATA,
               L2DTlb2and4MAssoc,
               LARGE,
               L2DTlb2and4MSize
               );
  Bytes[1] = InternalCacheInfoToDescriptor (
               TLB,
               INST,
               L2ITlb2and4MAssoc,
               LARGE,
               L2ITlb2and4MSize
               );
  Bytes[2] = InternalCacheInfoToDescriptor (
               TLB,
               DATA,
               L2DTlb4KAssoc,
               SMALL,
               L2DTlb4KSize
               );
  Bytes[3] = InternalCacheInfoToDescriptor (
               TLB,
               INST,
               L2ITlb4KAssoc,
               SMALL,
               L2ITlb4KSize
               );

  Bytes = (UINT8 *)Edx;
  Bytes[0] = InternalCacheInfoToDescriptor (
               CACHE,
               ((L2LinesPerTag < 2) ? L2 : L2_2LINESECTOR),
               L2Assoc,
               L2Size,
               L2LineSize
               );
  Bytes[1] = InternalCacheInfoToDescriptor (
               CACHE,
               ((L3LinesPerTag < 2) ? L3 : L3_2LINESECTOR),
               L3Assoc,
               L3Size,
               L3LineSize
               );
  Bytes[2] = _NULL_;
  Bytes[3] = _NULL_;
}

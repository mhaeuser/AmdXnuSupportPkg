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

///
/// These multipliers are used to encode 1*K .. 64*M in a 16 bit size field
///
#define K  1
#define M  1024

// TODO: Rework the list with the map from the Intel SDM.
STATIC CONST CPUID_CACHE_DESCRIPTOR mCacheDescriptorMap[] = {
  //  -------------------------------------------------------
//  value  type  level    ways  size  entries
//  -------------------------------------------------------
  { 0x00,  _NULL_,  NA,    NA,  NA,  NA  },
  { 0x01,  TLB,  INST,    4,  SMALL,  32  },  
  { 0x02,  TLB,  INST,    FULLY,  LARGE,  2   },  
  { 0x03,  TLB,  DATA,    4,  SMALL,  64  },  
  { 0x04,  TLB,  DATA,    4,  LARGE,  8   },  
  { 0x05,  TLB,  DATA1,    4,  LARGE,  32  },  
  { 0x06,  CACHE,  L1_INST,  4,  8*K,  32  },
  { 0x08,  CACHE,  L1_INST,  4,  16*K,  32  },
  { 0x09,  CACHE,  L1_INST,  4,  32*K,  64  },
  { 0x0A,  CACHE,  L1_DATA,  2,  8*K,  32  },
  { 0x0B,  TLB,  INST,    4,  LARGE,  4   },  
  { 0x0C,  CACHE,  L1_DATA,  4,  16*K,  32  },
  { 0x0D,  CACHE,  L1_DATA,  4,  16*K,  64  },
  { 0x0E,  CACHE,  L1_DATA,  6,  24*K,  64  },
  { 0x21,  CACHE,  L2,    8,  256*K,  64  },
  { 0x22,  CACHE,  L3_2LINESECTOR,  4,  512*K,  64  },
  { 0x23,  CACHE,  L3_2LINESECTOR, 8,  1*M,  64  },
  { 0x25,  CACHE,  L3_2LINESECTOR,  8,  2*M,  64  },
  { 0x29,  CACHE,  L3_2LINESECTOR, 8,  4*M,  64  },
  { 0x2C,  CACHE,  L1_DATA,  8,  32*K,  64  },
  { 0x30,  CACHE,  L1_INST,  8,  32*K,  64  },
  { 0x40,  CACHE,  L2,    NA,  0,  NA  },
  { 0x41,  CACHE,  L2,    4,  128*K,  32  },
  { 0x42,  CACHE,  L2,    4,  256*K,  32  },
  { 0x43,  CACHE,  L2,    4,  512*K,  32  },
  { 0x44,  CACHE,  L2,    4,  1*M,  32  },
  { 0x45,  CACHE,  L2,    4,  2*M,  32  },
  { 0x46,  CACHE,  L3,    4,  4*M,  64  },
  { 0x47,  CACHE,  L3,    8,  8*M,  64  },
  { 0x48,  CACHE,  L2,    12,   3*M,  64  },
  { 0x49,  CACHE,  L2,    16,  4*M,  64  },
  { 0x4A,  CACHE,  L3,    12,   6*M,  64  },
  { 0x4B,  CACHE,  L3,    16,  8*M,  64  },
  { 0x4C,  CACHE,  L3,    12,   12*M,  64  },
  { 0x4D,  CACHE,  L3,    16,  16*M,  64  },
  { 0x4E,  CACHE,  L2,    24,  6*M,  64  },
  { 0x4F,  TLB,  INST,    NA,  SMALL,  32  },  
  { 0x50,  TLB,  INST,    NA,  BOTH,  64  },  
  { 0x51,  TLB,  INST,    NA,  BOTH,  128 },  
  { 0x52,  TLB,  INST,    NA,  BOTH,  256 },  
  { 0x55,  TLB,  INST,    FULLY,  BOTH,  7   },  
  { 0x56,  TLB,  DATA0,    4,  LARGE,  16  },  
  { 0x57,  TLB,  DATA0,    4,  SMALL,  16  },  
  { 0x59,  TLB,  DATA0,    FULLY,  SMALL,  16  },  
  { 0x5A,  TLB,  DATA0,    4,  LARGE,  32  },  
  { 0x5B,  TLB,  DATA,    NA,  BOTH,  64  },  
  { 0x5C,  TLB,  DATA,    NA,  BOTH,  128 },  
  { 0x5D,  TLB,  DATA,    NA,  BOTH,  256 },  
  { 0x60,  CACHE,  L1,    16*K,  8,  64  },
  { 0x61,  CACHE,  L1,    4,  8*K,  64  },
  { 0x62,  CACHE,  L1,    4,  16*K,  64  },
  { 0x63,  CACHE,  L1,    4,  32*K,  64  },
  { 0x70,  CACHE,  TRACE,    8,  12*K,  NA  },
  { 0x71,  CACHE,  TRACE,    8,  16*K,  NA  },
  { 0x72,  CACHE,  TRACE,    8,  32*K,  NA  },
  { 0x76,  TLB,  INST,    NA,  BOTH,  8   },
  { 0x78,  CACHE,  L2,    4,  1*M,  64  },
  { 0x79,  CACHE,  L2_2LINESECTOR,  8,  128*K,  64  },
  { 0x7A,  CACHE,  L2_2LINESECTOR,  8,  256*K,  64  },
  { 0x7B,  CACHE,  L2_2LINESECTOR,  8,  512*K,  64  },
  { 0x7C,  CACHE,  L2_2LINESECTOR,  8,  1*M,  64  },
  { 0x7D,  CACHE,  L2,    8,  2*M,  64  },
  { 0x7F,  CACHE,  L2,    2,  512*K,  64  },
  { 0x80,  CACHE,  L2,    8,  512*K,  64  },
  { 0x82,  CACHE,  L2,    8,  256*K,  32  },
  { 0x83,  CACHE,  L2,    8,  512*K,  32  },
  { 0x84,  CACHE,  L2,    8,  1*M,  32  },
  { 0x85,  CACHE,  L2,    8,  2*M,  32  },
  { 0x86,  CACHE,  L2,    4,  512*K,  64  },
  { 0x87,  CACHE,  L2,    8,  1*M,  64  },
  { 0xB0,  TLB,  INST,    4,  SMALL,  128 },  
  { 0xB1,  TLB,  INST,    4,  LARGE,  8   },  
  { 0xB2,  TLB,  INST,    4,  SMALL,  64  },  
  { 0xB3,  TLB,  DATA,    4,  SMALL,  128 },  
  { 0xB4,  TLB,  DATA1,    4,  SMALL,  256 },  
  { 0xB5,  TLB,  DATA1,    8,  SMALL,  64  },  
  { 0xB6,  TLB,  DATA1,    8,  SMALL,  128 },  
  { 0xBA,  TLB,  DATA1,    4,  BOTH,  64  },  
  { 0xC1,  STLB,  DATA1,    8,  SMALL,  1024},  
  { 0xCA,  STLB,  DATA1,    4,  SMALL,  512 },  
  { 0xD0,  CACHE,  L3,    4,  512*K,  64  },  
  { 0xD1,  CACHE,  L3,    4,  1*M,  64  },  
  { 0xD2,  CACHE,  L3,    4,  2*M,  64  },  
  { 0xD3,  CACHE,  L3,    4,  4*M,  64  },  
  { 0xD4,  CACHE,  L3,    4,  8*M,  64  },  
  { 0xD6,  CACHE,  L3,    8,  1*M,  64  },  
  { 0xD7,  CACHE,  L3,    8,  2*M,  64  },  
  { 0xD8,  CACHE,  L3,    8,  4*M,  64  },  
  { 0xD9,  CACHE,  L3,    8,  8*M,  64  },  
  { 0xDA,  CACHE,  L3,    8,  12*M,  64  },  
  { 0xDC,  CACHE,  L3,    12,   1536*K,  64  },  
  { 0xDD,  CACHE,  L3,    12,   3*M,  64  },  
  { 0xDE,  CACHE,  L3,    12,   6*M,  64  },  
  { 0xDF,  CACHE,  L3,    12,  12*M,  64  },  
  { 0xE0,  CACHE,  L3,    12,  18*M,  64  },  
  { 0xE2,  CACHE,  L3,    16,  2*M,  64  },  
  { 0xE3,  CACHE,  L3,    16,  4*M,  64  },  
  { 0xE4,  CACHE,  L3,    16,  8*M,  64  },  
  { 0xE5,  CACHE,  L3,    16,  16*M,  64  },  
  { 0xE6,  CACHE,  L3,    16,  24*M,  64  },  
  { 0xF0,  PREFETCH, NA,    NA,  64,  NA  },  
  { 0xF1,  PREFETCH, NA,    NA,  128,  NA  },  
  { 0xFF,  CACHE,  NA,    NA,  0,  NA  }
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

/** @file
  Library instance for debug log print to memory.

  Copyright (c) 2019, Gavin Xue. All rights reserved.<BR>
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
**/

#include <Uefi.h>
#include <Library/IoLib.h>
#include <Library/PcdLib.h>
#include <Library/RamDebugLib.h>

//
// Default policy, shift the old data when buffer full
//
#define RAM_DEBUG_STOP_LOGGING_WHEN_BUFFER_FULL     FALSE

//
// Debug String End flag
//
#define DEBUG_STRING_END_FLAG          (0x0)

//
// Debug Print RAM Signature Offset
//
#define DEBUG_PRINT_RAM_SIG_ADDR       ((UINTN) RAM_DEBUG_BASE + (UINTN) &(((RAM_DEBUG_PRINT_HEADER *) 0)->RamDebugSig))

//
// Debug Print RAM Latest Index Offset
//
#define DEBUG_PRINT_RAM_LATESTIDX_ADDR ((UINTN) RAM_DEBUG_BASE + (UINTN) &(((RAM_DEBUG_PRINT_HEADER *) 0)->LatestIdx))

//
// Print Buffer start
//
#define DEBUG_PRINT_BUFFER_START       ((UINTN) RAM_DEBUG_BASE + sizeof (RAM_DEBUG_PRINT_HEADER))

//
// Print Buffer Size
//
#define DEBUG_PRINT_BUFFER_SIZE        ((UINT32) RAM_DEBUG_SIZE - sizeof (RAM_DEBUG_PRINT_HEADER))

//
// Debug Print RAM default Value
//
#define DEBUG_PRINT_BUFFER_DFT_VALUE   ((UINT8) 0xCC)


/**
  Get debug print latest Index.

  @param[in][out] LatestIndex           LatestIndex to be filled.

  @retval EFI_INVALID_PARAMETER         Invalid parameter.
  @retval EFI_WRITE_PROTECTED           Debug RAM region is read only.
  @retval EFI_SUCCESS                   Successfully get the Latest Index.

**/
EFI_STATUS
EFIAPI
RamDebugGetLatestIndex (
  IN OUT UINT32        *LatestIndex
  )
{
  UINT32             Index;
  UINT32             TempLatestIndex;
  UINT32             RamDebugSig;
  UINT8              DpBufferDftValue;

  if (LatestIndex == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  if (RAM_DEBUG_BASE == 0) {
    return EFI_INVALID_PARAMETER;
  }

  if (RAM_DEBUG_SIZE == 0) {
    return EFI_INVALID_PARAMETER;
  }

  RamDebugSig = MmioRead32 (DEBUG_PRINT_RAM_SIG_ADDR);
  //
  // Check Signature, if uninit means it is 1st time comes here
  //
  if (RamDebugSig != RAM_DEBUG_HEADER_ID) {
    //
    // Init Debug Print RAM Header
    //
    RamDebugSig = RAM_DEBUG_HEADER_ID;
    MmioWrite32 (DEBUG_PRINT_RAM_SIG_ADDR, RamDebugSig);

    //
    // Check if read only memory
    //
    RamDebugSig = 0;
    RamDebugSig = MmioRead32 (DEBUG_PRINT_RAM_SIG_ADDR);
    if (RamDebugSig != RAM_DEBUG_HEADER_ID) {
      return EFI_WRITE_PROTECTED;
    }

    //
    // Init Latest Index with zero
    //
    TempLatestIndex = 0;
    MmioWrite32 (DEBUG_PRINT_RAM_LATESTIDX_ADDR, TempLatestIndex);
    //
    // Init Debug Print Buffer with defalut value
    //
    DpBufferDftValue = DEBUG_PRINT_BUFFER_DFT_VALUE;
    for (Index = 0; Index < DEBUG_PRINT_BUFFER_SIZE; Index++) {
      MmioWrite8 (DEBUG_PRINT_BUFFER_START + Index, DpBufferDftValue);
    }
  }

  *LatestIndex = MmioRead32 (DEBUG_PRINT_RAM_LATESTIDX_ADDR);

  return EFI_SUCCESS;
}

/**
  Get a record's size which end of 0x00.

  @param[in] RecordAddr            The address of a record.

  @retval The size of a record

**/
UINT32
EFIAPI
RamDebugGetRecordSize (
  IN UINT64         RecordAddr
  )
{
  UINT32           RecordSize;
  UINT8            BufValue8;

  RecordSize = 0;

  do {
    BufValue8 = MmioRead8 ((UINTN) RecordAddr);
    RecordAddr++;
    RecordSize++;
  } while (BufValue8 != DEBUG_STRING_END_FLAG);

  return RecordSize;
}

/**
  Clean up the Debug Print buffer, remove oldest record, do the relocation.

  @param[in] NewRecordLength            The address of a record.
  @param[in] LatestIndex                LatestIndex to be Updated.

  @retval VOID

**/
VOID
EFIAPI
RamDebugCleanUp (
  IN UINTN           NewRecordLength,
  IN OUT UINT32      *LatestIndex
  )
{
  UINT32             RemainedSize;
  UINT32             RecordOffset;
  UINT32             RecordSize;
  UINT8              Value8;
  UINT32             Index;

  RecordOffset = 0;
  RemainedSize = DEBUG_PRINT_BUFFER_SIZE - *LatestIndex;

  //
  // Calculate how many record need be removed
  //
  while (RemainedSize < NewRecordLength) {
    RecordSize = RamDebugGetRecordSize (DEBUG_PRINT_BUFFER_START + RecordOffset);
    RecordOffset += RecordSize;
    RemainedSize += RecordSize;
  }

  //
  // Move forward (RecordOffset) byte data
  //
  for (Index = 0; Index < (*LatestIndex - RecordOffset); Index++) {
    Value8 = MmioRead8 (DEBUG_PRINT_BUFFER_START + RecordOffset + Index);
    MmioWrite8 (DEBUG_PRINT_BUFFER_START + Index, Value8);
  }
  *LatestIndex -= RecordOffset;

  //
  // Fill LatestIndex ~ End with default value
  //
  for (Index = 0; Index < (DEBUG_PRINT_BUFFER_SIZE - *LatestIndex); Index++) {
    Value8 = DEBUG_PRINT_BUFFER_DFT_VALUE;
    MmioWrite8 (DEBUG_PRINT_BUFFER_START + *LatestIndex + Index, Value8);
  }
}

/**
  Print formated string to memory.

  @param[in] Buffer                   The pointer to the data buffer to be written.
  @param[in] BufferSize               The size of buffer to written to memory.

  @retval VOID

**/
VOID
RamDebugPrint (
  IN CHAR8            *Buffer,
  IN UINTN            BufferSize
  )
{
  EFI_STATUS          Status;
  UINT32              LatestIndex;
  UINT32              Counter;
  UINT64              DebugPrintBufferAddr;
  UINT32              TempBufferSize;
  UINT32              StopLoggingWhenBufferFull;

  IoWrite8 (RTC_ADDRESS_REGISTER, RAM_DEBUG_CMOS_OFFSET);
  if (IoRead8 (RTC_DATA_REGISTER) == RAM_DEBUG_DISABLE) {
    return;
  }

  if (BufferSize > DEBUG_PRINT_BUFFER_SIZE) {
    return;
  }

  //
  // Get Latest Index
  //
  Status = RamDebugGetLatestIndex (&LatestIndex);
  if (!EFI_ERROR (Status)) {
    //
    // Add the size for '\0'
    //
    TempBufferSize = (UINT32) BufferSize + 1;
    StopLoggingWhenBufferFull = RAM_DEBUG_STOP_LOGGING_WHEN_BUFFER_FULL;

    //
    // Check if exceed the limit, if so shift the oldest data, and do the relocation
    //
    if (((LatestIndex + TempBufferSize) > DEBUG_PRINT_BUFFER_SIZE) && !StopLoggingWhenBufferFull) {
      RamDebugCleanUp (TempBufferSize, &LatestIndex);
    }

    if ((LatestIndex + TempBufferSize) <= DEBUG_PRINT_BUFFER_SIZE) {
      //
      // Save the data to RAM, Update the Latest Index
      //
      Counter = TempBufferSize;
      DebugPrintBufferAddr = (UINT64) (DEBUG_PRINT_BUFFER_START + LatestIndex);
      while (Counter--) {
        MmioWrite8 ((UINTN) DebugPrintBufferAddr, *Buffer);
        DebugPrintBufferAddr++;
        Buffer++;
      }

      LatestIndex += TempBufferSize;
      MmioWrite32 (DEBUG_PRINT_RAM_LATESTIDX_ADDR, LatestIndex);
    }
  }
}

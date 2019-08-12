/** @file
  function definitions for debug log to memory library.

  Copyright (c) 2019, Gavin Xue. All rights reserved.<BR>
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
**/

#ifndef _RAM_DEBUG_LIB_H_
#define _RAM_DEBUG_LIB_H_

#define RAM_DEBUG_BASE                 FixedPcdGet32 (PcdRamDebugMemAddr)
#define RAM_DEBUG_SIZE                 FixedPcdGet32 (PcdRamDebugMemSize)

//
// RAM Debug Print Header Signature: "RMDP".
//
#define RAM_DEBUG_HEADER_ID      SIGNATURE_32 ('R','M','D','P')

//
// RAM Debug Print Header
//
#pragma pack (1)
typedef struct {
  UINT32     RamDebugSig;        // 'R','M','D','P'
  UINT32     LatestIdx;          // Latest Index
  UINT8      Reserved[8];        // Reserved
} RAM_DEBUG_PRINT_HEADER;
#pragma pack ()

//
// CMOS flag for controlling RAM debug behavior.
//
#define RTC_ADDRESS_REGISTER     0x70
#define RTC_DATA_REGISTER        0x71

#define RAM_DEBUG_CMOS_OFFSET    0x50
#define RAM_DEBUG_ENABLE         0xFF
#define RAM_DEBUG_DISABLE        0x5A

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
  );

#endif

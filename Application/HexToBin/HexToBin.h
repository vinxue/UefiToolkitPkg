/** @file
  function definitions for internal to application functions.

  Copyright (c) 2022, Gavin Xue. All rights reserved.<BR>
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
**/

#ifndef _HEX_TO_BIN_H_
#define _HEX_TO_BIN_H_

#include <PiDxe.h>
#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/DebugLib.h>
#include <Library/ShellLib.h>
#include <Library/BaseLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DevicePathLib.h>
#include <Library/PrintLib.h>
#include <Protocol/BlockIo.h>
#include <Protocol/DiskIo.h>
//
// Intel HEX standard record types
//
typedef enum {
  RECORD_DATA,
  RECORD_END_OF_FILE,
  RECORD_EXTENDED_SEGMENT_ADDRESS,
  RECORD_START_SEGMENT_ADDRESS,
  RECORD_EXTENDED_LINEAR_ADDRESS,
  RECORD_START_LINEAR_ADDRESS
} INTEL_HEX_RECORD_TYPES;

//
// Define a maximum length of a line
//
#define MAX_LINE_LENGTH           256

#define INTEL_HEX_START_CODE      ':'

#endif

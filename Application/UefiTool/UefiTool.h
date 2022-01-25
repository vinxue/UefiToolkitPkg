/** @file
  function definitions for internal to application functions.

  Copyright (c) 2017 - 2022, Gavin Xue. All rights reserved.<BR>
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
**/

#ifndef _UEFI_TOOL_H_
#define _UEFI_TOOL_H_

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
#include <Library/IoLib.h>
#include <Register/Intel/Cpuid.h>
#include <Register/Intel/Msr.h>
#include <Protocol/MpService.h>

//
// CPUID Structures
//
typedef struct {
  UINT32         Eax;
  UINT32         Ebx;
  UINT32         Ecx;
  UINT32         Edx;
} UT_CPUID_REGISTER;

#endif

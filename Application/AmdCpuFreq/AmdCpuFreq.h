/** @file
  function definitions for internal to application functions.

  Copyright (c) 2019, Gavin Xue. All rights reserved.<BR>
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
**/

#ifndef _AMD_CPU_FREQ_H_
#define _AMD_CPU_FREQ_H_

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

#define MSR_PSTATE_0          0xC0010064ul
#define MSR_HW_PSTATE_STATUS  0xC0010293ul

//
// P-state MSR
//
typedef union {
  struct {                             // Bitfields of P-state MSR
    UINT64 CpuFid_7_0:8;               // CpuFid[7:0]
    UINT64 CpuDfsId:6;                 // CpuDfsId
    UINT64 CpuVid:8;                   // CpuVid
    UINT64 IddValue:8;                 // IddValue
    UINT64 IddDiv:2;                   // IddDiv
    UINT64 :31;                        // Reserved
    UINT64 PstateEn:1;                 // Pstate Enable
  } Field;
  UINT64  Value;
} PSTATE_MSR;

//
// Hardware PState Status MSR
//
typedef union {
  struct {                            // Bitfields of Hardware PState Status MSR
    UINT64 CurCpuFid:8;               // Current Pstate FID
    UINT64 CurCpuDfsId:6;             // Current Pstate DfsId
    UINT64 CurCpuVid:8;               // Current Pstate VID
    UINT64 CurHwPstate:3;             // Current Hw Pstate
    UINT64 :39;                       // Reserved
  } Field;
  UINT64  Value;
} HW_PSTATE_STATUS_MSR;

#endif

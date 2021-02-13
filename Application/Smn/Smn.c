/** @file
  A UEFI tool for AMD SMN register access.

  Copyright (c) 2021, Gavin Xue. All rights reserved.<BR>
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
**/

#include "Smn.h"

/**
  Read AMD SMN register.

  @param[in] IohcBus       IOHC (Node) bus number
  @param[in] SmnAddress    Register SMN address
  @param[in] Value         Pointer to register value

 */
VOID
AmdSmnRead (
  IN       UINT32              IohcBus,
  IN       UINT32              SmnAddress,
  IN       UINT32              *Value
  )
{
  UINT32    RegIndex;
  UINTN     PciAddress;

  RegIndex = SmnAddress;
  PciAddress = ((UINTN) IohcBus << 20) + IOHC_NB_SMN_INDEX_2_BIOS;
  PciWrite32 (PciAddress, RegIndex);
  PciAddress = ((UINTN) IohcBus << 20) + IOHC_NB_SMN_DATA_2_BIOS;
  *Value = PciRead32 (PciAddress);
}

/**
  Write AMD SMN register.

  @param[in] IohcBus       IOHC (Node) bus number
  @param[in] SmnAddress    Register SMN address
  @param[in] Value         Pointer to register value

 */
VOID
AmdSmnWrite (
  IN       UINT32              IohcBus,
  IN       UINT32              SmnAddress,
  IN       UINT32              Value
  )
{
  UINT32    RegIndex;
  UINTN     PciAddress;

  RegIndex = SmnAddress;
  PciAddress = ((UINTN) IohcBus << 20) + IOHC_NB_SMN_INDEX_2_BIOS;
  PciWrite32 (PciAddress, RegIndex);
  PciAddress = ((UINTN) IohcBus << 20) + IOHC_NB_SMN_DATA_2_BIOS;
  PciWrite32 (PciAddress, Value);
}

VOID
EFIAPI
ShowHelpInfo (
  VOID
  )
{
  Print (L"Help info:\n");
  Print (L"  Smn.efi [Address] [Data]\n");
  Print (L"  Address: SMN address to access\n");
  Print (L"  Data:    Data to be written\n\n");
}

/**
  UEFI application entry point which has an interface similar to a
  standard C main function.

  The ShellCEntryLib library instance wrappers the actual UEFI application
  entry point and calls this ShellAppMain function.

  @param[in]  Argc  The number of parameters.
  @param[in]  Argv  The array of pointers to parameters.

  @retval  0               The application exited normally.
  @retval  Other           An error occurred.

**/
INTN
EFIAPI
ShellAppMain (
  IN UINTN                     Argc,
  IN CHAR16                    **Argv
  )
{
  UINT32              SmnAddress;
  UINT32              Value;

  if (Argc == 1) {
    ShowHelpInfo ();
    return EFI_INVALID_PARAMETER;
  }

  if (Argc == 2) {
    if ((!StrCmp (Argv[1], L"-h")) || (!StrCmp (Argv[1], L"-H")) ||
        (!StrCmp (Argv[1], L"/h")) || (!StrCmp (Argv[1], L"/H"))) {
      ShowHelpInfo ();
      return EFI_INVALID_PARAMETER;
    }

    SmnAddress = (UINT32) StrHexToUintn (Argv[1]);

    AmdSmnRead (0, SmnAddress, &Value);

    Print (L"Read SMN address: 0x%x, value: 0x%x\n", SmnAddress, Value);

    return EFI_SUCCESS;
  }

  if (Argc == 3) {
    if ((!StrCmp (Argv[1], L"-h")) || (!StrCmp (Argv[1], L"-H")) ||
        (!StrCmp (Argv[1], L"/h")) || (!StrCmp (Argv[1], L"/H"))) {
      ShowHelpInfo ();
      return EFI_INVALID_PARAMETER;
    }

    SmnAddress = (UINT32) StrHexToUintn (Argv[1]);
    Value      = (UINT32) StrHexToUintn (Argv[2]);

    AmdSmnWrite (0, SmnAddress, Value);

    Print (L"Write value: 0x%x to SMN address: 0x%x\n", Value, SmnAddress);

    return EFI_SUCCESS;
  }

  return 0;
}

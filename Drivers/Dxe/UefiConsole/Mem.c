/** @file
  Main file for Dmem shell Debug1 function.

  Copyright (c) 2010 - 2016, Intel Corporation. All rights reserved.<BR>
  (C) Copyright 2015 Hewlett-Packard Development Company, L.P.<BR>
  (C) Copyright 2015 Hewlett Packard Enterprise Development LP<BR>
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#include "UefiConsole.h"
#include <Protocol/PciRootBridgeIo.h>

/**
  Make a printable character.

  If Char is printable then return it, otherwise return a question mark.

  @param[in] Char     The character to make printable.

  @return A printable character representing Char.
**/
CHAR16
MakePrintable (
  IN CONST CHAR16 Char
  )
{
  if ((Char < 0x20 && Char > 0) || (Char > 126)) {
    return (L'?');
  }
  return (Char);
}

/**
  Display some Memory-Mapped-IO memory.

  @param[in] Address    The starting address to display.
  @param[in] Size       The length of memory to display.
**/
SHELL_STATUS
DisplayMmioMemory (
  IN CONST VOID   *Address,
  IN CONST UINTN  Size
  )
{
  EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL *PciRbIo;
  EFI_STATUS                      Status;
  VOID                            *Buffer;
  SHELL_STATUS                    ShellStatus;

  ShellStatus = SHELL_SUCCESS;

  Status = gBS->LocateProtocol (&gEfiPciRootBridgeIoProtocolGuid, NULL, (VOID **)&PciRbIo);
  if (EFI_ERROR (Status)) {
    Print (L"mem: Protocol - PciRootBridgeIo not found.\n");
    return (SHELL_NOT_FOUND);
  }
  Buffer = AllocateZeroPool (Size);
  if (Buffer == NULL) {
    return SHELL_OUT_OF_RESOURCES;
  }

  Status = PciRbIo->Mem.Read (PciRbIo, EfiPciWidthUint8, (UINT64) (UINTN) Address, Size, Buffer);
  if (EFI_ERROR (Status)) {
    Print (L"mem: Problem accessing the data using Protocol - PciRootBridgeIo\n");
    ShellStatus = SHELL_NOT_FOUND;
  } else {
    Print (L"Memory Mapped IO Address %016LX %X Bytes\n", (UINT64) (UINTN) Address, Size);
    DumpHex (2, (UINTN)Address, Size, Buffer);
  }

  FreePool (Buffer);
  return (ShellStatus);
}

STATIC CONST SHELL_PARAM_ITEM ParamList[] = {
  {L"-mmio", TypeFlag},
  {NULL, TypeMax}
};

/**
  Function for 'mem' command.

  @param[in] ImageHandle  Handle to the Image (NULL if Internal).
  @param[in] SystemTable  Pointer to the System Table (NULL if Internal).
**/
SHELL_STATUS
EFIAPI
ShellCommandRunMem (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS          Status;
  LIST_ENTRY          *Package;
  CHAR16              *ProblemParam;
  SHELL_STATUS        ShellStatus;
  VOID                *Address;
  UINT64              Size;
  CONST CHAR16        *Temp1;

  ShellStatus         = SHELL_SUCCESS;
  Status              = EFI_SUCCESS;
  Address             = NULL;
  Size                = 0;

  //
  // parse the command line
  //
  Status = ConsoleCommandLineParse (ParamList, &Package, &ProblemParam, TRUE);
  if (EFI_ERROR (Status)) {
    if (Status == EFI_VOLUME_CORRUPTED && ProblemParam != NULL) {
      Print (L"mem: Unknown flag - '%s'\n", ProblemParam);
      FreePool (ProblemParam);
      ShellStatus = SHELL_INVALID_PARAMETER;
    } else {
      ASSERT (FALSE);
    }
  } else {
    if (ConsoleCommandLineGetCount (Package) > 3) {
      Print (L"mem: Too many arguments.\n");
      ShellStatus = SHELL_INVALID_PARAMETER;
    } else {
      Temp1 = ConsoleCommandLineGetRawValue (Package, 1);
      if (Temp1 == NULL) {
        Address = gST;
        Size = 512;
      } else {
        if (!ConsoleIsHexOrDecimalNumber (Temp1, TRUE, FALSE) ||
            EFI_ERROR (ConsoleConvertStringToUint64 (Temp1, (UINT64 *)&Address, TRUE, FALSE))) {
          Print (L"mem: Invalid argument - '%s'\n", Temp1);
          ShellStatus = SHELL_INVALID_PARAMETER;
        }
        Temp1 = ConsoleCommandLineGetRawValue (Package, 2);
        if (Temp1 == NULL) {
          Size = 512;
        } else {
          if (!ConsoleIsHexOrDecimalNumber (Temp1, FALSE, FALSE) ||
              EFI_ERROR (ConsoleConvertStringToUint64 (Temp1, &Size, TRUE, FALSE))) {
            Print (L"mem: Invalid argument - '%s'\n", Temp1);
            ShellStatus = SHELL_INVALID_PARAMETER;
          }
        }
      }
    }

    if (ShellStatus == SHELL_SUCCESS) {
      if (!ConsoleCommandLineGetFlag (Package, L"-mmio")) {
        Print (L"Memory Address %016LX %X Bytes\n", (UINT64) (UINTN) Address, Size);
        DumpHex (2, (UINTN) Address, (UINTN) Size, Address);
      } else {
        ShellStatus = DisplayMmioMemory (Address, (UINTN)Size);
      }
    }

    ConsoleCommandLineFreeVarList (Package);
  }

  return (ShellStatus);
}

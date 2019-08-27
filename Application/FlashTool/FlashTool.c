/** @file
  A simple UEFI tool for SPI flash programming.

  Copyright (c) 2019, Gavin Xue. All rights reserved.<BR>
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
**/

#include "FlashTool.h"


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
  EFI_STATUS            Status;
  VOID                  *Buffer;
  SHELL_FILE_HANDLE     SourceHandle;
  UINTN                 SourceFileSize;
  UINTN                 StartAddress;

  //
  // Open source file
  //
  Status = ShellOpenFileByName (Argv[1], &SourceHandle, EFI_FILE_MODE_READ, 0);
  if (EFI_ERROR (Status)) {
    Print (L"Failed to open file %s, Status: %r\n", Argv[1], Status);
    return Status;
  }

  //
  // Get file size of source file
  //
  Status = ShellGetFileSize (SourceHandle, &SourceFileSize);
  if (EFI_ERROR (Status)) {
    Print (L"Failed to read file %s size, Status: %r\n", Argv[1], Status);
    if (SourceHandle != NULL) {
      ShellCloseFile (&SourceHandle);
    }
    return Status;
  }

  Buffer = AllocateZeroPool (SourceFileSize);
  if (Buffer == NULL) {
    Print (L"Allocate pool failed\n");
    if (SourceHandle != NULL) {
      ShellCloseFile (&SourceHandle);
    }
    return EFI_OUT_OF_RESOURCES;
  }

  Status = ShellReadFile (SourceHandle, &SourceFileSize, Buffer);
  if (EFI_ERROR (Status)) {
    Print (L"Failed to read file %s, Status: %r\n", Argv[1], Status);
    if (SourceHandle != NULL) {
      ShellCloseFile (&SourceHandle);
    }
    if (Buffer != NULL) {
      FreePool (Buffer);
    }
    return Status;
  }

  if (SourceFileSize != (UINTN) PcdGet32 (PcdFlashAreaSize)) {
    Print (L"BIOS file size %x is not equal to flash size 0x%x.\n", SourceFileSize, (UINTN) PcdGet32 (PcdFlashAreaSize));
    if (SourceHandle != NULL) {
      ShellCloseFile (&SourceHandle);
    }
    if (Buffer != NULL) {
      FreePool (Buffer);
    }
    return EFI_BAD_BUFFER_SIZE;
  }

  StartAddress = 0;
  Print (L"Updating flash...\n");
  Status = PerformFlashWrite (
             PlatformFirmwareTypeSystemFirmware,
             StartAddress,
             FlashAddressTypeRelativeAddress,
             Buffer,
             (UINTN) PcdGet32 (PcdFlashAreaSize)
             );
  if (EFI_ERROR (Status)) {
    Print (L"Program failed: %r\n", Status);
  } else {
    Print (L"Program completed.\n");
  }

  if (SourceHandle != NULL) {
    ShellCloseFile (&SourceHandle);
  }

  if (Buffer != NULL) {
    FreePool (Buffer);
  }

  return EFI_SUCCESS;
}

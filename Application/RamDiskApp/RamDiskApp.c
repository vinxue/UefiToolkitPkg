/** @file
  A UEFI tool for RAM disk demo.

  Copyright (c) 2021, Gavin Xue. All rights reserved.<BR>
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
**/

#include "RamDiskApp.h"

/**
  This function reads a binary from disk.

  @param[in]  FileName          Pointer to file name
  @param[out] BufferSize        Return the number of bytes read.
  @param[out] Buffer            The buffer to put read data into.

  @retval EFI_SUCCESS           Data was read.
  @retval EFI_NO_MEDIA          The device has no media.
  @retval EFI_DEVICE_ERROR      The device reported an error.
  @retval EFI_VOLUME_CORRUPTED  The file system structures are corrupted.
  @retval EFI_BUFFER_TO_SMALL   Buffer is too small. ReadSize contains required size.

**/
EFI_STATUS
EFIAPI
ReadFileFromDisk (
  IN  CHAR16               *FileName,
  OUT  UINTN               *BufferSize,
  OUT  VOID                **Buffer
  )
{
  EFI_STATUS           Status;
  SHELL_FILE_HANDLE    FileHandle;
  UINTN                FileSize;
  VOID                 *FileBuffer;

  FileBuffer = NULL;

  Status = ShellOpenFileByName (FileName, &FileHandle, EFI_FILE_MODE_READ, 0);
  if (EFI_ERROR (Status)) {
    Print (L"Open file failed: %r\n", Status);
    return Status;
  }

  Status = ShellGetFileSize (FileHandle, &FileSize);
  if (EFI_ERROR (Status)) {
    Print (L"Failed to read file size, Status: %r\n", Status);
    if (FileHandle != NULL) {
      ShellCloseFile (&FileHandle);
    }
    return Status;
  }

  Status = gBS->AllocatePool (
                  EfiBootServicesData,
                  (UINTN) FileSize,
                  (VOID**) &FileBuffer
                  );
  if ((FileBuffer == NULL) || EFI_ERROR(Status)) {
    Print (L"Allocate resouce failed\n");
    if (FileHandle != NULL) {
      ShellCloseFile (&FileHandle);
    }
    return EFI_OUT_OF_RESOURCES;
  }

  Status = ShellReadFile (FileHandle, &FileSize, FileBuffer);
  if (EFI_ERROR (Status)) {
    Print (L"Failed to read file, Status: %r\n", Status);
    if (FileHandle != NULL) {
      ShellCloseFile (&FileHandle);
    }
    if (FileBuffer != NULL) {
      FreePool (FileBuffer);
    }
    return Status;
  }

  ShellCloseFile (&FileHandle);

  *BufferSize = FileSize;
  *Buffer     = FileBuffer;

  return EFI_SUCCESS;
}

/**
  Display Usage and Help information.

**/
VOID
EFIAPI
ShowHelpInfo (
  VOID
  )
{
  Print (L"Help info:\n");
  Print (L"  RamDiskApp.efi [ImageFile]\n");
  Print (L"  ImageFile: The file to be created to RAM disk.\n\n");
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
  EFI_STATUS                 Status;
  EFI_RAM_DISK_PROTOCOL      *RamDisk;
  EFI_DEVICE_PATH_PROTOCOL   *DevicePath;
  VOID                       *Buffer;
  UINTN                      BufferSize;

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

    //
    // Locate RAM Disk protocol
    //
    Status = gBS->LocateProtocol (&gEfiRamDiskProtocolGuid, NULL, (VOID**) &RamDisk);
    if (EFI_ERROR (Status)) {
      Print (L"Couldn't find the RAM Disk protocol: %r\n", Status);
      return Status;
    }

    //
    // Open image file
    //
    Status = ReadFileFromDisk (Argv[1], &BufferSize, &Buffer);
    if (EFI_ERROR (Status)) {
      Print (L"Read flash file failed. %r\n", Status);
      return Status;
    }

    //
    // Register a RAM disk with specified address, size and type.
    //
    Status = RamDisk->Register (
                        (UINTN) Buffer,
                        (UINT64) BufferSize,
                        &gEfiVirtualDiskGuid,
                        NULL,
                        &DevicePath
                        );
    if (EFI_ERROR (Status)) {
      Print (L"Failed to register RAM Disk: %r\n", Status);
      return Status;
    }

    Print (L"Create RAM Disk passed.\n");
    Print (L"Device Path is: %s\n\n", ConvertDevicePathToText (DevicePath, FALSE, FALSE));

    return EFI_SUCCESS;
  }

  return 0;
}

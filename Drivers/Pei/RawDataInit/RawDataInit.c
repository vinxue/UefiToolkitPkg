
/** @file
  Get RAW data from FVMAIN in PEI stage.

  Copyright (c) 2020, Gavin Xue. All rights reserved.<BR>
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
**/

#include <Base.h>
#include <PiPei.h>
#include <Uefi.h>
#include <Uefi/UefiBaseType.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/HobLib.h>
#include <Library/UefiLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/DebugLib.h>
#include <Library/PeiServicesLib.h>
#include <Library/PeiServicesTablePointerLib.h>

EFI_STATUS
EFIAPI
RawDataInitAtEndOfPei (
  IN EFI_PEI_SERVICES          **PeiServices,
  IN EFI_PEI_NOTIFY_DESCRIPTOR *NotifyDesc,
  IN VOID                      *Ppi
  );

STATIC EFI_PEI_NOTIFY_DESCRIPTOR mRawDataNotifyDesc = {
  (EFI_PEI_PPI_DESCRIPTOR_NOTIFY_CALLBACK | EFI_PEI_PPI_DESCRIPTOR_TERMINATE_LIST),
  &gEfiEndOfPeiSignalPpiGuid,
  RawDataInitAtEndOfPei
};


STATIC CONST CHAR8 Hex[] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};

/**
  Dump some hexadecimal data to the screen.

  @param[in] Indent     How many spaces to indent the output.
  @param[in] Offset     The offset of the printing.
  @param[in] DataSize   The size in bytes of UserData.
  @param[in] UserData   The data to print out.

**/
VOID
EFIAPI
DumpHex (
  IN UINTN        Indent,
  IN UINTN        Offset,
  IN UINTN        DataSize,
  IN VOID         *UserData
  )
{
  UINT8           *Data;
  CHAR8           Val[50];
  CHAR8           Str[20];
  UINT8           TempByte;
  UINTN           Size;
  UINTN           Index;

  Data = UserData;
  while (DataSize != 0) {
    Size = 16;
    if (Size > DataSize) {
      Size = DataSize;
    }

    for (Index = 0; Index < Size; Index += 1) {
      TempByte            = Data[Index];
      Val[Index * 3 + 0]  = Hex[TempByte >> 4];
      Val[Index * 3 + 1]  = Hex[TempByte & 0xF];
      Val[Index * 3 + 2]  = (CHAR8) ((Index == 7) ? '-' : ' ');
      Str[Index]          = (CHAR8) ((TempByte < ' ' || TempByte > '~') ? '.' : TempByte);
    }

    Val[Index * 3]  = 0;
    Str[Index]      = 0;
    DEBUG ((DEBUG_ERROR,"%*a%08X: %-48a *%a*\r\n", Indent, "", Offset, Val, Str));

    Data += Size;
    Offset += Size;
    DataSize -= Size;
  }
}

UINTN
EFIAPI
GetSectionSize (
  IN  EFI_SECTION_TYPE              SectionType,
  IN  VOID                          *Data
  )
{
  UINT32                            SectionSize;
  UINTN                             SectionHeaderSize;
  EFI_COMMON_SECTION_HEADER         *SectionData;

  SectionHeaderSize = sizeof (EFI_COMMON_SECTION_HEADER);
  SectionData = (EFI_COMMON_SECTION_HEADER *) ((UINT8 *) Data - SectionHeaderSize);

  if (SectionData->Type != SectionType) {
    SectionHeaderSize = sizeof (EFI_COMMON_SECTION_HEADER2);
    SectionData = (EFI_COMMON_SECTION_HEADER *) ((UINT8 *) Data - SectionHeaderSize);
  }
  ASSERT (SectionData->Type == SectionType);

  if (IS_SECTION2 (SectionData)) {
    SectionSize = SECTION2_SIZE (SectionData);
  } else {
    SectionSize = SECTION_SIZE (SectionData);
  }

  return SectionSize - SectionHeaderSize;
}


EFI_STATUS
EFIAPI
GetRawImage (
  IN EFI_GUID            *NameGuid,
  IN OUT VOID            **Buffer,
  IN OUT UINTN           *Size
  )
{
  EFI_STATUS                Status;
  UINTN                     FvInstances;
  EFI_PEI_FV_HANDLE         VolumeHandle;
  EFI_PEI_FILE_HANDLE       FvFileHandle;

  VolumeHandle = NULL;
  FvInstances    = 0;

  while (TRUE) {
    //
    // Traverse all firmware volume instances.
    //
    Status = PeiServicesFfsFindNextVolume (FvInstances++, &VolumeHandle);
    if (EFI_ERROR (Status)) {
      break;
    }

    FvFileHandle = NULL;
    Status = PeiServicesFfsFindFileByName (NameGuid, VolumeHandle, &FvFileHandle);
    if (EFI_ERROR (Status)) {
      continue;
    }

    //
    // Search RAW section.
    //
    Status = PeiServicesFfsFindSectionData (EFI_SECTION_RAW, FvFileHandle, Buffer);
      if (EFI_ERROR (Status)) {
      continue;
    }

    //
    // Got it!
    //
    *Size = GetSectionSize (EFI_SECTION_RAW, *Buffer);
    DEBUG ((DEBUG_INFO, "RAW data size = %X\n", *Size));
    break;
  }

  return Status;
}

EFI_STATUS
EFIAPI
RawDataInitAtEndOfPei (
  IN EFI_PEI_SERVICES          **PeiServices,
  IN EFI_PEI_NOTIFY_DESCRIPTOR *NotifyDesc,
  IN VOID                      *Ppi
  )
{
  EFI_STATUS             Status;
  UINTN                  RawSize;
  UINT8                  *Buffer;

  Status = GetRawImage (&gRawDataGuid, (VOID **) &Buffer, &RawSize);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "GetRawImage failed: %r\n", Status));
  } else {
    DEBUG ((DEBUG_INFO, "RAW data size: 0x%x\n", RawSize));

    DumpHex (2, 0, 0x100, Buffer);
  }

  return Status;
}

/**
  Entry point of this module.

  @param[in] FileHandle   Handle of the file being invoked.
  @param[in] PeiServices  Describes the list of possible PEI Services.

  @return Status.

**/
EFI_STATUS
EFIAPI
RawDataInitEntryPoint (
  IN       EFI_PEI_FILE_HANDLE  FileHandle,
  IN CONST EFI_PEI_SERVICES     **PeiServices
  )
{
  EFI_STATUS             Status;

  //
  // Register notification at the End of PEI
  //
  Status = PeiServicesNotifyPpi (&mRawDataNotifyDesc);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  return Status;
}

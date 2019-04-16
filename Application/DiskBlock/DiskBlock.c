/** @file
  A simple UEFI tool for disk partition read/write.

  Copyright (c) 2019, Gavin Xue. All rights reserved.<BR>
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
**/

#include "DiskBlock.h"

#define FLASH_DEVICE_PATH_SIZE(DevPath) ( GetDevicePathSize (DevPath) - \
                                            sizeof (EFI_DEVICE_PATH_PROTOCOL))

#define PARTITION_NAME_MAX_LENGTH 72/2

#define IS_ALPHA(Char) (((Char) <= L'z' && (Char) >= L'a') || \
                        ((Char) <= L'Z' && (Char) >= L'Z'))

typedef struct {
  LIST_ENTRY  Link;
  CHAR16      PartitionName[PARTITION_NAME_MAX_LENGTH];
  EFI_HANDLE  PartitionHandle;
} PARTITION_LIST;

STATIC CONST CHAR8 Hex[] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};

LIST_ENTRY mPartitionListHead;
STATIC EFI_DEVICE_PATH_PROTOCOL  *mDiskDevicePath;

/*
  Helper to free the partition list
*/
STATIC
VOID
FreePartitionList (
  VOID
  )
{
  PARTITION_LIST *Entry;
  PARTITION_LIST *NextEntry;

  Entry = (PARTITION_LIST *) GetFirstNode (&mPartitionListHead);
  while (!IsNull (&mPartitionListHead, &Entry->Link)) {
    NextEntry = (PARTITION_LIST *) GetNextNode (&mPartitionListHead, &Entry->Link);

    RemoveEntryList (&Entry->Link);
    FreePool (Entry);

    Entry = NextEntry;
  }
}

/*
  Read the PartitionName fields from the GPT partition entries, putting them
  into an allocated array that should later be freed.
*/
STATIC
EFI_STATUS
ReadPartitionEntries (
  IN  EFI_BLOCK_IO_PROTOCOL *BlockIo,
  OUT EFI_PARTITION_ENTRY  **PartitionEntries
  )
{
  UINTN                       EntrySize;
  UINTN                       NumEntries;
  UINTN                       BufferSize;
  UINT32                      MediaId;
  EFI_PARTITION_TABLE_HEADER *GptHeader;
  EFI_STATUS                  Status;

  MediaId = BlockIo->Media->MediaId;

  //
  // Read size of Partition entry and number of entries from GPT header
  //

  GptHeader = AllocatePool (BlockIo->Media->BlockSize);
  if (GptHeader == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  Status = BlockIo->ReadBlocks (BlockIo, MediaId, 1, BlockIo->Media->BlockSize, (VOID *) GptHeader);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  // Check there is a GPT on the media
  if (GptHeader->Header.Signature != EFI_PTAB_HEADER_ID ||
      GptHeader->MyLBA != 1) {
    DEBUG ((EFI_D_ERROR, "No GPT on flash, do not support MBR.\n"));
    return EFI_DEVICE_ERROR;
  }

  EntrySize = GptHeader->SizeOfPartitionEntry;
  NumEntries = GptHeader->NumberOfPartitionEntries;

  FreePool (GptHeader);

  ASSERT (EntrySize != 0);
  ASSERT (NumEntries != 0);

  BufferSize = ALIGN_VALUE (EntrySize * NumEntries, BlockIo->Media->BlockSize);
  *PartitionEntries = AllocatePool (BufferSize);
  if (PartitionEntries == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  Status = BlockIo->ReadBlocks (BlockIo, MediaId, 2, BufferSize, (VOID *) *PartitionEntries);
  if (EFI_ERROR (Status)) {
    FreePool (PartitionEntries);
    return Status;
  }

  return Status;
}

EFI_STATUS
EFIAPI
InitDevicePath (
  VOID
  )
{
  EFI_STATUS                Status;
  UINT8                     Index;
  UINTN                     HandleCount;
  EFI_HANDLE                *HandleBuffer;
  EFI_BLOCK_IO_PROTOCOL     *BlockIo;
  EFI_DEVICE_PATH_PROTOCOL   *DevicePath;

  //
  // Locate all the BlockIo protocol
  //
  Status = gBS->LocateHandleBuffer (
                  ByProtocol,
                  &gEfiBlockIoProtocolGuid,
                  NULL,
                  &HandleCount,
                  &HandleBuffer
                  );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  for (Index = 0; Index < HandleCount; Index++) {
    Status = gBS->HandleProtocol (
                    HandleBuffer[Index],
                    &gEfiBlockIoProtocolGuid,
                    (VOID **) &BlockIo
                    );
    if (EFI_ERROR (Status)) {
      return Status;
    }

    if ((BlockIo->Media->LogicalPartition) || (BlockIo->Media->RemovableMedia)) {
      continue;
    }

    Status = gBS->HandleProtocol (
                    HandleBuffer[Index],
                    &gEfiDevicePathProtocolGuid,
                    (VOID **) &DevicePath
                    );
    if (EFI_ERROR (Status)) {
      continue;
    }

    break;
  }

  if (Index >= HandleCount) {
    Print (L"Failed to find boot device.\n");
    return EFI_DEVICE_ERROR;
  }

  if (HandleBuffer != NULL) {
    FreePool (HandleBuffer);
    HandleBuffer = NULL;
  }

  mDiskDevicePath = DuplicateDevicePath (DevicePath);

  Print (L"%s\n", ConvertDevicePathToText (mDiskDevicePath, FALSE, FALSE));

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
InitDiskPartition (
  VOID
  )
{
  EFI_STATUS                         Status;
  EFI_DEVICE_PATH_PROTOCOL           *FlashDevicePath;
  EFI_DEVICE_PATH_PROTOCOL           *FlashDevicePathDup;
  EFI_DEVICE_PATH_PROTOCOL           *DevicePath;
  EFI_DEVICE_PATH_PROTOCOL           *NextNode;
  HARDDRIVE_DEVICE_PATH              *PartitionNode;
  UINTN                               NumHandles;
  EFI_HANDLE                         *AllHandles;
  UINTN                               LoopIndex;
  EFI_HANDLE                          FlashHandle;
  EFI_BLOCK_IO_PROTOCOL              *FlashBlockIo;
  EFI_PARTITION_ENTRY                *PartitionEntries;
  PARTITION_LIST                     *Entry;


  InitializeListHead (&mPartitionListHead);

  FlashDevicePath    = DuplicateDevicePath (mDiskDevicePath);
  FlashDevicePathDup = DuplicateDevicePath (mDiskDevicePath);

  Status = gBS->LocateDevicePath (&gEfiBlockIoProtocolGuid, &FlashDevicePathDup, &FlashHandle);
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "Warning: Couldn't locate disk device (status: %r)\n", Status));
    // Failing to locate partitions should not prevent to do other actions
    return EFI_SUCCESS;
  }

  Status = gBS->OpenProtocol (
                  FlashHandle,
                  &gEfiBlockIoProtocolGuid,
                  (VOID **) &FlashBlockIo,
                  gImageHandle,
                  NULL,
                  EFI_OPEN_PROTOCOL_GET_PROTOCOL
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "Couldn't open disk device (status: %r)\n", Status));
    return EFI_DEVICE_ERROR;
  }

  // Read the GPT partition entry array into memory so we can get the partition names
  Status = ReadPartitionEntries (FlashBlockIo, &PartitionEntries);
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "Warning: Failed to read partitions from disk device (status: %r)\n", Status));
    // Failing to locate partitions should not prevent to do other actions
    return EFI_SUCCESS;
  }

  // Get every Block IO protocol instance installed in the system
  Status = gBS->LocateHandleBuffer (
                  ByProtocol,
                  &gEfiBlockIoProtocolGuid,
                  NULL,
                  &NumHandles,
                  &AllHandles
                  );
  ASSERT_EFI_ERROR (Status);

  // Filter out handles that aren't children of the flash device
  for (LoopIndex = 0; LoopIndex < NumHandles; LoopIndex++) {
    // Get the device path for the handle
    Status = gBS->OpenProtocol (
                    AllHandles[LoopIndex],
                    &gEfiDevicePathProtocolGuid,
                    (VOID **) &DevicePath,
                    gImageHandle,
                    NULL,
                    EFI_OPEN_PROTOCOL_GET_PROTOCOL
                    );
    ASSERT_EFI_ERROR (Status);

    // Check if it is a sub-device of the flash device
    if (!CompareMem (DevicePath, FlashDevicePath, FLASH_DEVICE_PATH_SIZE (FlashDevicePath))) {
      // Device path starts with path of flash device. Check it isn't the flash
      // device itself.
      NextNode = NextDevicePathNode (DevicePath);
      while (!IsDevicePathEndType (NextNode)) {
        if (NextNode->Type == MEDIA_DEVICE_PATH &&
            NextNode->SubType == MEDIA_HARDDRIVE_DP) {
              break;
            }
        NextNode = NextDevicePathNode (NextNode);
      }

      if (IsDevicePathEndType (NextNode)) {
        continue;
      }

      // Assert that this device path node represents a partition.
      ASSERT (NextNode->Type == MEDIA_DEVICE_PATH &&
              NextNode->SubType == MEDIA_HARDDRIVE_DP);

      PartitionNode = (HARDDRIVE_DEVICE_PATH *) NextNode;

      // Assert that the partition type is GPT. ReadPartitionEntries checks for
      // presence of a GPT, so we should never find MBR partitions.
      // ("MBRType" is a misnomer - this field is actually called "Partition
      //  Format")
      ASSERT (PartitionNode->MBRType == MBR_TYPE_EFI_PARTITION_TABLE_HEADER);

      // The firmware may install a handle for "partition 0", representing the
      // whole device. Ignore it.
      if (PartitionNode->PartitionNumber == 0) {
        continue;
      }

      //
      // Add the partition handle to the list
      //

      // Create entry
      Entry = AllocatePool (sizeof (PARTITION_LIST));
      if (Entry == NULL) {
        Status = EFI_OUT_OF_RESOURCES;
        FreePartitionList ();
        goto Exit;
      }

      // Copy handle and partition name
      Entry->PartitionHandle = AllHandles[LoopIndex];
      CopyMem (
        Entry->PartitionName,
        PartitionEntries[PartitionNode->PartitionNumber - 1].PartitionName, // Partition numbers start from 1.
        PARTITION_NAME_MAX_LENGTH
        );

      Print (L"BLK%d: %s\n", PartitionNode->PartitionNumber, PartitionEntries[PartitionNode->PartitionNumber - 1].PartitionName);
      Print (L"%s\n\n", ConvertDevicePathToText (DevicePath, FALSE, FALSE));

      InsertTailList (&mPartitionListHead, &Entry->Link);

      // Print a debug message if the partition label is empty or looks like
      // garbage.
      if (!IS_ALPHA (Entry->PartitionName[0])) {
        DEBUG ((EFI_D_ERROR,
          "Warning: Partition %d doesn't seem to have a GPT partition label. "
          "You won't be able to flash it.\n",
          PartitionNode->PartitionNumber
          ));
      }
    }
  }

Exit:
  FreePool (PartitionEntries);
  FreePool (FlashDevicePath);
  FreePool (AllHandles);
  return Status;
}

EFI_STATUS
EFIAPI
OpenPartition (
  IN CHAR8                   *PartitionName,
  OUT EFI_BLOCK_IO_PROTOCOL  **BlockIo,
  OUT EFI_DISK_IO_PROTOCOL   **DiskIo
  )
{
  EFI_STATUS               Status;
  PARTITION_LIST           *Entry;
  CHAR16                   PartitionNameUnicode[60];
  BOOLEAN                  PartitionFound;

  AsciiStrToUnicodeStrS (PartitionName, PartitionNameUnicode, ARRAY_SIZE (PartitionNameUnicode));

  PartitionFound = FALSE;
  Entry = (PARTITION_LIST *) GetFirstNode (&(mPartitionListHead));
  while (!IsNull (&mPartitionListHead, &Entry->Link)) {
    // Search the partition list for the partition named by PartitionName
    if (StrCmp (Entry->PartitionName, PartitionNameUnicode) == 0) {
      PartitionFound = TRUE;
      break;
    }

    Entry = (PARTITION_LIST *) GetNextNode (&mPartitionListHead, &(Entry)->Link);
  }

  if (!PartitionFound) {
    return EFI_NOT_FOUND;
  }

  Status = gBS->OpenProtocol (
                  Entry->PartitionHandle,
                  &gEfiBlockIoProtocolGuid,
                  (VOID **) BlockIo,
                  gImageHandle,
                  NULL,
                  EFI_OPEN_PROTOCOL_GET_PROTOCOL
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "Couldn't open Block IO for flash: %r\n", Status));
    return Status;
  }

  Status = gBS->OpenProtocol (
                  Entry->PartitionHandle,
                  &gEfiDiskIoProtocolGuid,
                  (VOID **) DiskIo,
                  gImageHandle,
                  NULL,
                  EFI_OPEN_PROTOCOL_GET_PROTOCOL
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "Couldn't open Disk IO for flash: %r\n", Status));
    return Status;
  }

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
ReadPartition (
  IN CHAR8         *PartitionName,
  IN UINTN         Offset,
  OUT VOID         *Buffer,
  IN UINTN         BufferSize
  )
{
  EFI_STATUS               Status;
  EFI_BLOCK_IO_PROTOCOL    *BlockIo;
  EFI_DISK_IO_PROTOCOL     *DiskIo;
  UINT32                   MediaId;

  Status = OpenPartition (PartitionName, &BlockIo, &DiskIo);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  MediaId = BlockIo->Media->MediaId;

  Status = DiskIo->ReadDisk (DiskIo, MediaId, Offset, BufferSize, Buffer);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  BlockIo->FlushBlocks(BlockIo);

  return EFI_SUCCESS;
}

EFI_STATUS
WritePartition (
  IN CHAR8         *PartitionName,
  IN UINTN         Offset,
  IN VOID          *Buffer,
  IN UINTN         BufferSize
  )
{
  EFI_STATUS               Status;
  EFI_BLOCK_IO_PROTOCOL    *BlockIo;
  EFI_DISK_IO_PROTOCOL     *DiskIo;
  UINT32                   MediaId;

  Status = OpenPartition (PartitionName, &BlockIo, &DiskIo);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  MediaId = BlockIo->Media->MediaId;

  Status = DiskIo->WriteDisk (DiskIo, MediaId, Offset, BufferSize, Buffer);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  BlockIo->FlushBlocks(BlockIo);

  return EFI_SUCCESS;
}

VOID
EFIAPI
DumpHex (
  IN UINTN        Indent,
  IN UINTN        Offset,
  IN UINTN        DataSize,
  IN VOID         *UserData
  )
{
  UINT8 *Data;

  CHAR8 Val[50];

  CHAR8 Str[20];

  UINT8 TempByte;
  UINTN Size;
  UINTN Index;

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
    Print(L"%*a%08X: %-48a *%a*\r\n", Indent, "", Offset, Val, Str);

    Data += Size;
    Offset += Size;
    DataSize -= Size;
  }
}

VOID
EFIAPI
ShowHelpInfo(
  VOID
  )
{
  Print (L"Help info:\n");
  Print (L"  DiskBlock.efi read partname offset size\n\n");
}

/**
UEFI application entry point which has an interface similar to a
standard C main function.

The ShellCEntryLib library instance wrappers the actual UEFI application
entry point and calls this ShellAppMain function.

@param  ImageHandle  The image handle of the UEFI Application.
@param  SystemTable  A pointer to the EFI System Table.

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
  EFI_STATUS                Status;
  CHAR8                     PartitionName[18];
  UINTN                     Offset;
  VOID                      *Buffer;
  UINTN                     BufferSize;

  if (Argc == 1) {
    ShowHelpInfo ();
    return EFI_INVALID_PARAMETER;
  }

  Status = InitDevicePath ();
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = InitDiskPartition ();
  if (EFI_ERROR (Status)) {
    return Status;
  }

  if (Argc == 5) {
    if ((!StrCmp (Argv[1], L"read")) || (!StrCmp (Argv[1], L"READ"))) {
      UnicodeStrToAsciiStrS (Argv[2], PartitionName, ARRAY_SIZE (PartitionName));
      Offset     = StrHexToUintn (Argv[3]);
      BufferSize = StrHexToUintn (Argv[4]);

      Buffer = AllocatePool (BufferSize);
      if (Buffer == NULL) {
        return EFI_OUT_OF_RESOURCES;
      }

      Status = ReadPartition (PartitionName, Offset, Buffer, BufferSize);
      if (EFI_ERROR (Status)) {
        Print (L"Read partition %s failed.\n", PartitionName);
        return Status;
      }

      DumpHex (2, 0, BufferSize, Buffer);

      if (Buffer != NULL) {
        FreePool (Buffer);
        Buffer = NULL;
      }
    }
  }

  return 0;
}

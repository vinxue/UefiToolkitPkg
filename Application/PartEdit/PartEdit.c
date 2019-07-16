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

#include "PartEdit.h"

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

typedef struct {
  EFI_DEVICE_PATH_PROTOCOL  *ParentDevicePath;
  EFI_BLOCK_IO_PROTOCOL     *BlockIo;
  EFI_DISK_IO_PROTOCOL      *DiskIo;
} PARTITON_DATA;

STATIC CONST CHAR8 Hex[] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};

LIST_ENTRY                  mPartitionListHead;
EFI_DEVICE_PATH_PROTOCOL    *mDiskDevicePath;
PARTITON_DATA               *mPartData;


//
// Sparse format support
//
#define SPARSE_HEADER_MAGIC         0xED26FF3A
#define CHUNK_TYPE_RAW              0xCAC1
#define CHUNK_TYPE_FILL             0xCAC2
#define CHUNK_TYPE_DONT_CARE        0xCAC3
#define CHUNK_TYPE_CRC32            0xCAC4

#define FILL_BUF_SIZE               (16 * 1024 * 1024)
#define SPARSE_BLOCK_SIZE           4096

typedef struct {
  UINT32       Magic;
  UINT16       MajorVersion;
  UINT16       MinorVersion;
  UINT16       FileHeaderSize;
  UINT16       ChunkHeaderSize;
  UINT32       BlockSize;
  UINT32       TotalBlocks;
  UINT32       TotalChunks;
  UINT32       ImageChecksum;
} SPARSE_HEADER;

typedef struct {
  UINT16       ChunkType;
  UINT16       Reserved1;
  UINT32       ChunkSize;
  UINT32       TotalSize;
} CHUNK_HEADER;

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

  FileBuffer = AllocateZeroPool (FileSize);
  if (FileBuffer == NULL) {
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
    if (Buffer != NULL) {
      FreePool (Buffer);
    }
    return Status;
  }

  ShellCloseFile (&FileHandle);

  *BufferSize = FileSize;
  *Buffer     = FileBuffer;

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
SaveFileToDisk (
  IN  CHAR16              *FileName,
  IN  UINTN               BufferSize,
  IN  VOID                *Buffer
  )
{
  EFI_STATUS           Status;
  SHELL_FILE_HANDLE    FileHandle;

  Status = ShellOpenFileByName (FileName, &FileHandle, EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE | EFI_FILE_MODE_CREATE, 0);
  if (EFI_ERROR (Status)) {
    Print (L"Open file failed: %r\n", Status);
    return Status;
  }

  Status = ShellWriteFile (FileHandle, &BufferSize, Buffer);
  if (EFI_ERROR (Status)) {
    Print (L"Write file failed: %r\n", Status);
    ShellCloseFile (&FileHandle);
    return Status;
  }

  ShellCloseFile (&FileHandle);

  return EFI_SUCCESS;
}

BOOLEAN
EFIAPI
IsUsbDevice (
  IN EFI_DEVICE_PATH_PROTOCOL   *DevicePath
  )
{
  EFI_DEVICE_PATH_PROTOCOL  *TempDevicePath;
  BOOLEAN                   Match;

  if (DevicePath == NULL) {
    return FALSE;
  }

  Match = FALSE;

  //
  // Search for USB device path node.
  //
  TempDevicePath = DevicePath;
  while (!IsDevicePathEnd (TempDevicePath)) {
    if ((DevicePathType (TempDevicePath) == MESSAGING_DEVICE_PATH) &&
        ((DevicePathSubType (TempDevicePath) == MSG_USB_DP))) {
      Match = TRUE;
    }
    TempDevicePath = NextDevicePathNode (TempDevicePath);
  }

  return Match;
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
  EFI_DISK_IO_PROTOCOL      *DiskIo;
  EFI_DEVICE_PATH_PROTOCOL  *DevicePath;

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

    Status = gBS->HandleProtocol (
                    HandleBuffer[Index],
                    &gEfiDiskIoProtocolGuid,
                    (VOID **) &DiskIo
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

    if (IsUsbDevice (DevicePath)) {
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

  mPartData->BlockIo          = BlockIo;
  mPartData->DiskIo           = DiskIo;
  mPartData->ParentDevicePath = mDiskDevicePath;

  return EFI_SUCCESS;
}

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
  UINTN                              NumHandles;
  EFI_HANDLE                         *AllHandles;
  UINTN                              LoopIndex;
  EFI_HANDLE                         FlashHandle;
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
        PARTITION_NAME_MAX_LENGTH * 2
        );

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
    DEBUG ((EFI_D_ERROR, "Couldn't open Block IO for disk: %r\n", Status));
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
    DEBUG ((EFI_D_ERROR, "Couldn't open Disk IO for disk: %r\n", Status));
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
  UINTN                    PartitionSize;

  Status = OpenPartition (PartitionName, &BlockIo, &DiskIo);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  //
  // Check read partition size will fit on device.
  //
  PartitionSize = (BlockIo->Media->LastBlock + 1) * BlockIo->Media->BlockSize;
  if (PartitionSize < BufferSize) {
    DEBUG ((EFI_D_ERROR, "Partition not big enough.\n"));
    DEBUG ((EFI_D_ERROR, "Partition Size: %d\nImage Size: %d\n",  PartitionSize, BufferSize));
    return EFI_VOLUME_FULL;
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
  UINTN                    PartitionSize;

  Status = OpenPartition (PartitionName, &BlockIo, &DiskIo);
  if (EFI_ERROR (Status)) {

    return Status;
  }

  //
  // Check write partition size will fit on device.
  //
  PartitionSize = (BlockIo->Media->LastBlock + 1) * BlockIo->Media->BlockSize;
  if (PartitionSize < BufferSize) {
    DEBUG ((EFI_D_ERROR, "Partition not big enough.\n"));
    DEBUG ((EFI_D_ERROR, "Partition Size: %d\nImage Size: %d\n",  PartitionSize, BufferSize));
    return EFI_VOLUME_FULL;
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
DumpParentDevice (
  VOID
  )
{
  EFI_STATUS                 Status;
  PARTITION_LIST             *Entry;
  PARTITION_LIST             *NextEntry;
  EFI_DEVICE_PATH_PROTOCOL   *DevicePath;

  Entry = (PARTITION_LIST *) GetFirstNode (&mPartitionListHead);
  while (!IsNull (&mPartitionListHead, &Entry->Link)) {
    Status = gBS->OpenProtocol (
                    Entry->PartitionHandle,
                    &gEfiDevicePathProtocolGuid,
                    (VOID **) &DevicePath,
                    gImageHandle,
                    NULL,
                    EFI_OPEN_PROTOCOL_GET_PROTOCOL
                    );
    if (EFI_ERROR (Status)) {
      Print (L"Get device path failed: %r\n", Status);
    }

    Print (L"PartitionName: %s\n", Entry->PartitionName);
    Print (L"%s\n\n", ConvertDevicePathToText (DevicePath, FALSE, FALSE));

    NextEntry = (PARTITION_LIST *) GetNextNode (&mPartitionListHead, &Entry->Link);
    Entry = NextEntry;
  }

  Print (L"%s\n", ConvertDevicePathToText (mPartData->ParentDevicePath, FALSE, FALSE));
  Print (L"MediaId         : %d\n", mPartData->BlockIo->Media->MediaId);
  Print (L"RemovableMedia  : %d\n", mPartData->BlockIo->Media->RemovableMedia);
  Print (L"MediaPresent    : %d\n", mPartData->BlockIo->Media->MediaPresent);
  Print (L"LogicalPartition: %d\n", mPartData->BlockIo->Media->LogicalPartition);
  Print (L"ReadOnly        : %d\n", mPartData->BlockIo->Media->ReadOnly);
  Print (L"WriteCaching    : %d\n", mPartData->BlockIo->Media->WriteCaching);
  Print (L"BlockSize       : 0x%x\n", mPartData->BlockIo->Media->BlockSize);
  Print (L"IoAlign         : 0x%x\n", mPartData->BlockIo->Media->IoAlign);
  Print (L"LastBlock       : 0x%x\n", mPartData->BlockIo->Media->LastBlock);
  Print (L"LowestAlignedLba: 0x%x\n", mPartData->BlockIo->Media->LowestAlignedLba);
  Print (L"LogicalBlocksPerPhysicalBlock   : 0x%x\n", mPartData->BlockIo->Media->LogicalBlocksPerPhysicalBlock);
  Print (L"OptimalTransferLengthGranularity: 0x%x\n", mPartData->BlockIo->Media->OptimalTransferLengthGranularity);
}

EFI_STATUS
EFIAPI
FlashSparseImage (
  IN CHAR8         *PartitionName,
  IN SPARSE_HEADER *SparseHeader
  )
{
  EFI_STATUS        Status;
  UINTN             Chunk;
  UINTN             Offset;
  UINTN             Left;
  UINTN             Count;
  UINTN             FillBufSize;
  UINT8             *Image;
  CHUNK_HEADER      *ChunkHeader;
  VOID              *FillBuf;

  Status = EFI_SUCCESS;
  Offset = 0;

  Image = (UINT8 *)SparseHeader;
  Image += SparseHeader->FileHeaderSize;

  //
  // Allocate the fill buffer
  //
  FillBufSize = FILL_BUF_SIZE;
  FillBuf = AllocatePool (FillBufSize);
  if (FillBuf == NULL) {
    DEBUG ((DEBUG_ERROR, "Fail to allocate the fill buffer.\n"));
    return EFI_OUT_OF_RESOURCES;
  }

  for (Chunk = 0; Chunk < SparseHeader->TotalChunks; Chunk++) {
    ChunkHeader = (CHUNK_HEADER *)Image;
    DEBUG ((DEBUG_INFO,
      "Chunk #%d - Type: 0x%x Size: %d TotalSize: 0x%lx Offset 0x%lx\n",
      (Chunk + 1),
      ChunkHeader->ChunkType,
      ChunkHeader->ChunkSize,
      ChunkHeader->TotalSize,
      Offset
      ));

    Image += sizeof (CHUNK_HEADER);

    switch (ChunkHeader->ChunkType) {
    case CHUNK_TYPE_RAW:
      Status = WritePartition (
                 PartitionName,
                 Offset,
                 Image,
                 ChunkHeader->ChunkSize * SparseHeader->BlockSize
                 );
      if (EFI_ERROR (Status)) {
        FreePool (FillBuf);
        return Status;
      }
      Image += (UINTN)ChunkHeader->ChunkSize * SparseHeader->BlockSize;
      Offset += (UINTN)ChunkHeader->ChunkSize * SparseHeader->BlockSize;
      break;
    case CHUNK_TYPE_FILL:
      Left = (UINTN)ChunkHeader->ChunkSize * SparseHeader->BlockSize;
      while (Left > 0) {
        if (Left > FILL_BUF_SIZE) {
          Count = FILL_BUF_SIZE;
        } else {
          Count = Left;
        }
        SetMem32 (FillBuf, Count, *(UINT32 *)Image);
        Status = WritePartition (
                 PartitionName,
                 Offset,
                 FillBuf,
                 Count
                 );
        if (EFI_ERROR (Status)) {
          FreePool (FillBuf);
          return Status;
        }
        Offset += Count;
        Left = Left - Count;
      }
      Image += sizeof (UINT32);
      break;
    case CHUNK_TYPE_DONT_CARE:
      Offset += (UINTN)ChunkHeader->ChunkSize * SparseHeader->BlockSize;
      break;
    default:
      Print (L"Unsupported Chunk Type:0x%x\n", ChunkHeader->ChunkType);
      break;
    }
  }

  FreePool (FillBuf);
  return Status;
}

EFI_STATUS
EFIAPI
FlashPartition (
  IN CHAR8     *PartitionName,
  IN UINTN     BufferSize,
  IN VOID      *Buffer
  )
{
  EFI_STATUS          Status;
  SPARSE_HEADER       *SparseHeader;

  if (Buffer == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  SparseHeader = (SPARSE_HEADER *) Buffer;
  if (SparseHeader->Magic == SPARSE_HEADER_MAGIC) {
    DEBUG ((DEBUG_INFO, "Sparse Magic: 0x%x\n", SparseHeader->Magic));
    DEBUG ((DEBUG_INFO, "Major: %d, Minor: %d\n", SparseHeader->MajorVersion, SparseHeader->MinorVersion));
    DEBUG ((DEBUG_INFO, "FileHeaderSize: %d, ChunkHeaderSize: %d\n", SparseHeader->FileHeaderSize, SparseHeader->ChunkHeaderSize));
    DEBUG ((DEBUG_INFO, "BlockSize: %d, TotalBlocks: %d\n", SparseHeader->BlockSize, SparseHeader->TotalBlocks));
    DEBUG ((DEBUG_INFO, "TotalChunks: %d, ImageChecksum: %d\n", SparseHeader->TotalChunks, SparseHeader->ImageChecksum));

    if (SparseHeader->MajorVersion != 1) {
      DEBUG ((DEBUG_ERROR, "Sparse image version %d.%d not supported.\n", SparseHeader->MajorVersion, SparseHeader->MinorVersion));
      return EFI_INVALID_PARAMETER;
    }
    Status = FlashSparseImage (PartitionName, SparseHeader);
  } else {
    Status = WritePartition (
               PartitionName,
               0,
               Buffer,
               BufferSize
               );
  }

  return Status;
}

EFI_STATUS
EFIAPI
ErasePartition (
  IN CHAR8                  *PartitionName
  )
{
  EFI_STATUS               Status;
  EFI_BLOCK_IO_PROTOCOL    *BlockIo;
  EFI_DISK_IO_PROTOCOL     *DiskIo;
  UINT32                   MediaId;
  UINTN                    PartitionSize;
  VOID                     *FillBuffer;
  UINTN                    FillBufferSize;
  UINTN                    Offset;
  UINTN                    Count;

  Status = OpenPartition (PartitionName, &BlockIo, &DiskIo);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  //
  // Read partition size.
  //
  PartitionSize = (BlockIo->Media->LastBlock + 1) * BlockIo->Media->BlockSize;

  MediaId = BlockIo->Media->MediaId;

  //
  // Allocate the fill buffer
  //
  FillBufferSize = SIZE_64MB;
  FillBuffer = AllocateZeroPool (FillBufferSize);
  if (FillBuffer == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  Offset = 0;

  while (PartitionSize > 0) {
    if (PartitionSize > SIZE_64MB) {
      Count = SIZE_64MB;
    } else {
      Count = PartitionSize;
    }

    Status = BlockIo->WriteBlocks (
                        BlockIo,
                        MediaId,
                        Offset,
                        PartitionSize,
                        FillBuffer
                        );
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "Failed to write disk: %r. Offset: 0x%lx.\n", Status, Offset));
      if (FillBuffer != NULL) {
        FreePool (FillBuffer);
      }
      return Status;
    }

    PartitionSize -= Count;
    Offset        += Count / (BlockIo->Media->BlockSize);
  }

  BlockIo->FlushBlocks (BlockIo);

  if (FillBuffer != NULL) {
    FreePool (FillBuffer);
  }

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
EraseGptTable (
  IN PARTITON_DATA          *PartData
  )
{
  EFI_STATUS            Status;
  VOID                  *Buffer;
  UINTN                 BufferSize;
  UINT32                BlockSize;

  if (PartData == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  BlockSize = PartData->BlockIo->Media->BlockSize;

  BufferSize = 2 * BlockSize + MAX_GPT_ENTRIES * GPT_ENTRY_SIZE;
  Buffer = AllocateZeroPool (BufferSize);
  if (Buffer == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  Status = PartData->DiskIo->WriteDisk (
             PartData->DiskIo,
             PartData->BlockIo->Media->MediaId,
             0,
             BufferSize,
             Buffer
             );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  if (Buffer != NULL) {
    FreePool (Buffer);
    Buffer = NULL;
  }

  return Status;
}

EFI_STATUS
EFIAPI
SaveDiskData (
  IN PARTITON_DATA          *PartData,
  IN CHAR16                 *FileName,
  IN UINTN                  Offset,
  IN UINTN                  BufferSize
  )
{
  EFI_STATUS          Status;
  VOID                *Buffer;

  if (PartData == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  Buffer = AllocateZeroPool (BufferSize);
  if (Buffer == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  Status = PartData->DiskIo->ReadDisk (
             PartData->DiskIo,
             PartData->BlockIo->Media->MediaId,
             Offset,
             BufferSize,
             Buffer
             );
  if (EFI_ERROR (Status)) {
    if (Buffer != NULL) {
      FreePool (Buffer);
      Buffer = NULL;
    }
    return Status;
  }

  Status = SaveFileToDisk (FileName, BufferSize, Buffer);
  if (EFI_ERROR (Status)) {
    if (Buffer != NULL) {
      FreePool (Buffer);
      Buffer = NULL;
    }
    return Status;
  }

  if (Buffer != NULL) {
    FreePool (Buffer);
    Buffer = NULL;
  }

  return Status;
}

EFI_STATUS
EFIAPI
WriteDataToDisk (
  IN PARTITON_DATA          *PartData,
  IN CHAR16                 *FileName,
  IN UINTN                  Offset,
  IN UINTN                  Size
  )
{
  EFI_STATUS             Status;
  VOID                   *Buffer;
  UINTN                  BufferSize;
  UINTN                  TempSize;

  TempSize = Size;

  Status = ReadFileFromDisk (FileName, &BufferSize, &Buffer);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  if (Size == 0) {
    TempSize = BufferSize;
  }

  Status = PartData->DiskIo->WriteDisk (
             PartData->DiskIo,
             PartData->BlockIo->Media->MediaId,
             Offset,
             TempSize,
             Buffer
             );
  if (EFI_ERROR (Status)) {
    if (Buffer != NULL) {
      FreePool (Buffer);
      Buffer = NULL;
    }
    return Status;
  }

  PartData->BlockIo->FlushBlocks (PartData->BlockIo);

  if (Buffer != NULL) {
    FreePool (Buffer);
    Buffer = NULL;
  }

  return EFI_SUCCESS;
}

VOID
EFIAPI
ShowHelpInfo (
  VOID
  )
{
  Print (L"\nUEFI Partition Editor. Version: 1.1.0.0.\n");
  Print (L"Copyright (c) 2019 Gavin Xue. All rights reserved.\n\n");
  Print (L"Help info:\n");
  Print (L"  PartEdit.efi -d (Dump parent disk info)\n");
  Print (L"  PartEdit.efi read partname offset size\n");
  Print (L"  PartEdit.efi flash partname filename\n");
  Print (L"  PartEdit.efi erase partname\n");
  Print (L"  PartEdit.efi reset (Erase GPT table)\n");
  Print (L"  PartEdit.efi save offset size filename\n");
  Print (L"  PartEdit.efi write offset size filename\n\n");
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
  EFI_STATUS                Status;
  CHAR8                     PartitionName[18];
  UINTN                     Offset;
  VOID                      *Buffer;
  UINTN                     BufferSize;

  if (Argc == 1) {
    ShowHelpInfo ();
    return EFI_INVALID_PARAMETER;
  }

  mPartData = AllocateZeroPool (sizeof (PARTITON_DATA));
  if (mPartData == NULL) {
    DEBUG ((DEBUG_ERROR, "Fail to allocate the partition data.\n"));
    return EFI_OUT_OF_RESOURCES;
  }

  Status = InitDevicePath ();
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = InitDiskPartition ();
  if (EFI_ERROR (Status)) {
    return Status;
  }

  if (Argc == 2) {
    //
    // Dump parent disk information.
    //
    if ((!StrCmp (Argv[1], L"-d")) || (!StrCmp (Argv[1], L"/d"))) {
      DumpParentDevice ();
      return EFI_SUCCESS;
    }

    //
    // Erase GPT table.
    //
    if ((!StrCmp (Argv[1], L"reset")) || (!StrCmp (Argv[1], L"RESET"))) {
      Status = EraseGptTable (mPartData);
      if (EFI_ERROR (Status)) {
        Print (L"Erase GPT table failed: %r\n", Status);
        return Status;
      }

      Print (L"Erase GPT table passed.\n");
      return EFI_SUCCESS;
    }

  }

   if (Argc == 3) {
    //
    // Erase a given partition
    //
    if ((!StrCmp (Argv[1], L"erase")) || (!StrCmp (Argv[1], L"ERASE"))) {
      UnicodeStrToAsciiStrS (Argv[2], PartitionName, ARRAY_SIZE (PartitionName));
      Status = ErasePartition (PartitionName);
      if (EFI_ERROR (Status)) {
        Print (L"Erase partition: %a failed. %r\n", PartitionName, Status);
        return Status;
      }
      Print (L"Erase partition: %a passed.\n", PartitionName);
      return EFI_SUCCESS;
    }

   }

  if (Argc == 4) {
    //
    // Flash a binary to a partition
    //
    if ((!StrCmp (Argv[1], L"flash")) || (!StrCmp (Argv[1], L"FLASH"))) {
      UnicodeStrToAsciiStrS (Argv[2], PartitionName, ARRAY_SIZE (PartitionName));

      Status = ReadFileFromDisk (Argv[3], &BufferSize, &Buffer);
      if (EFI_ERROR (Status)) {
        Print (L"Read flash file failed. %r\n", Status);
        return Status;
      }

      Status = FlashPartition (PartitionName, BufferSize, Buffer);
      if (EFI_ERROR (Status)) {
        Print (L"Flash partition %a failed: %r\n", PartitionName, Status);
      } else {
        Print (L"Flash partition %a passed: %r\n", PartitionName, Status);
      }

      if (Buffer != NULL) {
        FreePool (Buffer);
        Buffer = NULL;
      }

      if (mPartData != NULL) {
        FreePool (mPartData);
        mPartData = NULL;
      }

      return Status;
    }
  }

  if (Argc == 5) {
    //
    // Read data from a given partition name.
    //
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

      return EFI_SUCCESS;
    }

    //
    // Save disk data to a file.
    //
    if ((!StrCmp (Argv[1], L"save")) || (!StrCmp (Argv[1], L"SAVE"))) {
      Offset     = StrHexToUintn (Argv[2]);
      BufferSize = StrHexToUintn (Argv[3]);

      Status = SaveDiskData (mPartData, Argv[4], Offset, BufferSize);
      if (EFI_ERROR (Status)) {
        Print (L"Save disk data failed. %r\n", Status);
        return Status;
      }

      Print (L"Save disk data passed.\n");

      return EFI_SUCCESS;
    }

    //
    // Write a binary to disk
    //
    if ((!StrCmp (Argv[1], L"write")) || (!StrCmp (Argv[1], L"WRITE"))) {
      Offset     = StrHexToUintn (Argv[2]);
      BufferSize = StrHexToUintn (Argv[3]);

      Status = WriteDataToDisk (mPartData, Argv[4], Offset, BufferSize);
      if (EFI_ERROR (Status)) {
        Print (L"Write disk data failed. %r\n", Status);
        return Status;
      }

      Print (L"Write disk data passed.\n");

      return EFI_SUCCESS;
    }

  }

  return 0;
}

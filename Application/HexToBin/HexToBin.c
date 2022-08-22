/** @file
  Convert a Intel HEX file to a binary.

  Copyright (c) 2022, Gavin Xue. All rights reserved.<BR>
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
**/

#include "HexToBin.h"


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

  if (!EFI_ERROR (ShellFileExists (FileName))) {
    ShellDeleteFileByName (FileName);
  }

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

/**
  Change char to int value based on Hex.

  @param[in] Char     The input char.

  @return The character's index value.

**/
UINT8
IntelHexCharToHex (
  IN UINT8       *Char
  )
{
  //
  // Change the character to hex
  //
  if (*Char >= '0' && *Char <= '9') {
    return (*Char - '0');
  }

  if (*Char >= 'a' && *Char <= 'f') {
    return (*Char - 'a' + 10);
  }

  if (*Char >= 'A' && *Char <= 'F') {
    return (*Char - 'A' + 10);
  }

  DEBUG ((DEBUG_ERROR, "Invalid character: %c\n", *Char));

  return 0;
}

/**
  Get a byte from character.

  @param[in] Char     The input char.

  @return The character's byte value.

**/
UINT8
IntelHexGetByte (
  IN UINT8       *Char
  )
{
  return ((IntelHexCharToHex (Char) << 4) | IntelHexCharToHex (Char + 1)) & 0xFF;
}

/**
  Get a word from character.

  @param[in] Char     The input char.

  @return The character's word value.

**/
UINT16
IntelHexGetWord (
  IN UINT8       *Char
  )
{
  return ((IntelHexGetByte (Char) << 8) | IntelHexGetByte (Char + 2)) & 0xFFFF;
}

/**
  Copy one line data from buffer data to the line buffer.

  @param[in]      Buffer          Buffer data.
  @param[in]      BufferSize      Buffer Size.
  @param[in, out] LineBuffer      Line buffer to store the found line data.
  @param[in, out] LineSize        On input, size of the input line buffer.
                                  On output, size of the actual line buffer.

  @retval EFI_BUFFER_TOO_SMALL    The size of input line buffer is not enough.
  @retval EFI_SUCCESS             Copy line data into the line buffer.

**/
EFI_STATUS
IntelHexGetLine (
  IN      UINT8                *Buffer,
  IN      UINTN                BufferSize,
  IN OUT  UINT8                *LineBuffer,
  IN OUT  UINTN                *LineSize
  )
{
  UINTN                       Length;
  UINT8                       *PtrBuf;
  UINTN                       PtrEnd;

  PtrBuf = Buffer;
  PtrEnd = (UINTN) Buffer + BufferSize;

  //
  // 0x0D indicates a line break. Otherwise there is no line break
  //
  while ((UINTN) PtrBuf < PtrEnd) {
    if (*PtrBuf == 0x0D || *PtrBuf == 0x0A) {
      break;
    }
    PtrBuf++;
  }

  if ((UINTN) PtrBuf >= (PtrEnd - 1)) {
    //
    // The buffer ends without any line break
    // or it is the last character of the buffer
    //
    Length = BufferSize;
  } else if (*(PtrBuf + 1) == 0x0A) {
    //
    // Further check if a 0x0A follows. If yes, count 0xA
    //
    Length = (UINTN) PtrBuf - (UINTN) Buffer + 2;
  } else {
    Length = (UINTN) PtrBuf - (UINTN) Buffer + 1;
  }

  if (Length > (*LineSize)) {
    *LineSize = Length;
    return EFI_BUFFER_TOO_SMALL;
  }

  SetMem (LineBuffer, *LineSize, 0x0);
  *LineSize = Length;
  CopyMem (LineBuffer, Buffer, Length);

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
ProcessIntelHexFile (
  IN VOID              *DataBuffer,
  IN UINTN             BufferSize,
  IN OUT VOID          **BinBuffer,
  IN OUT UINTN         *BinSize
  )
{
  EFI_STATUS               Status;
  UINT8                    *Source;
  UINT8                    *CurrentPtr;
  UINT8                    *BufferEnd;
  UINT8                    *SourcePtrLine;
  UINT8                    *PtrLine;
  UINTN                    LineLength;
  UINTN                    SourceLength;
  UINTN                    MaxLineLength;
  UINTN                    BinaryFileSize;
  UINTN                    LineNumber;
  UINT8                    ByteCount;
  UINT16                   Address;
  UINT8                    RecordType;
  UINTN                    CheckSum;
  UINTN                    Index;
  UINT8                    *Buffer;
  UINT8                    Data;
  UINTN                    BinLength;

  BufferEnd        = (UINT8 *) ( (UINTN) DataBuffer + BufferSize);
  CurrentPtr       = (UINT8 *) DataBuffer;
  MaxLineLength    = MAX_LINE_LENGTH;
  BinaryFileSize   = 0;
  LineNumber       = 0;

  SourcePtrLine = AllocatePool (MaxLineLength);
  if (SourcePtrLine == NULL) {
    DEBUG ((DEBUG_ERROR, "Allocate resource for line failed.\n"));
    return EFI_OUT_OF_RESOURCES;
  }

  Buffer = AllocatePool (BufferSize);
  if (Buffer == NULL) {
    DEBUG ((DEBUG_ERROR, "Allocate resource for BIN buffer failed.\n"));
    return EFI_OUT_OF_RESOURCES;
  }

  SetMem (Buffer, BufferSize, 0xFF);

  while (CurrentPtr < BufferEnd) {
    Source              = CurrentPtr;
    SourceLength        = (UINTN) BufferEnd - (UINTN) CurrentPtr;
    LineLength          = MaxLineLength;
    CheckSum            = 0;

    Status = IntelHexGetLine (
               (UINT8 *) Source,
               SourceLength,
               (UINT8 *) SourcePtrLine,
               &LineLength
               );
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "IntelHexGetLine() falied: %r\n", Status));
      FreePool (PtrLine);
      FreePool (Buffer);
      return Status;
    }

    PtrLine = SourcePtrLine;

    CurrentPtr = (UINT8 *) ((UINTN) CurrentPtr + LineLength);

    if (*PtrLine != INTEL_HEX_START_CODE) {
      DEBUG ((DEBUG_INFO, "Invalid Hex format on Line: 0x%x\n", LineNumber));
    }

    //
    // Intel HEX format:
    // Start code  |  Byte count  |  Address  |  Record type  |  Data  |  Checksum
    // Byte count, two hex digits (one hex digit pair)
    //
    PtrLine++;
    ByteCount = IntelHexGetByte (PtrLine);
    if (ByteCount == 0) {
      break;
    }
    CheckSum += ByteCount;

    //
    // Address, four hex digits
    //
    PtrLine += 2;
    Address = IntelHexGetWord (PtrLine);

    CheckSum += IntelHexGetByte (PtrLine);
    CheckSum += IntelHexGetByte (PtrLine + 2);

    //
    // Record type, two hex digits
    //
    PtrLine += 4;
    RecordType = IntelHexGetByte (PtrLine);
    CheckSum += RecordType;

    PtrLine += 2;

    switch (RecordType) {
    case RECORD_DATA:
      BinLength = Address;
      for (Index = 0; Index < ByteCount; Index++) {
        Data = IntelHexGetByte (PtrLine);
        CheckSum += Data;
        PtrLine  += 2;

        CopyMem (Buffer + BinLength, &Data, sizeof (UINT8));

        BinLength++;
      }

      //
      // Verify the record
      //
      CheckSum += IntelHexGetByte (PtrLine);
      if (CheckSum & 0xFF) {
        DEBUG ((DEBUG_ERROR, "Checksum error on Line: 0x%x\n", LineNumber));
        Print (L"Checksum error: 0x%x\n", LineNumber);
      }
      break;

    case RECORD_END_OF_FILE:
      DEBUG ((DEBUG_INFO, "End of file on Line: 0x%x\n", LineNumber));
      break;


    case RECORD_EXTENDED_SEGMENT_ADDRESS:
    case RECORD_START_SEGMENT_ADDRESS:
    case RECORD_EXTENDED_LINEAR_ADDRESS:
    case RECORD_START_LINEAR_ADDRESS:
      DEBUG ((DEBUG_INFO, "Record type: 0x%x on Line: 0x%x\n", RecordType, LineNumber));
      break;

    default:
      DEBUG ((DEBUG_INFO, "Invalid Record type: 0x%x on Line: 0x%x\n", RecordType, LineNumber));
      return EFI_UNSUPPORTED;
    }

    LineNumber++;
  }

  if (SourcePtrLine != NULL) {
    FreePool (SourcePtrLine);
  }
  *BinBuffer = Buffer;
  *BinSize   = BinLength;

  DEBUG ((DEBUG_INFO, "BIN file size: 0x%x\n", *BinSize));

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
  Print (L"  HexToBin.efi [HexFile] [BinFile]\n");
  Print (L"  Example: HexToBin.efi IntelHex.hex BinOut.bin\n\n");
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
  VOID                      *HexBuffer;
  UINTN                     HexBufferSize;
  VOID                      *BinBuffer;
  UINTN                     BinBufferSize;

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
  }

  if (Argc == 3) {
    //
    // Read a Intel Hex file
    //
    Status = ReadFileFromDisk (Argv[1], &HexBufferSize, &HexBuffer);
    if (EFI_ERROR (Status)) {
      Print (L"Read Intel Hex file failed. %r\n", Status);
      return Status;
    }

    Status = ProcessIntelHexFile (HexBuffer, HexBufferSize, &BinBuffer, &BinBufferSize);
    if (EFI_ERROR (Status)) {
      Print (L"Process Intel Hex file failed. %r\n", Status);
      return Status;
    }

    Status = SaveFileToDisk (Argv[2], BinBufferSize, BinBuffer);
    if (EFI_ERROR (Status)) {
      Print (L"Write BIN file failed. %r\n", Status);
      return Status;
    }

    if (BinBuffer != NULL) {
      FreePool (BinBuffer);
    }

    return EFI_SUCCESS;
  }

  return 0;
}

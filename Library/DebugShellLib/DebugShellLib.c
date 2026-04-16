/** @file
  Debug Shell Library implementation.
  Interactive serial-port command shell for debugging.

  Copyright (c) 2026, Gavin Xue. All rights reserved.<BR>
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
**/

#include <PiPei.h>
#include <Library/BaseLib.h>
#include <Library/SerialPortLib.h>
#include <Library/DebugShellLib.h>

/*
  Internal constants
*/

#define SHELL_CMD_BUF_SIZE  128
#define SHELL_MAX_ARGS      8

/*
  I/O helpers
*/

/**
  Write a NUL-terminated ASCII string to the serial port.

  @param[in]  Str  NUL-terminated ASCII string.
**/
STATIC VOID
ShellPrint (
  IN CONST CHAR8  *Str
  )
{
  UINTN  Len;

  for (Len = 0; Str[Len] != '\0'; Len++) {
  }

  if (Len > 0) {
    SerialPortWrite ((UINT8 *)Str, Len);
  }
}

/**
  Print Value as "0xXXXXXXXXXXXXXXXX" (always 16 uppercase hex digits).

  @param[in]  Value  64-bit value to print in hex format.
**/
STATIC VOID
ShellPrintHex64 (
  IN UINT64  Value
  )
{
  CHAR8  Buf[19];   /* "0x" + 16 digits + NUL */
  INT32  i;

  Buf[0] = '0';
  Buf[1] = 'x';
  for (i = 15; i >= 0; i--) {
    UINT8  Nibble;

    Nibble         = (UINT8)(Value & 0xF);
    Buf[2 + i]     = (Nibble < 10) ? (CHAR8)('0' + Nibble) : (CHAR8)('A' + Nibble - 10);
    Value        >>= 4;
  }
  Buf[18] = '\0';
  ShellPrint (Buf);
}

/**
  Parse an ASCII hex string (optional "0x"/"0X" prefix) into a UINT64.

  @param[in]  Str    Input ASCII hex string.
  @param[out] Value  Pointer to output UINT64 value.

  @retval TRUE   Parsed successfully; *Value is set.
  @retval FALSE  Empty string or contains non-hex characters.
**/
STATIC BOOLEAN
ShellParseHex64 (
  IN  CONST CHAR8  *Str,
  OUT UINT64       *Value
  )
{
  UINT64  Result;
  UINT8   Nibble;

  if (Str == NULL || Value == NULL) {
    return FALSE;
  }

  if (Str[0] == '0' && (Str[1] == 'x' || Str[1] == 'X')) {
    Str += 2;
  }

  if (Str[0] == '\0') {
    return FALSE;
  }

  Result = 0;
  while (*Str != '\0') {
    if (*Str >= '0' && *Str <= '9') {
      Nibble = (UINT8)(*Str - '0');
    } else if (*Str >= 'a' && *Str <= 'f') {
      Nibble = (UINT8)(*Str - 'a' + 10);
    } else if (*Str >= 'A' && *Str <= 'F') {
      Nibble = (UINT8)(*Str - 'A' + 10);
    } else {
      return FALSE;
    }

    Result = (Result << 4) | Nibble;
    Str++;
  }

  *Value = Result;
  return TRUE;
}

/**
  Read a line from the serial port with local echo and backspace support.
  Blocks until CR or LF is received.

  @param[out] Buf      Destination buffer for the NUL-terminated line.
  @param[in]  BufSize  Size of Buf in bytes (including NUL terminator).
**/
STATIC VOID
ShellReadLine (
  OUT CHAR8  *Buf,
  IN  UINTN  BufSize
  )
{
  UINTN  Pos;
  UINT8  Ch;
  UINT8  Bs[3];

  Pos   = 0;
  Bs[0] = '\b';
  Bs[1] = ' ';
  Bs[2] = '\b';

  while (TRUE) {
    while (!SerialPortPoll ()) {
    }

    SerialPortRead (&Ch, 1);

    if ((Ch == '\r') || (Ch == '\n')) {
      ShellPrint ("\r\n");
      break;
    } else if ((Ch == '\b') || (Ch == 0x7F)) {
      if (Pos > 0) {
        Pos--;
        SerialPortWrite (Bs, 3);
      }
    } else if ((Ch >= 0x20) && (Ch < 0x7F)) {
      if (Pos < BufSize - 1) {
        Buf[Pos++] = (CHAR8)Ch;
        SerialPortWrite (&Ch, 1);
      }
    }
  }

  Buf[Pos] = '\0';
}

/**
  Tokenize Line in-place by replacing whitespace with NUL bytes.
  Argv[] entries point into the modified Line buffer.

  @param[in,out] Line     Input line buffer to tokenize.
  @param[out]    Argv     Array of pointers to tokens.
  @param[in]     MaxArgs  Maximum number of tokens to extract.

  @return Number of tokens found (argc).
**/
STATIC UINTN
ShellTokenize (
  IN OUT CHAR8   *Line,
  OUT    CHAR8   *Argv[],
  IN     UINTN   MaxArgs
  )
{
  UINTN  Argc;

  Argc = 0;
  while ((*Line != '\0') && (Argc < MaxArgs)) {
    while ((*Line == ' ') || (*Line == '\t')) {
      Line++;
    }

    if (*Line == '\0') {
      break;
    }

    Argv[Argc++] = Line;

    while ((*Line != '\0') && (*Line != ' ') && (*Line != '\t')) {
      Line++;
    }

    if (*Line != '\0') {
      *Line++ = '\0';
    }
  }

  return Argc;
}

/*
  Command handlers
*/

/**
  Display help message with available commands.
**/
STATIC VOID
CmdHelp (
  VOID
  )
{
  ShellPrint ("Available commands:\r\n");
  ShellPrint ("  readmsr  <index>         - Read 64-bit MSR (hex index)\r\n");
  ShellPrint ("  writemsr <index> <value> - Write 64-bit MSR (hex index and value)\r\n");
  ShellPrint ("  help                     - Show this message\r\n");
  ShellPrint ("  exit                     - Leave debug shell and continue boot\r\n");
}

/**
  Handle "readmsr" command to read a 64-bit MSR.

  @param[in]  Argc  Argument count.
  @param[in]  Argv  Argument array.
**/
STATIC VOID
CmdReadMsr (
  IN UINTN   Argc,
  IN CHAR8  *Argv[]
  )
{
  UINT64  Index;
  UINT64  Value;

  if (Argc < 2) {
    ShellPrint ("Usage: readmsr <index_hex>\r\n");
    return;
  }

  if ((!ShellParseHex64 (Argv[1], &Index)) || (Index > 0xFFFFFFFFULL)) {
    ShellPrint ("Error: invalid MSR index '");
    ShellPrint (Argv[1]);
    ShellPrint ("'\r\n");
    return;
  }

  Value = AsmReadMsr64 ((UINT32)Index);

  ShellPrint ("MSR[");
  ShellPrintHex64 (Index);
  ShellPrint ("] = ");
  ShellPrintHex64 (Value);
  ShellPrint ("\r\n");
}

/**
  Handle "writemsr" command to write a 64-bit MSR.

  @param[in]  Argc  Argument count.
  @param[in]  Argv  Argument array.
**/
STATIC VOID
CmdWriteMsr (
  IN UINTN   Argc,
  IN CHAR8  *Argv[]
  )
{
  UINT64  Index;
  UINT64  Value;

  if (Argc < 3) {
    ShellPrint ("Usage: writemsr <index_hex> <value_hex>\r\n");
    return;
  }

  if ((!ShellParseHex64 (Argv[1], &Index)) || (Index > 0xFFFFFFFFULL)) {
    ShellPrint ("Error: invalid MSR index '");
    ShellPrint (Argv[1]);
    ShellPrint ("'\r\n");
    return;
  }

  if (!ShellParseHex64 (Argv[2], &Value)) {
    ShellPrint ("Error: invalid value '");
    ShellPrint (Argv[2]);
    ShellPrint ("'\r\n");
    return;
  }

  AsmWriteMsr64 ((UINT32)Index, Value);

  ShellPrint ("MSR[");
  ShellPrintHex64 (Index);
  ShellPrint ("] <- ");
  ShellPrintHex64 (Value);
  ShellPrint (" (written)\r\n");
}

/**
  Dispatch one tokenized command line.

  @param[in]  Line  Command line to process.

  @retval TRUE   The caller should exit the shell loop ("exit" command).
  @retval FALSE  Continue the shell loop.
**/
STATIC BOOLEAN
ShellProcessCommand (
  IN CHAR8  *Line
  )
{
  CHAR8  *Argv[SHELL_MAX_ARGS];
  UINTN   Argc;

  Argc = ShellTokenize (Line, Argv, SHELL_MAX_ARGS);

  if (Argc == 0) {
    return FALSE;
  }

  if (AsciiStrCmp (Argv[0], "help") == 0) {
    CmdHelp ();
  } else if (AsciiStrCmp (Argv[0], "readmsr") == 0) {
    CmdReadMsr (Argc, Argv);
  } else if (AsciiStrCmp (Argv[0], "writemsr") == 0) {
    CmdWriteMsr (Argc, Argv);
  } else if (AsciiStrCmp (Argv[0], "exit") == 0) {
    ShellPrint ("Exiting debug shell...\r\n");
    return TRUE;
  } else {
    ShellPrint ("Unknown command: '");
    ShellPrint (Argv[0]);
    ShellPrint ("'. Type 'help' for available commands.\r\n");
  }

  return FALSE;
}

/*
  Public API
*/

/**
  Run the interactive debug shell over the serial port.

  @param[in]  Prompt  Command prompt string; NULL selects the default "> ".
**/

VOID
EFIAPI
RunDebugShell (
  IN CONST CHAR8  *Prompt  OPTIONAL
  )
{
  CHAR8        CmdBuf[SHELL_CMD_BUF_SIZE];
  CONST CHAR8  *ActivePrompt;

  ActivePrompt = (Prompt != NULL) ? Prompt : "> ";

  ShellPrint ("\r\n*** Debug Shell ***\r\n");
  ShellPrint ("Type 'help' for available commands.\r\n\r\n");

  while (TRUE) {
    ShellPrint (ActivePrompt);
    ShellReadLine (CmdBuf, sizeof (CmdBuf));
    if (ShellProcessCommand (CmdBuf)) {
      break;
    }
  }
}

/** @file
  Debug Shell Library public API.
  Provides an interactive serial-port command shell for use debugging.

  Copyright (c) 2026, Gavin Xue. All rights reserved.<BR>
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
**/

#ifndef DEBUG_SHELL_LIB_H_
#define DEBUG_SHELL_LIB_H_

/**
  Run the interactive debug shell over the serial port.

  Displays a prompt and processes commands line-by-line until the user
  types "exit".  All I/O is performed through SerialPortLib primitives
  so the function is safe to call before permanent memory exists.

  Supported built-in commands:
    readmsr  <index>         - Read a 64-bit MSR (hexadecimal index)
    writemsr <index> <value> - Write a 64-bit MSR (hexadecimal index and value)
    help                     - Print the command list
    exit                     - Return to the caller

  @param[in]  Prompt  Optional NUL-terminated ASCII string used as the
                      command prompt.  Pass NULL to use the default "> ".
**/
VOID
EFIAPI
RunDebugShell (
  IN CONST CHAR8  *Prompt  OPTIONAL
  );

#endif  // DEBUG_SHELL_LIB_H_

/** @file
  A UEFI tool to generate GUID.

  Copyright (c) 2020, Gavin Xue. All rights reserved.<BR>
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
**/

#include "GenGuid.h"

/**
  Initialize a random seed using current time and monotonic count.

  Get current time and monotonic count first. Then initialize a random seed
  based on some basic mathematics operation on the hour, day, minute, second,
  nanosecond and year of the current time and the monotonic count value.

  @return The random seed initialized with current time.

**/
UINT32
EFIAPI
RandomInitSeed (
  VOID
  )
{
  EFI_TIME                  Time;
  UINT32                    Seed;
  UINT64                    MonotonicCount;

  gRT->GetTime (&Time, NULL);
  Seed = (~Time.Hour << 24 | Time.Day << 16 | Time.Minute << 8 | Time.Second);
  Seed ^= Time.Nanosecond;
  Seed ^= Time.Year << 7;

  gBS->GetNextMonotonicCount (&MonotonicCount);
  Seed += (UINT32) MonotonicCount;

  return Seed;
}

/**
  Generate random numbers.

  @param[in, out]  Rand       The buffer to contain random numbers.
  @param[in]       RandLength The length of the Rand buffer.

**/
VOID
GenRandom (
  IN OUT UINT8  *Rand,
  IN     UINTN  RandLength
  )
{
  UINT32  Random;

  while (RandLength > 0) {
    Random  = RANDOM (RandomInitSeed ());
    *Rand++ = (UINT8) (Random);
    RandLength--;
  }
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
  EFI_GUID           Guid;
  UINT64             TempRand[2];

  //
  // Generate GUID (RFC 4122).
  //
  GenRandom ((UINT8 *) TempRand, sizeof (EFI_GUID));
  DEBUG ((DEBUG_INFO, "TempRand[0]: %lx, TempRand[1]: %lx\n", TempRand[0], TempRand[1]));

  Guid.Data1 = (UINT32) TempRand[0];
  Guid.Data2 = (UINT16) RShiftU64 (TempRand[0], 32);
  Guid.Data3 = (((UINT16) RShiftU64 (TempRand[0], 48)) & 0x0FFF) | 0x4000;  // version 4: random generation
  CopyMem (Guid.Data4, &TempRand[1], sizeof (UINT64));
  Guid.Data4[0] = (Guid.Data4[0] & 0x3F) | 0x80;  // Reversion 0b10

  Print (L"GUID: %g\n", &Guid);

  return 0;
}

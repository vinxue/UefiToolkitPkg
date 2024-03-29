/** @file
  A simple UEFI tool for debugging.

  Copyright (c) 2017 - 2022, Gavin Xue. All rights reserved.<BR>
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
**/

#include "UefiTool.h"

EFI_MP_SERVICES_PROTOCOL          *mMpService;
EFI_PROCESSOR_INFORMATION         *mProcessorLocBuf;
UINTN                             mProcessorNum;
UINTN                             mBspIndex;

CHAR16
ToUpper (
  CHAR16  a
  )
{
  if ('a' <= a && a <= 'z') {
    return (CHAR16) (a - 0x20);
  } else {
    return a;
  }
}

VOID
EFIAPI
StrUpr (
  IN OUT CHAR16  *Str
  )
{
  while (*Str) {
    *Str = ToUpper (*Str);
    Str += 1;
  }
}

//
// Bits definition of Command flag list
//
#define OPCODE_RDMSR_BIT                      BIT0    // Read MSR register
#define OPCODE_WRMSR_BIT                      BIT1    // Write MSR register
#define OPCODE_CPUID_BIT                      BIT2    // Read MSR
#define OPCODE_ALLPROCESSOR_BIT               BIT3    // Support run routine on All processors
#define OPCODE_PROCESSOR_INDEX_BIT            BIT4    // Support run routine on a specific AP
#define OPCODE_SGDT_BIT                       BIT5    // Read GDTR
#define OPCODE_CR_BIT                         BIT6    // Read CR registers
#define OPCODE_RMM_BIT                        BIT7    // Read MMIO/IO
#define OPCODE_WMM_BIT                        BIT8    // Write MMIO/IO
#define OPCODE_DUMPMEM_BIT                    BIT9    // Dump memory region to a file
#define OPCODE_UCODE_BIT                      BIT10   // Get CPU Microcode signature/version
#define OPCODE_READ_MEM_TEST_BIT              BIT11
#define OPCODE_WRITE_MEM_TEST_BIT             BIT12

typedef struct {
  // MSR
  UINT32                    MsrIndex;
  UINT64                    MsrValue;

  // CPUID
  UINT32                    CpuIdIndex;
  UINT32                    CpuIdSubIndex;

  UINTN                     ProcessorIndex;

  // MEM / MMIO / IO
  UINTN                     MmAddress;
  UINTN                     MmValue;
  UINTN                     MmWidth;
  UINT8                     MmAccessType;  // 0: Mem / MMIO; 1: IO

  // Dump memory to file
  UINTN                     DumpMemAddress;
  UINTN                     DumpMemSize;
  CHAR16                    DumpMemFile[256];
} UEFI_TOOL_CONTEXT;

UEFI_TOOL_CONTEXT    gUtContext;

VOID
EFIAPI
ShowHelpInfo(
  VOID
  )
{
  Print (L"Help info:\n");
  Print (L"  UefiTool.efi -H\n\n");
  Print (L"Read MSR register:\n");
  Print (L"  UefiTool.efi RDMSR [MSRIndex] [OPTION: -A | -P]\n\n");
  Print (L"Write MSR register:\n");
  Print (L"  UefiTool.efi WRMSR [MSRIndex] [MSRValue]\n\n");
  Print (L"Read CPUID:\n");
  Print (L"  UefiTool.efi CPUID [CPUID_Index] [CPUID_SubIndex]\n\n");
  Print (L"Read GDTR register:\n");
  Print (L"  UefiTool.efi -SGDT\n\n");
  Print (L"Read CR register:\n");
  Print (L"  UefiTool.efi -CR\n\n");
  Print (L"Get Microcode signature:\n");
  Print (L"  UefiTool.efi -MCU\n\n");
  Print (L"Dump memory to file:\n");
  Print (L"  UefiTool.efi DUMPMEM [Address] [Size] [File]\n\n");
}

/**
  Get All processors EFI_CPU_LOCATION in system. LocationBuf is allocated inside the function
  Caller is responsible to free LocationBuf.

  @retval EFI_SUCCESS              Operation completed successfully.
  @retval EFI_UNSUPPORTED          MpService protocol not found.

**/
EFI_STATUS
GetProcessorsCpuLocation (
  VOID
  )
{
  EFI_STATUS                        Status;
  UINTN                             EnabledProcessorNum;
  UINTN                             Index;

  Status = gBS->LocateProtocol (
                  &gEfiMpServiceProtocolGuid,
                  NULL,
                  (VOID **)&mMpService
                  );
  if (EFI_ERROR (Status)) {
    //
    // MP protocol is not installed
    //
    return EFI_UNSUPPORTED;
  }

  Status = mMpService->GetNumberOfProcessors (
                         mMpService,
                         &mProcessorNum,
                         &EnabledProcessorNum
                         );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = gBS->AllocatePool (
                  EfiBootServicesData,
                  sizeof(EFI_PROCESSOR_INFORMATION) * mProcessorNum,
                  (VOID **) &mProcessorLocBuf
                  );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  //
  // Get each processor Location info
  //
  for (Index = 0; Index < mProcessorNum; Index++) {
    Status = mMpService->GetProcessorInfo (
                           mMpService,
                           Index,
                           &mProcessorLocBuf[Index]
                           );
    if (EFI_ERROR (Status)) {
      FreePool(mProcessorLocBuf);
      return Status;
    }
  }

  Status = mMpService->WhoAmI (
                         mMpService,
                         &mBspIndex
                         );
  return Status;
}

VOID
ApUtReadMsr (
  OUT UINT64             *MsrData
  )
{
  *MsrData = AsmReadMsr64 (gUtContext.MsrIndex);
}

VOID
ApUtReadCpuId (
  OUT VOID             *Buffer
  )
{
  UT_CPUID_REGISTER    *CpuidRegister;

  CpuidRegister = (UT_CPUID_REGISTER *) Buffer;
  AsmCpuidEx (gUtContext.CpuIdIndex, gUtContext.CpuIdSubIndex, &CpuidRegister->Eax, &CpuidRegister->Ebx, &CpuidRegister->Ecx, &CpuidRegister->Edx);
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

  Status = ShellOpenFileByName (
             FileName,
             &FileHandle,
             EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE | EFI_FILE_MODE_CREATE,
             0
             );
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
  Get microcode update signature of currently loaded microcode update.

  @return  Microcode signature.

**/
UINT32
GetCurrentMicrocodeSignature (
  VOID
  )
{
  MSR_IA32_BIOS_SIGN_ID_REGISTER   BiosSignIdMsr;

  AsmWriteMsr64 (MSR_IA32_BIOS_SIGN_ID, 0);
  AsmCpuid (CPUID_VERSION_INFO, NULL, NULL, NULL, NULL);
  BiosSignIdMsr.Uint64 = AsmReadMsr64 (MSR_IA32_BIOS_SIGN_ID);

  return BiosSignIdMsr.Bits.MicrocodeUpdateSignature;
}


VOID
ReadMemoryOnEachProcessor (
  OUT UINT64             *MemoryData
  )
{
  *MemoryData = MmioRead64 (gUtContext.MmAddress);
}

VOID
WriteMemoryOnEachProcessor (
  VOID
  )
{
  MmioWrite64 (gUtContext.MmAddress, gUtContext.MmValue);
}

/**
  The function to be run on the designated AP of the system to get CPU Microcode signature.

  @param[in]  Index         The handle index of the AP.

**/
VOID
ApUtGetMicrocodeSignature (
  OUT UINT32             *CurrentMicrocodeSignature
  )
{
  *CurrentMicrocodeSignature = GetCurrentMicrocodeSignature ();
}

EFI_STATUS
EFIAPI
UefiToolRoutine (
  UINT64             Opcode
  )
{
  EFI_STATUS         Status;
  UINT64             MsrData;
  UINTN              Index;
  IA32_DESCRIPTOR    GdtrDesc;
  UINTN              Data;
  UT_CPUID_REGISTER  CpuidRegister;
  UINT32             CurrentMicrocodeSignature;
  UINT64             Data64;

  //
  // Read a MSR register
  //
  if (Opcode == OPCODE_RDMSR_BIT) {
    MsrData = AsmReadMsr64 (gUtContext.MsrIndex);
    Print (L"RDMSR[0x%X]: 0x%016lX\n", gUtContext.MsrIndex, MsrData);
  }

  //
  // Write a MSR register
  //
  if (Opcode == OPCODE_WRMSR_BIT) {
    MsrData = AsmWriteMsr64 (gUtContext.MsrIndex, gUtContext.MsrValue);
    Print (L"WR Data 0x%016lX to MSR[0x%X]\n", gUtContext.MsrValue, gUtContext.MsrIndex);
  }

  //
  // Retrieves CPUID information for BSP
  //
  if (Opcode == OPCODE_CPUID_BIT) {
    AsmCpuidEx (gUtContext.CpuIdIndex, gUtContext.CpuIdSubIndex, &CpuidRegister.Eax, &CpuidRegister.Ebx, &CpuidRegister.Ecx, &CpuidRegister.Edx);
    Print (L"CPUID Index: 0x%X     SubIndex: 0x%X\n", gUtContext.CpuIdIndex, gUtContext.CpuIdSubIndex);
    Print (L"EAX = 0x%08X\n", CpuidRegister.Eax);
    Print (L"EBX = 0x%08X\n", CpuidRegister.Ebx);
    Print (L"ECX = 0x%08X\n", CpuidRegister.Ecx);
    Print (L"EDX = 0x%08X\n", CpuidRegister.Edx);
  }

  //
  // Reads the current Global Descriptor Table Register(GDTR) descriptor
  //
  if (Opcode == OPCODE_SGDT_BIT) {
    AsmReadGdtr (&GdtrDesc);
    Print (L"GDTR Base: 0x%X\n", GdtrDesc.Base);
    Print (L"GDTR Limit: 0x%X\n", GdtrDesc.Limit);
  }

  //
  // Reads the current value of the Control Register (CR0/2/3/4)
  //
  if (Opcode == OPCODE_CR_BIT) {
    Print (L"CR0: 0x%08X\n",  AsmReadCr0 ());
    Print (L"CR2: 0x%08X\n",  AsmReadCr2 ());
    Print (L"CR3: 0x%08X\n",  AsmReadCr3 ());
    Print (L"CR4: 0x%08X\n",  AsmReadCr4 ());
  }

  //
  // Read a MSR register for a specific AP
  //
  if (Opcode == (OPCODE_RDMSR_BIT | OPCODE_PROCESSOR_INDEX_BIT)) {
    Index = gUtContext.ProcessorIndex;
    MsrData = 0;
    if (Index == mBspIndex) {
        MsrData = AsmReadMsr64 (gUtContext.MsrIndex);
    } else {
      mMpService->StartupThisAP (
                    mMpService,
                    (EFI_AP_PROCEDURE) ApUtReadMsr,
                    Index,
                    NULL,
                    0,
                    (UINT64 *) &MsrData,
                    NULL
                    );
    }

    Print (L"RDMSR[0x%X][ProcNum: %d S%d_C%d_T%d]: [64b] 0x%016lX\n", gUtContext.MsrIndex, Index,
      mProcessorLocBuf[Index].Location.Package,
      mProcessorLocBuf[Index].Location.Core,
      mProcessorLocBuf[Index].Location.Thread,
      MsrData);
  }

  //
  // Read a MSR register for BSP and all APs
  //
  if (Opcode == (OPCODE_RDMSR_BIT | OPCODE_ALLPROCESSOR_BIT)) {
    for (Index = 0; Index < mProcessorNum; Index++) {
      MsrData = 0;
      if (Index == mBspIndex) {
        MsrData = AsmReadMsr64 (gUtContext.MsrIndex);
      } else {
        mMpService->StartupThisAP (
                      mMpService,
                      (EFI_AP_PROCEDURE) ApUtReadMsr,
                      Index,
                      NULL,
                      0,
                      (UINT64 *) &MsrData,
                      NULL
                      );
      }

      Print (L"RDMSR[0x%X][ProcNum: %d S%d_C%d_T%d]: [64b] 0x%16lX\n", gUtContext.MsrIndex, Index,
        mProcessorLocBuf[Index].Location.Package,
        mProcessorLocBuf[Index].Location.Core,
        mProcessorLocBuf[Index].Location.Thread,
        MsrData);
    }
  }

  //
  // Retrieves CPUID information for BSP and all APs
  //
  if (Opcode == (OPCODE_CPUID_BIT | OPCODE_ALLPROCESSOR_BIT)) {
    for (Index = 0; Index < mProcessorNum; Index++) {
      ZeroMem (&CpuidRegister, sizeof (UT_CPUID_REGISTER));
      if (Index == mBspIndex) {
        AsmCpuidEx (gUtContext.CpuIdIndex, gUtContext.CpuIdSubIndex, &CpuidRegister.Eax, &CpuidRegister.Ebx, &CpuidRegister.Ecx, &CpuidRegister.Edx);
      } else {
        mMpService->StartupThisAP (
                      mMpService,
                      (EFI_AP_PROCEDURE) ApUtReadCpuId,
                      Index,
                      NULL,
                      0,
                      (VOID *) &CpuidRegister,
                      NULL
                      );
      }

      Print (L"CPUID[S%d_C%d_T%d]: Index: 0x%X     SubIndex: 0x%X\n",
        mProcessorLocBuf[Index].Location.Package,
        mProcessorLocBuf[Index].Location.Core,
        mProcessorLocBuf[Index].Location.Thread,
        gUtContext.CpuIdIndex, gUtContext.CpuIdSubIndex);
        Print (L"EAX = 0x%08X\n", CpuidRegister.Eax);
        Print (L"EBX = 0x%08X\n", CpuidRegister.Ebx);
        Print (L"ECX = 0x%08X\n", CpuidRegister.Ecx);
        Print (L"EDX = 0x%08X\n\n", CpuidRegister.Edx);
    }
  }

  //
  // Read MEM/MMIO/IO address
  //
  if (Opcode & (OPCODE_RMM_BIT | OPCODE_WMM_BIT)) {
    if (gUtContext.MmAccessType) {  // IO
      switch (gUtContext.MmWidth) {
        case 1:
          if (Opcode & OPCODE_RMM_BIT) {  // Read
            Data = IoRead8 (gUtContext.MmAddress);
          } else {
            IoWrite8 (gUtContext.MmAddress, (UINT8) gUtContext.MmValue);
          }
          break;
        case 2:
          if (Opcode & OPCODE_RMM_BIT) {  // Read
            Data = IoRead16 (gUtContext.MmAddress);
          } else {
            IoWrite16 (gUtContext.MmAddress, (UINT16) gUtContext.MmValue);
          }
          break;
        case 4:
          if (Opcode & OPCODE_RMM_BIT) {  // Read
            Data = IoRead32 (gUtContext.MmAddress);
          } else {
            IoWrite32 (gUtContext.MmAddress, (UINT32) gUtContext.MmValue);
          }
          break;
        case 8:
          if (Opcode & OPCODE_RMM_BIT) {  // Read
            Data = IoRead64 (gUtContext.MmAddress);
          } else {
            IoWrite64 ((UINTN) gUtContext.MmAddress, (UINT64) gUtContext.MmValue);
          }
          break;
        default:
          ASSERT (FALSE);
          break;
      }
      if (Opcode & OPCODE_RMM_BIT) {  // Read
        Print (L"Read IO [0x%X] = 0x%X\n", gUtContext.MmAddress, Data);
      } else {
        Print (L"Write 0x%x to IO [0x%X]\n", gUtContext.MmValue, gUtContext.MmAddress);
      }
    } else {  // MEM/MMIO
      switch (gUtContext.MmWidth) {
        case 1:
          if (Opcode & OPCODE_RMM_BIT) {  // Read
            Data = MmioRead8 (gUtContext.MmAddress);
          } else {
            MmioWrite8 (gUtContext.MmAddress, (UINT8) gUtContext.MmValue);
          }
          break;
        case 2:
          if (Opcode & OPCODE_RMM_BIT) {  // Read
            Data = MmioRead16 (gUtContext.MmAddress);
          } else {
            MmioWrite16 (gUtContext.MmAddress, (UINT16) gUtContext.MmValue);
          }
          break;
        case 4:
          if (Opcode & OPCODE_RMM_BIT) {  // Read
            Data = MmioRead32 (gUtContext.MmAddress);
          } else {
            MmioWrite32 (gUtContext.MmAddress, (UINT32) gUtContext.MmValue);
          }
          break;
        case 8:
          if (Opcode & OPCODE_RMM_BIT) {  // Read
            Data = IoRead64 (gUtContext.MmAddress);
          } else {
            MmioWrite64 ((UINTN) gUtContext.MmAddress, (UINT64) gUtContext.MmValue);
          }
          break;
        default:
          ASSERT (FALSE);
          break;
      }
      if (Opcode & OPCODE_RMM_BIT) {  // Read
        Print (L"Read Mem [0x%X] = 0x%X\n", gUtContext.MmAddress, Data);
      } else {
        Print (L"Write 0x%x to Mem [0x%X]\n", gUtContext.MmValue, gUtContext.MmAddress);
      }
    }
  }

  //
  // Dump memory to file
  //
  if (Opcode == OPCODE_DUMPMEM_BIT) {
    Status = SaveFileToDisk (gUtContext.DumpMemFile, gUtContext.DumpMemSize, (UINT8 *) gUtContext.DumpMemAddress);
    if (EFI_ERROR (Status)) {
      Print (L"Save memory to file failed.\n");
      return Status;
    }

    Print (L"Save memory to %s Passed.\n", gUtContext.DumpMemFile);
  }

  //
  // Get microcode update signature
  //
  if (Opcode == OPCODE_UCODE_BIT) {
    for (Index = 0; Index < mProcessorNum; Index++) {
      CurrentMicrocodeSignature = 0;
      if (Index == mBspIndex) {
        CurrentMicrocodeSignature = GetCurrentMicrocodeSignature ();
      } else {
        mMpService->StartupThisAP (
                      mMpService,
                      (EFI_AP_PROCEDURE) ApUtGetMicrocodeSignature,
                      Index,
                      NULL,
                      0,
                      (UINT32 *) &CurrentMicrocodeSignature,
                      NULL
                      );
      }

      Print (L"Microcode Signature [ProcNum: %d S%d_C%d_T%d]: 0x%08X\n", Index,
        mProcessorLocBuf[Index].Location.Package,
        mProcessorLocBuf[Index].Location.Core,
        mProcessorLocBuf[Index].Location.Thread,
        CurrentMicrocodeSignature);
    }
  }

  //
  // Read memory address on each processors
  //
  if (Opcode == OPCODE_READ_MEM_TEST_BIT) {
    for (Index = 0; Index < mProcessorNum; Index++) {
      Data64 = 0;
      if (Index == mBspIndex) {
        Data64 = MmioRead64 (gUtContext.MmAddress);
      } else {
        mMpService->StartupThisAP (
                      mMpService,
                      (EFI_AP_PROCEDURE) ReadMemoryOnEachProcessor,
                      Index,
                      NULL,
                      0,
                      (UINT32 *) &Data64,
                      NULL
                      );
      }

      Print (L"Address[0x%lx] [ProcNum: %d S%d_C%d_T%d]: 0x%lX\n", gUtContext.MmAddress, Index,
        mProcessorLocBuf[Index].Location.Package,
        mProcessorLocBuf[Index].Location.Core,
        mProcessorLocBuf[Index].Location.Thread,
        Data64);
    }
  }

  //
  // Write memory address on each processors
  //
  if (Opcode == OPCODE_WRITE_MEM_TEST_BIT) {
    for (Index = 0; Index < mProcessorNum; Index++) {
      Data64 = 0;
      if (Index == mBspIndex) {
        MmioWrite64 (gUtContext.MmAddress, gUtContext.MmValue);
      } else {
        mMpService->StartupThisAP (
                      mMpService,
                      (EFI_AP_PROCEDURE) WriteMemoryOnEachProcessor,
                      Index,
                      NULL,
                      0,
                      NULL,
                      NULL
                      );
      }

      Print (L"Address[0x%lx] Value[0x%lx] [ProcNum: %d S%d_C%d_T%d]\n", gUtContext.MmAddress, gUtContext.MmValue, Index,
        mProcessorLocBuf[Index].Location.Package,
        mProcessorLocBuf[Index].Location.Core,
        mProcessorLocBuf[Index].Location.Thread);
    }
  }

  return EFI_SUCCESS;
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
  UINT8                     Index;
  UINT64                    Opcode;

  Print (L"\nUEFI Debug Tool. Version: 1.0.0.3\n");
  Print (L"Copyright (c) 2017 - 2022 Gavin Xue. All rights reserved.\n\n");

  Opcode = 0x0;
  SetMem (&gUtContext, sizeof (UEFI_TOOL_CONTEXT), 0x0);

  gUtContext.MmWidth = 4;

  if (Argc == 1) {
    ShowHelpInfo ();
    return EFI_INVALID_PARAMETER;
  }

  else if (Argc == 2) {
    StrUpr (Argv[1]);
    if ((!StrCmp (Argv[1], L"/H")) || (!StrCmp (Argv[1], L"-H")) ||
        (!StrCmp (Argv[1], L"/?")) || (!StrCmp (Argv[1], L"-?"))) {
      ShowHelpInfo ();
      return EFI_SUCCESS;
    } else if ((!StrCmp (Argv[1], L"/SGDT")) || (!StrCmp (Argv[1], L"-SGDT"))) {
      Opcode |= OPCODE_SGDT_BIT;
    } else if ((!StrCmp (Argv[1], L"/CR")) || (!StrCmp (Argv[1], L"-CR"))) {
      Opcode |= OPCODE_CR_BIT;
    } else if ((!StrCmp (Argv[1], L"/MCU")) || (!StrCmp (Argv[1], L"-MCU"))) {
      Opcode |= OPCODE_UCODE_BIT;
    } else {
      Print (L"Invaild parameter.\n");
      return EFI_INVALID_PARAMETER;
    }
  }

  Status = GetProcessorsCpuLocation ();

  Index = 1;
  while (TRUE) {
    if (Index + 1 > Argc) {
      break;
    }

    StrUpr (Argv[Index]);

    if ((Index + 1 < Argc) && (StrCmp (Argv[Index], L"RDMSR") == 0)) {
      gUtContext.MsrIndex  = (UINT32) StrHexToUintn (Argv[Index + 1]);
      Index += 2;
      Opcode |= OPCODE_RDMSR_BIT;
    } else if ((Index + 2 < Argc) && (StrCmp (Argv[Index], L"WRMSR") == 0)) {
      gUtContext.MsrIndex  = (UINT32) StrHexToUintn (Argv[Index + 1]);
      gUtContext.MsrValue  = StrHexToUintn (Argv[Index + 2]);
      Index += 3;
      Opcode |= OPCODE_WRMSR_BIT;
    } else if ((Index + 2 < Argc) && (StrCmp (Argv[Index], L"CPUID") == 0)) {
      gUtContext.CpuIdIndex    = (UINT32) StrHexToUintn (Argv[Index + 1]);
      gUtContext.CpuIdSubIndex = (UINT32) StrHexToUintn (Argv[Index + 2]);
      Index += 3;
      Opcode |= OPCODE_CPUID_BIT;
    } else if ((Index < Argc) && (StrCmp (Argv[Index], L"-A") == 0)) {
      Index++;
      Opcode |= OPCODE_ALLPROCESSOR_BIT;
    } else if ((Index + 1 < Argc) && (StrCmp (Argv[Index], L"-P") == 0)) {
      gUtContext.ProcessorIndex  = StrHexToUintn (Argv[Index + 1]);
      Index += 2;
      Opcode |= OPCODE_PROCESSOR_INDEX_BIT;
    } else if ((Index + 1 < Argc) && (StrCmp (Argv[Index], L"-RMM") == 0)) {
      gUtContext.MmAddress = StrHexToUintn (Argv[Index + 1]);
      Index += 2;
      Opcode |= OPCODE_RMM_BIT;
    } else if ((Index + 1 < Argc) && (StrCmp (Argv[Index], L"-WMM") == 0)) {
      gUtContext.MmAddress = StrHexToUintn (Argv[Index + 1]);
      gUtContext.MmValue = StrHexToUintn (Argv[Index + 2]);
      Index += 3;
      Opcode |= OPCODE_WMM_BIT;
    } else if ((Index < Argc) && (StrCmp (Argv[Index], L"-W") == 0)) {
      gUtContext.MmWidth = StrHexToUintn (Argv[Index + 1]);
      Index += 2;
    } else if ((Index < Argc) && (StrCmp (Argv[Index], L"-MEM") == 0)) {
      gUtContext.MmAccessType = 0;
      Index++;
    } else if ((Index < Argc) && (StrCmp (Argv[Index], L"-IO") == 0)) {
      gUtContext.MmAccessType = 1;
      Index++;
    } else if ((Index + 3 < Argc) && (StrCmp (Argv[Index], L"DUMPMEM") == 0)) {
      gUtContext.DumpMemAddress = StrHexToUintn (Argv[Index + 1]);
      gUtContext.DumpMemSize    = StrHexToUintn (Argv[Index + 2]);
      CopyMem (gUtContext.DumpMemFile, Argv[Index + 3], sizeof (Argv[Index + 3]));
      Index += 4;
      Opcode |= OPCODE_DUMPMEM_BIT;

    } else if ((Index + 1 < Argc) && (StrCmp (Argv[Index], L"-RT") == 0)) {
      gUtContext.MmAddress = StrHexToUintn (Argv[Index + 1]);
      Index += 2;
      Opcode |= OPCODE_READ_MEM_TEST_BIT;
    } else if ((Index + 1 < Argc) && (StrCmp (Argv[Index], L"-WT") == 0)) {
      gUtContext.MmAddress = StrHexToUintn (Argv[Index + 1]);
      gUtContext.MmValue = StrHexToUintn (Argv[Index + 2]);
      Index += 3;
      Opcode |= OPCODE_WRITE_MEM_TEST_BIT;

    } else {
      Index++;
    }
  }

  UefiToolRoutine (Opcode);

  return 0;
}

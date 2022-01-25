# UEFI Tool Introduction

UefiTool is a simple UEFI application based on edk2. It supports various commands, including:
- Reads a Machine Specific Register (MSR) on BSP or APs.
- Writes a value to a MSR.
- Retrieves CPUID information using an extended leaf identifier.
- Reads the current Global Descriptor Table Register (GDTR) descriptor.
- Reads the current value of the Control Register (CR0/CR2/CR3/CR4).
- Gets microcode update signature of currently loaded microcode update.
- Dumps a memory range to a file.
- etc.

## How to use it?
1. Copy UefiTool.efi to USB Key.
2. Boot system to UEFI Shell.
3. Run "UefiTool.efi" to show help message

```
FS0:> UefiTool.efi
UEFI Debug Tool. Version: 1.0.0.3
Copyright (c) 2017 - 2022 Gavin Xue. All rights reserved.

Help info:
  UefiTool.efi -H

Read MSR register:
  UefiTool.efi RDMSR [MSRIndex] [OPTION: -A | -P]

Write MSR register:
  UefiTool.efi WRMSR [MSRIndex] [MSRValue]

Read CPUID:
  UefiTool.efi CPUID [CPUID_Index] [CPUID_SubIndex]

Read GDTR register:
  UefiTool.efi -SGDT

Read CR register:
  UefiTool.efi -CR

Get Microcode signature:
  UefiTool.efi -MCU

Dump memory to file:
  UefiTool.efi DUMPMEM [Address] [Size] [File]
```

## What's the examples?
1. Reads a MSR register (0x8B) on all processors.
```
FS0:> UefiTool.efi RDMSR 0x8B -A
UEFI Debug Tool. Version: 1.0.0.3
Copyright (c) 2017 - 2022 Gavin Xue. All rights reserved.

RDMSR[0x8B][ProcNum: 0 S0_C0_T0]: [64b] 0xDEADBEEF00000000
RDMSR[0x8B][ProcNum: 1 S0_C1_T0]: [64b] 0xDEADBEEF00000000
RDMSR[0x8B][ProcNum: 2 S0_C2_T0]: [64b] 0xDEADBEEF00000000
RDMSR[0x8B][ProcNum: 3 S0_C3_T0]: [64b] 0xDEADBEEF00000000
```

2. Gets microcode update signature.
```
FS0:> UefiTool.efi -MCU
UEFI Debug Tool. Version: 1.0.0.3
Copyright (c) 2017 - 2022 Gavin Xue. All rights reserved.

Microcode Signature [ProcNum: 0 S0_C0_T0]: 0xDEADBEEF
Microcode Signature [ProcNum: 1 S0_C1_T0]: 0xDEADBEEF
Microcode Signature [ProcNum: 2 S0_C2_T0]: 0xDEADBEEF
Microcode Signature [ProcNum: 3 S0_C3_T0]: 0xDEADBEEF
```

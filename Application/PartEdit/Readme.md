# PartEdit UEFI Application Introduction

PartEdit is a simple UEFI application based on edk2. It supports various disk (SATA or NVMe) operation, including:
- Dump disk partition name and info.
- Read/write NVMe from disk offset.
- Read/write NVMe from specific partition name.
- Write a file to a partition.
- Read a partition data and save to a file.
- GPT table erase/save.
- Dump disk data on screen by specific offset or LBA.
- etc.

> Notes: PartEdit only support ONE disk (SATA or NVMe) on system.


## How to use it?
1. Copy PartEdit.efi to USB Key.
2. Boot system to UEFI Shell.
3. Run "PartEdit.efi" to show help message

```
FS3:> PartEdit.efi
UEFI Partition Editor. Version: 1.1.0.0.
Copyright (c) 2019 - 2020 Gavin Xue. All rights reserved.

Help info:
  PartEdit.efi -d (Dump parent disk info)
  PartEdit.efi read partname offset size
  PartEdit.efi flash partname filename
  PartEdit.efi savepart partname
  PartEdit.efi erase partname
  PartEdit.efi gpt erase
  PartEdit.efi gpt save
  PartEdit.efi save offset size filename
  PartEdit.efi write offset size filename
  PartEdit.efi dump offset
  PartEdit.efi LBA [LBA]
```

## What's the examples?
1. Dump attached disk info:
```
FS3:> PartEdit.efi -d
PartitionName: EFI system partition
PciRoot(0x0)/Pci(0x1,0x1)/Pci(0x0,0x0)/NVMe(0x1,55-49-0A-22-28-B7-26-00)/HD(1,GPT,F5FACD59-B7C4-4E14-8055-04E8ADF601AC,0x800,0x32000)

PartitionName: Microsoft reserved partition
PciRoot(0x0)/Pci(0x1,0x1)/Pci(0x0,0x0)/NVMe(0x1,55-49-0A-22-28-B7-26-00)/HD(2,GPT,1C5A93F3-0575-41C8-9F16-19915B182159,0x32800,0x8000)

PartitionName: Basic data partition
PciRoot(0x0)/Pci(0x1,0x1)/Pci(0x0,0x0)/NVMe(0x1,55-49-0A-22-28-B7-26-00)/HD(3,GPT,E447E599-AF8D-4C45-955F-C96C8D1F6733,0x3A800,0x3C00000)

...

PciRoot(0x0)/Pci(0x1,0x1)/Pci(0x0,0x0)/NVMe(0x1,55-49-0A-22-28-B7-26-00)
MediaId         : 0
RemovableMedia  : 0
MediaPresent    : 1
LogicalPartition: 0
ReadOnly        : 0
WriteCaching    : 0
BlockSize       : 0x200
IoAlign         : 0x8
LastBlock       : 0x1BF244AF
LowestAlignedLba: 0x1
LogicalBlocksPerPhysicalBlock   : 0x1
OptimalTransferLengthGranularity: 0x0
```
2. Dump disk data from offset 0x200
```
FS3:> PartEdit.efi dump 200
Offset 0000000000000200 Size 00000200 bytes
  00000000: 45 46 49 20 50 41 52 54-00 00 01 00 5C 00 00 00  *EFI PART....\...*
  00000010: 27 89 37 E8 00 00 00 00-01 00 00 00 00 00 00 00  *'.7.............*
  00000020: AF 44 F2 1B 00 00 00 00-22 00 00 00 00 00 00 00  *.D......".......*
  00000030: 8E 44 F2 1B 00 00 00 00-2F 1D F8 34 10 0E B1 4E  *.D....../..4...N*
  00000040: AE EE 9D AA 03 49 DD EA-02 00 00 00 00 00 00 00  *.....I..........*
  00000050: 80 00 00 00 80 00 00 00-82 20 91 09 00 00 00 00  *......... ......*
  00000060: 00 00 00 00 00 00 00 00-00 00 00 00 00 00 00 00  *................*
  00000070: 00 00 00 00 00 00 00 00-00 00 00 00 00 00 00 00  *................*
  00000080: 00 00 00 00 00 00 00 00-00 00 00 00 00 00 00 00  *................*
  00000090: 00 00 00 00 00 00 00 00-00 00 00 00 00 00 00 00  *................*
  000000A0: 00 00 00 00 00 00 00 00-00 00 00 00 00 00 00 00  *................*
  000000B0: 00 00 00 00 00 00 00 00-00 00 00 00 00 00 00 00  *................*
  000000C0: 00 00 00 00 00 00 00 00-00 00 00 00 00 00 00 00  *................*
  000000D0: 00 00 00 00 00 00 00 00-00 00 00 00 00 00 00 00  *................*
  000000E0: 00 00 00 00 00 00 00 00-00 00 00 00 00 00 00 00  *................*
  000000F0: 00 00 00 00 00 00 00 00-00 00 00 00 00 00 00 00  *................*
  00000100: 00 00 00 00 00 00 00 00-00 00 00 00 00 00 00 00  *................*
  00000110: 00 00 00 00 00 00 00 00-00 00 00 00 00 00 00 00  *................*
  00000120: 00 00 00 00 00 00 00 00-00 00 00 00 00 00 00 00  *................*
  00000130: 00 00 00 00 00 00 00 00-00 00 00 00 00 00 00 00  *................*
  00000140: 00 00 00 00 00 00 00 00-00 00 00 00 00 00 00 00  *................*
  00000150: 00 00 00 00 00 00 00 00-00 00 00 00 00 00 00 00  *................*
  00000160: 00 00 00 00 00 00 00 00-00 00 00 00 00 00 00 00  *................*
  00000170: 00 00 00 00 00 00 00 00-00 00 00 00 00 00 00 00  *................*
  00000180: 00 00 00 00 00 00 00 00-00 00 00 00 00 00 00 00  *................*
  00000190: 00 00 00 00 00 00 00 00-00 00 00 00 00 00 00 00  *................*
  000001A0: 00 00 00 00 00 00 00 00-00 00 00 00 00 00 00 00  *................*
  000001B0: 00 00 00 00 00 00 00 00-00 00 00 00 00 00 00 00  *................*
  000001C0: 00 00 00 00 00 00 00 00-00 00 00 00 00 00 00 00  *................*
  000001D0: 00 00 00 00 00 00 00 00-00 00 00 00 00 00 00 00  *................*
  000001E0: 00 00 00 00 00 00 00 00-00 00 00 00 00 00 00 00  *................*
  000001F0: 00 00 00 00 00 00 00 00-00 00 00 00 00 00 00 00  *................*
  ```

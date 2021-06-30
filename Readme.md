# UefiPubPkg - A UEFI public package for development and collection.
### Build Status

OS|Build Status
---|---
Windows|[![Build Status](https://dev.azure.com/vinxue/UefiPkg/_apis/build/status/vinxue.UefiPubPkg?branchName=master&jobName=WindowsBuild)](https://dev.azure.com/vinxue/UefiPkg/_build/latest?definitionId=8&branchName=master)
Linux|[![Build Status](https://dev.azure.com/vinxue/UefiPkg/_apis/build/status/vinxue.UefiPubPkg?branchName=master&jobName=LinuxBuild)](https://dev.azure.com/vinxue/UefiPkg/_build/latest?definitionId=8&branchName=master)
## Build

1. Install Visual Studio 2019
2. Get a full and buildable EDKII repository and UefiPubPkg package, use follow steps of git command
```
   git clone https://github.com/tianocore/edk2.git
   cd edk2
   git submodule update --init
   git clone https://github.com/vinxue/UefiPubPkg.git
```
3. Run commands:
```
   edksetup.bat rebuild
   build -a IA32 -a X64 -b RELEASE -t VS2019 -p UefiPubPkg\UefiPkg.dsc
```

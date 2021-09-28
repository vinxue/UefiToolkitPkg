# UefiToolkitPkg - A UEFI toolkit package for development and collection
[![Open in Visual Studio Code](https://open.vscode.dev/badges/open-in-vscode.svg)](https://open.vscode.dev/vinxue/UefiToolkitPkg)

## Build Status

OS|Build Status
---|---
Windows|[![Build Status](https://dev.azure.com/vinxue/UefiPkg/_apis/build/status/vinxue.UefiToolkitPkg?branchName=master&jobName=WindowsBuild)](https://dev.azure.com/vinxue/UefiPkg/_build/latest?definitionId=9&branchName=master)
Linux|[![Build Status](https://dev.azure.com/vinxue/UefiPkg/_apis/build/status/vinxue.UefiToolkitPkg?branchName=master&jobName=LinuxBuild)](https://dev.azure.com/vinxue/UefiPkg/_build/latest?definitionId=9&branchName=master)

## Build

1. Install Visual Studio 2019.
2. Get a full and buildable EDKII repository and UefiToolkitPkg package, use follow steps of git command:
```
   git clone https://github.com/tianocore/edk2.git
   cd edk2
   git submodule update --init
   git clone https://github.com/vinxue/UefiToolkitPkg.git
```
3. Run build commands:
```
   edksetup.bat rebuild
   build -a IA32 -a X64 -b RELEASE -t VS2019 -p UefiToolkitPkg\UefiPkg.dsc
```

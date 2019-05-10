# UefiPkg - A UEFI package for development and collection.
### Build Status

OS|Build Status
---|---
Windows|[![Build Status](https://dev.azure.com/vinxue/UefiPkg/_apis/build/status/vinxue.UefiPkg?branchName=master)](https://dev.azure.com/vinxue/UefiPkg/_build/latest?definitionId=5&branchName=master)
Linux|[![Build Status](https://travis-ci.org/vinxue/UefiPkg.svg?branch=master)](https://travis-ci.org/vinxue/UefiPkg)
## Build

1. Install Visual Studio 2017 and download UDK2018 code.
2. Copy UefiPkg to UDK2018 root directory.
3. Run commands:

       edksetup.bat
       build -p UefiPkg/UefiPkg.dsc -a X64 -t VS2017 -b RELEASE

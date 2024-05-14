#!/bin/bash

echo "Install required software from apt..."
sudo apt update
sudo apt install build-essential uuid-dev iasl nasm

git clone https://github.com/tianocore/edk2
cd edk2
git submodule update --init

if [ ! -d UefiToolkitPkg ]; then
  ln -s .. UefiToolkitPkg
fi

source edksetup.sh
make -C BaseTools

build -a IA32 -a X64 -b RELEASE -t GCC5 -p UefiToolkitPkg/UefiPkg.dsc

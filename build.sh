#!/bin/bash

BUILDDIR=$(dirname "$0")
pushd "$BUILDDIR" >/dev/null
BUILDDIR=$(pwd)
popd >/dev/null

echo "Install required software from apt..."
sudo apt update || exit 1
sudo apt install build-essential uuid-dev iasl nasm || exit 1

git clone https://github.com/tianocore/edk2
cd edk2
git submodule update --init

if [ ! -d UefiToolkitPkg ]; then
  ln -s .. UefiToolkitPkg || exit 1
fi

source edksetup.sh || exit 1
make -C BaseTools || exit 1

build -a IA32 -a X64 -b RELEASE -t GCC5 -p UefiToolkitPkg/UefiPkg.dsc || exit 1

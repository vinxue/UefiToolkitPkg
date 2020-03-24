#!/bin/bash

BUILDDIR=$(dirname "$0")
pushd "$BUILDDIR" >/dev/null
BUILDDIR=$(pwd)
popd >/dev/null

if [ "$(which gcc)" = "" ] || [ "$(which nasm)" = "" ] || [ "$(which iasl)" = "" ]]; then
  echo "Install required software from apt..."
  sudo apt update || exit 1
  sudo apt install build-essential uuid-dev iasl gcc-5 nasm || exit 1
fi

git clone https://github.com/tianocore/edk2 -b UDK2018
cd edk2

if [ ! -d UefiPkg ]; then
  ln -s .. UefiPkg || exit 1
fi

source edksetup.sh || exit 1
make -C BaseTools || exit 1

build -a IA32 -a X64 -b RELEASE -t GCC5 -p UefiPkg/UefiPkg.dsc || exit 1

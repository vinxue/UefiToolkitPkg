#!/bin/bash

echo "Install required software from apt..."
sudo apt update || exit 1
sudo apt install build-essential uuid-dev iasl || exit 1
wget http://mirrors.kernel.org/ubuntu/pool/universe/n/nasm/nasm_2.15.05-1_amd64.deb
sudo dpkg -i nasm_2.15.05-1_amd64.deb

git clone https://github.com/tianocore/edk2
cd edk2
git submodule update --init

if [ ! -d UefiToolkitPkg ]; then
  ln -s .. UefiToolkitPkg || exit 1
fi

source edksetup.sh || exit 1
make -C BaseTools || exit 1

build -a IA32 -a X64 -b RELEASE -t GCC5 -p UefiToolkitPkg/UefiPkg.dsc || exit 1

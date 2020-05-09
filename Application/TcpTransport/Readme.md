# TcpTransport - A UEFI application to receive TCP network packets.

## Build
Build TcpTransport UEFI application with edk2 (https://github.com/tianocore/edk2).

e.g.

    edksetup.bat
    build -a X64 -b RELEASE -t VS2017 -p AbcPkg/AbcPkg.dsc

## Verification
1. Download the free utility Packet Sender from https://packetsender.com/.
2. Run TcpTransport application in UEFI Shell.
3. Run Packet Sender utility to send data in host PC by command line: (Windows OS as example)

        C:\PacketSenderPortable>packetsender.com -t 10.65.83.170 5554 -f Test.bin
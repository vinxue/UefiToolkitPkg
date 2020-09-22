## @file
# Generate a capsule.
#
# This tool generates a UEFI Capsule around an FMP Capsule.  The capsule payload
# be signed using signtool or OpenSSL and if it is signed the signed content
# includes an FMP Payload Header.
#
# This tool is intended to be used to generate UEFI Capsules to update the
# system firmware or device firmware for integrated devices.  In order to
# keep the tool as simple as possible, it has the following limitations:
#   * Do not support multiple payloads in a capsule.
#   * Do not support optional drivers in a capsule.
#   * Do not support vendor code bytes in a capsule.
#
# Copyright (c) 2018, Intel Corporation. All rights reserved.<BR>
# This program and the accompanying materials
# are licensed and made available under the terms and conditions of the BSD License
# which accompanies this distribution.  The full text of the license may be found at
# http://opensource.org/licenses/bsd-license.php
#
# THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
# WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
#

'''
GenerateCapsule
'''

import sys
import argparse
import uuid
import struct
import subprocess
import os
import tempfile
import shutil
import platform
sys.dont_write_bytecode = True
from Uefi.Capsule.UefiCapsuleHeader import UefiCapsuleHeaderClass
from Uefi.Capsule.FmpCapsuleHeader  import FmpCapsuleHeaderClass
from Uefi.Capsule.FmpAuthHeader     import FmpAuthHeaderClass
from Rsa2048Sha256Sign              import SignPayloadByRsa2048Sha256

#
# Globals for help information
#
__prog__        = 'GenerateCapsule'
__version__     = '0.9'
__copyright__   = 'Copyright (c) 2018, Intel Corporation. All rights reserved.'
__description__ = 'Generate a capsule.\n'

if __name__ == '__main__':
    def convert_arg_line_to_args(arg_line):
        for arg in arg_line.split():
            if not arg.strip():
                continue
            yield arg

    def ValidateUnsignedInteger (Argument):
        try:
            Value = int (Argument, 0)
        except:
            Message = '{Argument} is not a valid integer value.'.format (Argument = Argument)
            raise argparse.ArgumentTypeError (Message)
        if Value < 0:
            Message = '{Argument} is a negative value.'.format (Argument = Argument)
            raise argparse.ArgumentTypeError (Message)
        return Value

    def ValidateRegistryFormatGuid (Argument):
        try:
            Value = uuid.UUID (Argument)
        except:
            Message = '{Argument} is not a valid registry format GUID value.'.format (Argument = Argument)
            raise argparse.ArgumentTypeError (Message)
        return Value

    #
    # Create command line argument parser object
    #
    parser = argparse.ArgumentParser (
                        prog = __prog__,
                        description = __description__ + __copyright__,
                        conflict_handler = 'resolve',
                        fromfile_prefix_chars = '@'
                        )
    parser.convert_arg_line_to_args = convert_arg_line_to_args

    #
    # Add input and output file arguments
    #
    parser.add_argument("InputFile",  type = argparse.FileType('rb'),
                        help = "Input binary payload filename.")
    parser.add_argument("-o", "--output", dest = 'OutputFile', type = argparse.FileType('wb'),
                        help = "Output filename.")
    #
    # Add group for -e and -d flags that are mutually exclusive and required
    #
    group = parser.add_mutually_exclusive_group (required = True)
    group.add_argument ("-e", "--encode", dest = 'Encode', action = "store_true",
                        help = "Encode file")
    group.add_argument ("--dump-info", dest = 'DumpInfo', action = "store_true",
                        help = "Display FMP Payload Header information")
    #
    # Add optional arguments for this command
    #
    parser.add_argument ("--capflag", dest = 'CapsuleFlag', action='append', default = [],
                         choices=['PersistAcrossReset', 'InitiateReset'],
                         help = "Capsule flag can be PersistAcrossReset or InitiateReset or not set")
    parser.add_argument ("--capoemflag", dest = 'CapsuleOemFlag', type = ValidateUnsignedInteger, default = 0x0000,
                         help = "Capsule OEM Flag is an integer between 0x0000 and 0xffff.")

    parser.add_argument ("--guid", dest = 'Guid', type = ValidateRegistryFormatGuid,
                         help = "The FMP/ESRT GUID in registry format.  Required for encode operations.")
    parser.add_argument ("--hardware-instance", dest = 'HardwareInstance', type = ValidateUnsignedInteger, default = 0x0000000000000000,
                         help = "The 64-bit hardware instance.  The default is 0x0000000000000000")

    parser.add_argument ("--monotonic-count", dest = 'MonotonicCount', type = ValidateUnsignedInteger, default = 0x0000000000000000,
                         help = "64-bit monotonic count value in header.  Default is 0x0000000000000000.")

    parser.add_argument ("--private-key", dest='PrivateKeyFile', type=argparse.FileType('rb'),
                         help="specify the private key filename.")

    #
    # Add optional arguments common to all operations
    #
    parser.add_argument ('--version', action='version', version='%(prog)s ' + __version__)
    parser.add_argument ("-v", "--verbose", dest = 'Verbose', action = "store_true",
                         help = "Turn on verbose output with informational messages printed, including capsule headers and warning messages.")

    #
    # Parse command line arguments
    #
    args = parser.parse_args()

    #
    # Perform additional argument verification
    #
    if args.Encode:
        if args.Guid is None:
            parser.error ('the following option is required: --guid')
        if 'PersistAcrossReset' not in args.CapsuleFlag:
            if 'InitiateReset' in args.CapsuleFlag:
                parser.error ('--capflag InitiateReset also requires --capflag PersistAcrossReset')
        if args.CapsuleOemFlag > 0xFFFF:
            parser.error ('--capoemflag must be an integer between 0x0000 and 0xffff')
        if args.HardwareInstance > 0xFFFFFFFFFFFFFFFF:
            parser.error ('--hardware-instance must be an integer in range 0x0..0xffffffffffffffff')
        if args.MonotonicCount > 0xFFFFFFFFFFFFFFFF:
            parser.error ('--monotonic-count must be an integer in range 0x0..0xffffffffffffffff')
        if args.OutputFile is None:
            parser.error ('the following option is required for all encode and decode operations: --output')

        if platform.system() != 'Windows':
            parser.error ('Use of openssl is not supported on this operating system.')

        args.PrivateKeyFile.close()
        args.PrivateKeyFile = args.PrivateKeyFile.name

    if args.DumpInfo:
        if args.OutputFile is not None:
            parser.error ('the following option is not supported for dumpinfo operations: --output')

    #
    # Read binary input file
    #
    try:
        if args.Verbose:
            print ('Read binary input file {File}'.format (File = args.InputFile.name))
        Buffer = args.InputFile.read ()
        args.InputFile.close ()
    except:
        print ('GenerateCapsule: error: can not read binary input file {File}'.format (File = args.InputFile.name))
        sys.exit (1)

    #
    # Create objects
    #
    UefiCapsuleHeader = UefiCapsuleHeaderClass ()
    FmpCapsuleHeader  = FmpCapsuleHeaderClass ()
    FmpAuthHeader     = FmpAuthHeaderClass ()

    if args.Encode:
        Result = Buffer
        #
        # Sign image with 64-bit MonotonicCount appended to end of image
        #
        try:
            CertData = SignPayloadByRsa2048Sha256 (
                        str (args.MonotonicCount),
                        args.PrivateKeyFile,
                        Result
                        )
        except:
            print ('GenerateCapsule: error: can not sign payload')
            sys.exit (1)

        try:
            FmpAuthHeader.MonotonicCount = args.MonotonicCount
            FmpAuthHeader.CertData       = CertData
            FmpAuthHeader.Payload        = Result
            Result = FmpAuthHeader.Encode ()
            if args.Verbose:
                FmpAuthHeader.DumpInfo ()
        except:
            print ('GenerateCapsule: error: can not encode FMP Auth Header')
            sys.exit (1)

        try:
            FmpCapsuleHeader.AddPayload (args.Guid, Result, HardwareInstance = args.HardwareInstance)
            Result = FmpCapsuleHeader.Encode ()
            if args.Verbose:
                FmpCapsuleHeader.DumpInfo ()
        except:
            print ('GenerateCapsule: error: can not encode FMP Capsule Header')
            sys.exit (1)

        try:
            UefiCapsuleHeader.OemFlags            = args.CapsuleOemFlag
            UefiCapsuleHeader.PersistAcrossReset  = 'PersistAcrossReset'  in args.CapsuleFlag
            UefiCapsuleHeader.PopulateSystemTable = False
            UefiCapsuleHeader.InitiateReset       = 'InitiateReset'       in args.CapsuleFlag
            UefiCapsuleHeader.Payload             = Result
            Result = UefiCapsuleHeader.Encode ()
            if args.Verbose:
                UefiCapsuleHeader.DumpInfo ()
        except:
            print ('GenerateCapsule: error: can not encode UEFI Capsule Header')
            sys.exit (1)

    elif args.DumpInfo:
        try:
            Result = UefiCapsuleHeader.Decode (Buffer)
            FmpCapsuleHeader.Decode (Result)
            Result = FmpCapsuleHeader.GetFmpCapsuleImageHeader (0).Payload
            print ('========')
            UefiCapsuleHeader.DumpInfo ()
            print ('--------')
            FmpCapsuleHeader.DumpInfo ()
            try:
                Result = FmpAuthHeader.Decode (Result)
                print ('--------')
                FmpAuthHeader.DumpInfo ()
            except:
                print ('--------')
                print ('No EFI_FIRMWARE_IMAGE_AUTHENTICATION')
                print ('--------')
                print ('No FMP_PAYLOAD_HEADER')
            print ('========')
        except:
            print ('GenerateCapsule: error: can not decode capsule')
            sys.exit (1)
    else:
        print('GenerateCapsule: error: invalid options')
        sys.exit (1)

    #
    # Write binary output file
    #
    if args.OutputFile is not None:
        try:
            if args.Verbose:
                print ('Write binary output file {File}'.format (File = args.OutputFile.name))
            args.OutputFile.write (Result)
            args.OutputFile.close ()
        except:
            print ('GenerateCapsule: error: can not write binary output file {File}'.format (File = args.OutputFile.name))
            sys.exit (1)

    if args.Verbose:
        print('Success')

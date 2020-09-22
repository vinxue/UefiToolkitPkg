## @file
# This tool encodes and decodes GUIDed FFS sections or FMP capsule for a GUID type of
# EFI_CERT_TYPE_RSA2048_SHA256_GUID defined in the UEFI 2.4 Specification as
#   {0xa7717414, 0xc616, 0x4977, {0x94, 0x20, 0x84, 0x47, 0x12, 0xa7, 0x35, 0xbf}}
# This tool has been tested with OpenSSL 1.0.1e 11 Feb 2013
#
# Copyright (c) 2013 - 2018, Intel Corporation. All rights reserved.<BR>
# This program and the accompanying materials
# are licensed and made available under the terms and conditions of the BSD License
# which accompanies this distribution.  The full text of the license may be found at
# http://opensource.org/licenses/bsd-license.php
#
# THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
# WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
#

'''
Rsa2048Sha256Sign
'''

import os
import sys
import argparse
import subprocess
import uuid
import struct
import collections

#
# Globals for help information
#
__prog__      = 'Rsa2048Sha256Sign'
__version__   = '%s Version %s' % (__prog__, '0.9')
__copyright__ = 'Copyright (c) 2013 - 2016, Intel Corporation. All rights reserved.'
__usage__     = '%s -e|-d [options] <input_file>' % (__prog__)

#
# GUID for SHA 256 Hash Algorithm from UEFI Specification
#
EFI_HASH_ALGORITHM_SHA256_GUID = uuid.UUID('{51aa59de-fdf2-4ea3-bc63-875fb7842ee9}')

#
# Structure defintion to unpack EFI_CERT_BLOCK_RSA_2048_SHA256 from UEFI 2.4 Specification
#
#   typedef struct _EFI_CERT_BLOCK_RSA_2048_SHA256 {
#     EFI_GUID HashType;
#     UINT8 PublicKey[256];
#     UINT8 Signature[256];
#   } EFI_CERT_BLOCK_RSA_2048_SHA256;
#
EFI_CERT_BLOCK_RSA_2048_SHA256        = collections.namedtuple('EFI_CERT_BLOCK_RSA_2048_SHA256', ['HashType','PublicKey','Signature'])
EFI_CERT_BLOCK_RSA_2048_SHA256_STRUCT = struct.Struct('16s256s256s')

def SignPayloadByRsa2048Sha256 (MonotonicCountStr, PrivateKeyFile, InputFile):
  #
  # Generate file path to Open SSL command
  #
  OpenSslCommand = 'openssl'
  try:
    OpenSslPath = os.environ['OPENSSL_PATH']
    OpenSslCommand = os.path.join(OpenSslPath, OpenSslCommand)
    if ' ' in OpenSslCommand:
      OpenSslCommand = '"' + OpenSslCommand + '"'
  except:
    pass

  #
  # Verify that Open SSL command is available
  #
  try:
    Process = subprocess.Popen('%s version' % (OpenSslCommand), stdout=subprocess.PIPE, stderr=subprocess.PIPE, shell=True)
  except:
    print 'ERROR: Open SSL command not available.  Please verify PATH or set OPENSSL_PATH'
    sys.exit(1)

  Version = Process.communicate()
  if Process.returncode <> 0:
    print 'ERROR: Open SSL command not available.  Please verify PATH or set OPENSSL_PATH'
    sys.exit(Process.returncode)
  print Version[0]

  #
  # Read input file into a buffer
  #
  InputFileBuffer = InputFile

  #
  # Save private key filename and close private key file
  #
  try:
    PrivateKeyFileName = PrivateKeyFile
    with open (PrivateKeyFileName, 'rb') as file:
      PrivateKeyFile = file.read ()
  except:
    print 'ERROR: read signing private key file %s failed' % (PrivateKeyFileName)
    sys.exit(1)

  #
  # Extract public key from private key into STDOUT
  #
  Process = subprocess.Popen('%s rsa -in "%s" -modulus -noout' % (OpenSslCommand, PrivateKeyFileName), stdout=subprocess.PIPE, stderr=subprocess.PIPE, shell=True)
  PublicKeyHexString = Process.communicate()[0].split('=')[1].strip()
  PublicKey = ''
  while len(PublicKeyHexString) > 0:
    PublicKey = PublicKey + chr(int(PublicKeyHexString[0:2],16))
    PublicKeyHexString=PublicKeyHexString[2:]
  if Process.returncode <> 0:
    sys.exit(Process.returncode)

  if MonotonicCountStr:
    try:
      if MonotonicCountStr.upper().startswith('0X'):
        MonotonicCountValue = (long)(MonotonicCountStr, 16)
      else:
        MonotonicCountValue = (long)(MonotonicCountStr)
    except:
        pass

  FullInputFileBuffer = InputFileBuffer
  if MonotonicCountStr:
    format = "%dsQ" % len(InputFileBuffer)
    FullInputFileBuffer = struct.pack(format, InputFileBuffer, MonotonicCountValue)
  #
  # Sign the input file using the specified private key and capture signature from STDOUT
  #
  Process = subprocess.Popen('%s dgst -sha256 -sign "%s"' % (OpenSslCommand, PrivateKeyFileName), stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE, shell=True)
  Signature = Process.communicate(input=FullInputFileBuffer)[0]
  if Process.returncode <> 0:
    sys.exit(Process.returncode)

  return EFI_HASH_ALGORITHM_SHA256_GUID.get_bytes_le() + PublicKey + Signature

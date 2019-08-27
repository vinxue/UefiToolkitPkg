/** @file

  Copyright (c) 2019, Gavin Xue. All rights reserved.<BR>

  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#include <Base.h>
#include <Library/BaseLib.h>
#include <Library/PcdLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiApplicationEntryPoint.h>
#include <Library/DebugLib.h>
#include <Library/PrintLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/ShellLib.h>
#include <Library/TimerLib.h>
#include <Library/NetLib.h>
#include <Protocol/SimpleTextOut.h>
#include <Protocol/SimpleTextIn.h>
#include <Protocol/Dhcp4.h>
#include <Protocol/Tcp4.h>
#include <Protocol/ServiceBinding.h>

EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *mTextOut;
EFI_TCP4_PROTOCOL               *mTcpConnection;
EFI_TCP4_PROTOCOL               *mTcpListener;

EFI_TCP4_IO_TOKEN                mReceiveToken;
EFI_TCP4_RECEIVE_DATA            mRxData;
EFI_TCP4_IO_TOKEN                mTransmitToken;
EFI_TCP4_TRANSMIT_DATA           mTxData;
// We also reuse the accept token
EFI_TCP4_LISTEN_TOKEN            mAcceptToken;

EFI_TCP4_CLOSE_TOKEN             mCloseToken;
EFI_EVENT mReceiveEvent;
EFI_SERVICE_BINDING_PROTOCOL     *mTcpServiceBinding;
EFI_HANDLE                       mTcpHandle = NULL;

EFI_EVENT                        mFinishedEvent;

#define RX_FRAGMENT_SIZE         2048
#define TCP_STATION_PORT         5554

BOOLEAN IsRxDone = FALSE;

#define IP4_ADDR_TO_STRING(IpAddr, IpAddrString) UnicodeSPrint (       \
                                                   IpAddrString,       \
                                                   16 * 2,             \
                                                   L"%d.%d.%d.%d",     \
                                                   IpAddr.Addr[0],     \
                                                   IpAddr.Addr[1],     \
                                                   IpAddr.Addr[2],     \
                                                   IpAddr.Addr[3]      \
                                                   );

VOID
EFIAPI
CommonNotify (
  IN EFI_EVENT  Event,
  IN VOID       *Context
  )
{
  if ((Event == NULL) || (Context == NULL)) {
    return ;
  }

  *((BOOLEAN *) Context) = TRUE;
}

EFI_STATUS
TcpDataReceive (
  IN  OUT UINTN               *BufferSize,
  IN  OUT VOID                **Buffer
  )
{
  EFI_STATUS             Status;
  VOID                   *FragmentBuffer;
  UINT8                  *DataBuffer;
  UINT8                  *BufferTemp;
  NET_FRAGMENT           Fragment;
  UINTN                  TotalDataNumber;
  UINTN                  AllocateLength;

  DataBuffer      = NULL;
  Fragment.Len    = 0;
  Fragment.Bulk   = NULL;
  TotalDataNumber = 0;
  AllocateLength  = 0;
  *BufferSize     = 0;
  *Buffer         = NULL;

  Status = EFI_SUCCESS;

  FragmentBuffer = AllocateZeroPool (RX_FRAGMENT_SIZE);
  ASSERT (FragmentBuffer != NULL);
  if (FragmentBuffer == NULL) {
    DEBUG ((DEBUG_ERROR, "TCP out of resources"));
    return EFI_OUT_OF_RESOURCES;
  }

  while (TRUE) {
    IsRxDone = FALSE;
    mRxData.DataLength = RX_FRAGMENT_SIZE;
    mRxData.FragmentTable[0].FragmentLength = RX_FRAGMENT_SIZE;
    mRxData.FragmentTable[0].FragmentBuffer = FragmentBuffer;

    Status = mTcpConnection->Receive (mTcpConnection, &mReceiveToken);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "TCP receive failed: %r\n", Status));
      return Status;
    }

    while (!IsRxDone) {
      mTcpConnection->Poll (mTcpConnection);
    }

    Status = mReceiveToken.CompletionToken.Status;
    if (EFI_ERROR (Status)) {
      gBS->CloseEvent (mReceiveToken.CompletionToken.Event);
      return Status;
    }

    Fragment.Len  = mReceiveToken.Packet.RxData->FragmentTable[0].FragmentLength;
    Fragment.Bulk = (UINT8 *) mReceiveToken.Packet.RxData->FragmentTable[0].FragmentBuffer;

    //
    // Append the received data
    //
    if (AllocateLength < TotalDataNumber + Fragment.Len) {
      AllocateLength += SIZE_8MB;
      BufferTemp = AllocateZeroPool (AllocateLength);
      if (BufferTemp == NULL) {
        if (DataBuffer != NULL) {
          FreePool (DataBuffer);
        }
        return EFI_OUT_OF_RESOURCES;
      }

      if (DataBuffer != NULL) {
        CopyMem (BufferTemp, DataBuffer, TotalDataNumber);
        FreePool (DataBuffer);
      }
    }

    if (Fragment.Bulk != NULL) {
      CopyMem (BufferTemp + TotalDataNumber, Fragment.Bulk, Fragment.Len);
    }

    TotalDataNumber += Fragment.Len;
    DataBuffer      = BufferTemp;

    *Buffer     = BufferTemp;
    *BufferSize = TotalDataNumber;
  }

  return Status;
}

VOID
EFIAPI
SaveFileToDisk (
  IN  UINTN               BufferSize,
  IN  VOID                *Buffer
  )
{
  EFI_STATUS           Status;
  SHELL_FILE_HANDLE    FileHandle;

  mTextOut->OutputString (mTextOut, L"Save file...\r\n");

  if (!EFI_ERROR (ShellFileExists (L"Download.bin"))) {
    ShellDeleteFileByName (L"Download.bin");
  }

  Status = ShellOpenFileByName (L"Download.bin", &FileHandle, EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE | EFI_FILE_MODE_CREATE, 0);
  if (EFI_ERROR (Status)) {
    mTextOut->OutputString (mTextOut, L"ERROR: Open file\n");
    FreePool (Buffer);
    return;
  }

  Status = ShellWriteFile (FileHandle, &BufferSize, Buffer);
  if (EFI_ERROR (Status)) {
    mTextOut->OutputString (mTextOut, L"ERROR: Write file\n");
    FreePool (Buffer);
    ShellCloseFile (&FileHandle);
    return;
  }

  ShellCloseFile (&FileHandle);
  mTextOut->OutputString (mTextOut, L"Save file completed.\r\n");
}

VOID
EFIAPI
ConnectionAccepted (
  IN EFI_EVENT Event,
  IN VOID     *Context
  )
{
  EFI_STATUS             Status;
  EFI_TCP4_LISTEN_TOKEN  *AcceptToken;
  UINTN                  DataSize;
  VOID                   *Data;

  AcceptToken = (EFI_TCP4_LISTEN_TOKEN *) Context;
  Status = AcceptToken->CompletionToken.Status;

  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "TCP: Connection Error: %r\n", Status));
    return;
  }
  DEBUG ((DEBUG_ERROR, "TCP: Connection Received.\n"));

  //
  // Accepting a new TCP connection creates a new instance of the TCP protocol.
  // Open it and prepare to receive on it.
  //
  Status = gBS->OpenProtocol (
                  AcceptToken->NewChildHandle,
                  &gEfiTcp4ProtocolGuid,
                  (VOID **) &mTcpConnection,
                  gImageHandle,
                  NULL,
                  EFI_OPEN_PROTOCOL_GET_PROTOCOL
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Open TCP Connection: %r\n", Status));
    return;
  }

  Status = gBS->CreateEvent (
                  EVT_NOTIFY_SIGNAL,
                  TPL_NOTIFY,
                  CommonNotify,
                  &IsRxDone,
                  &(mReceiveToken.CompletionToken.Event)
                  );
  ASSERT_EFI_ERROR (Status);

  TcpDataReceive (&DataSize, &Data);

  SaveFileToDisk (DataSize, Data);
}

/**
  UEFI application entry point which has an interface similar to a
  standard C main function.

  The ShellCEntryLib library instance wrappers the actual UEFI application
  entry point and calls this ShellAppMain function.

  @param[in]  Argc  The number of parameters.
  @param[in]  Argv  The array of pointers to parameters.

  @retval  0               The application exited normally.
  @retval  Other           An error occurred.

**/
INTN
EFIAPI
ShellAppMain (
  IN UINTN                     Argc,
  IN CHAR16                    **Argv
  )
{
  EFI_STATUS                      Status;
  EFI_HANDLE                      NetDeviceHandle;
  EFI_HANDLE                      *HandleBuffer;
  EFI_IP4_MODE_DATA               Ip4ModeData;
  UINTN                           NumHandles;
  CHAR16                          IpAddrString[16];
  EFI_EVENT                       WaitEventArray[2];
  EFI_SIMPLE_TEXT_INPUT_PROTOCOL  *TextIn;
  UINTN                           EventIndex;

  Status = gBS->LocateProtocol (&gEfiSimpleTextOutProtocolGuid, NULL, (VOID **) &mTextOut);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Couldn't open Text Output Protocol: %r\n", Status));
    return Status;
  }

  Status = gBS->LocateProtocol (&gEfiSimpleTextInProtocolGuid, NULL, (VOID **) &TextIn);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Couldn't open Text Input Protocol: %r\n", Status));
    return Status;
  }

  EFI_TCP4_CONFIG_DATA TcpConfigData = {
    0x00,                                           // IPv4 Type of Service
    255,                                            // IPv4 Time to Live
    {                                               // AccessPoint:
      TRUE,                                         // Use default address
      { {0, 0, 0, 0} },                             // IP Address  (ignored - use default)
      { {0, 0, 0, 0} },                             // Subnet mask (ignored - use default)
      TCP_STATION_PORT,                             // Station port
      { {0, 0, 0, 0} },                             // Remote address: accept any
      0,                                            // Remote Port: accept any
      FALSE                                         // ActiveFlag: be a "server"
    },
    NULL                                            // Default advanced TCP options
  };

  mTextOut->OutputString (mTextOut, L"Initialising TCP transport...\r\n");

  //
  // Open a passive TCP instance
  //
  Status = gBS->LocateHandleBuffer (
                  ByProtocol,
                  &gEfiTcp4ServiceBindingProtocolGuid,
                  NULL,
                  &NumHandles,
                  &HandleBuffer
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Find TCP Service Binding: %r\n", Status));
    return Status;
  }

  //
  // We just use the first network device
  //
  NetDeviceHandle = HandleBuffer[0];

  Status =  gBS->OpenProtocol (
                    NetDeviceHandle,
                    &gEfiTcp4ServiceBindingProtocolGuid,
                    (VOID **) &mTcpServiceBinding,
                    gImageHandle,
                    NULL,
                    EFI_OPEN_PROTOCOL_GET_PROTOCOL
                    );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Open TCP Service Binding: %r\n", Status));
    return Status;
  }

  Status = mTcpServiceBinding->CreateChild (mTcpServiceBinding, &mTcpHandle);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "TCP ServiceBinding Create: %r\n", Status));
    return Status;
  }

  Status =  gBS->OpenProtocol (
                   mTcpHandle,
                   &gEfiTcp4ProtocolGuid,
                   (VOID **) &mTcpListener,
                   gImageHandle,
                   NULL,
                   EFI_OPEN_PROTOCOL_GET_PROTOCOL
                   );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Open TCP Protocol: %r\n", Status));
  }

  //
  // Set up re-usable tokens
  //
  mRxData.UrgentFlag = FALSE;
  mRxData.FragmentCount = 1;
  mReceiveToken.Packet.RxData = &mRxData;

  mTxData.Push = TRUE;
  mTxData.Urgent = FALSE;
  mTxData.FragmentCount = 1;
  mTransmitToken.Packet.TxData = &mTxData;

  Status = gBS->CreateEvent (
                  EVT_NOTIFY_SIGNAL,
                  TPL_CALLBACK,
                  ConnectionAccepted,
                  &mAcceptToken,
                  &mAcceptToken.CompletionToken.Event
                  );
  ASSERT_EFI_ERROR (Status);

  Status = gBS->CreateEvent (
                  EVT_NOTIFY_SIGNAL,
                  TPL_CALLBACK,
                  CommonNotify,
                  &mCloseToken,
                  &mCloseToken.CompletionToken.Event
                  );
  ASSERT_EFI_ERROR (Status);

  //
  // Configure the TCP instance
  //
  Status = mTcpListener->Configure (mTcpListener, &TcpConfigData);
  if (Status == EFI_NO_MAPPING) {
    // Wait until the IP configuration process (probably DHCP) has finished
    do {
      Status = mTcpListener->GetModeData (mTcpListener,
                               NULL, NULL,
                               &Ip4ModeData,
                               NULL, NULL
                               );
      ASSERT_EFI_ERROR (Status);
    } while (!Ip4ModeData.IsConfigured);
    Status = mTcpListener->Configure (mTcpListener, &TcpConfigData);
  } else if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "TCP Configure: %r\n", Status));
    return Status;
  }

  //
  // Tell the user our address and hostname
  //
  IP4_ADDR_TO_STRING (Ip4ModeData.ConfigData.StationAddress, IpAddrString);

  mTextOut->OutputString (mTextOut, L"TCP transport configured.\r\n");
  mTextOut->OutputString (mTextOut, L"IP address: ");
  mTextOut->OutputString (mTextOut ,IpAddrString);
  mTextOut->OutputString (mTextOut, L"\r\n");

  //
  // Start listening for a connection
  //
  Status = mTcpListener->Accept (mTcpListener, &mAcceptToken);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "TCP Accept: %r\n", Status));
    return Status;
  }

  mTextOut->OutputString (mTextOut, L"TCP transport initialised.\r\n");

  FreePool (HandleBuffer);

  Status = gBS->CreateEvent (0, TPL_CALLBACK, NULL, NULL, &mFinishedEvent);
  ASSERT_EFI_ERROR (Status);

  mTextOut->OutputString (mTextOut, L"Press any key to quit.\r\n");

  // Quit when the user presses any key, or mFinishedEvent is signalled
  WaitEventArray[0] = mFinishedEvent;
  WaitEventArray[1] = TextIn->WaitForKey;
  gBS->WaitForEvent (2, WaitEventArray, &EventIndex);

  return EFI_SUCCESS;
}

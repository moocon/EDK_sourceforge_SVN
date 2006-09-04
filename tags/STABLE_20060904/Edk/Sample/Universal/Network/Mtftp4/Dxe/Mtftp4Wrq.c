/*++

Copyright (c) 2006, Intel Corporation                                                         
All rights reserved. This program and the accompanying materials                          
are licensed and made available under the terms and conditions of the BSD License         
which accompanies this distribution.  The full text of the license may be found at        
http://opensource.org/licenses/bsd-license.php                                            
                                                                                          
THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,                     
WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED. 

Module Name:

  Mtftp4Wrq.c
 
Abstract:

  Routines to process Wrq (upload)
  
--*/

#include "Mtftp4Impl.h"

VOID
Mtftp4WrqInput (
  IN NET_BUF                *UdpPacket,
  IN UDP_POINTS             *Points,
  IN EFI_STATUS             IoStatus,
  IN VOID                   *Context
  );

EFI_STATUS
Mtftp4WrqStart (
  IN MTFTP4_PROTOCOL        *Instance,
  IN UINT16                 Operation
  )
/*++

Routine Description:

  Start the MTFTP session for pload. It will first init some states, 
  then send the WRQ request packet, and start receiving the  packet.

Arguments:

  Instance  - The MTFTP session
  Operation - Redundant parameter, which is always EFI_MTFTP4_OPCODE_WRQ here.

Returns:

  EFI_SUCCESS - The upload process has been started.
  Others      - Failed to start the upload.

--*/
{
  EFI_STATUS                Status;

  //
  // The valid block number range are [0, 0xffff]. For example:
  // the client sends an WRQ request to the server, the server
  // ACK with an ACK0 to let client start transfer the first
  // packet.
  //
  Status = Mtftp4InitBlockRange (&Instance->Blocks, 0, 0xffff);

  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = Mtftp4SendRequest (Instance);

  if (EFI_ERROR (Status)) {
    return Status;
  }

  return UdpIoRecvDatagram (Instance->UnicastPort, Mtftp4WrqInput, Instance, 0);
}

EFI_STATUS
Mtftp4WrqSendBlock (
  IN MTFTP4_PROTOCOL        *Instance,
  IN UINT16                 BlockNum
  )
/*++

Routine Description:

  Build then send a MTFTP data packet for the MTFTP upload session.

Arguments:

  Instance  - The MTFTP upload session
  BlockNum  - The block number to send

Returns:

  EFI_OUT_OF_RESOURCES - Failed to build the packet
  EFI_ABORTED          - The consumer of this child directs to abort the
                         transmission by return an error through PacketNeeded
  EFI_SUCCESS          - The data is sent.
                         
--*/
{
  EFI_MTFTP4_PACKET         *Packet;
  EFI_MTFTP4_TOKEN          *Token;
  NET_BUF                   *UdpPacket;
  EFI_STATUS                Status;
  UINT16                    DataLen;
  UINT8                     *DataBuf;
  UINT64                    Start;

  //
  // Allocate a buffer to hold the user data
  //
  UdpPacket = NetbufAlloc (Instance->BlkSize + MTFTP4_DATA_HEAD_LEN);

  if (UdpPacket == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  Packet = (EFI_MTFTP4_PACKET *)NetbufAllocSpace (UdpPacket, MTFTP4_DATA_HEAD_LEN, FALSE);

  Packet->Data.OpCode = HTONS (EFI_MTFTP4_OPCODE_DATA);
  Packet->Data.Block  = HTONS (BlockNum);

  //
  // Read the block from either the buffer or PacketNeeded callback
  //
  Token   = Instance->Token;
  DataLen = Instance->BlkSize;

  if (Token->Buffer != NULL) {
    Start = MultU64x32 (BlockNum - 1, Instance->BlkSize);

    if (Token->BufferSize < Start + Instance->BlkSize) {
      DataLen             = (UINT16) (Token->BufferSize - Start);
      Instance->LastBlock = BlockNum;
      Mtftp4SetLastBlockNum (&Instance->Blocks, BlockNum);
    }

    if (DataLen > 0) {
      NetbufAllocSpace (UdpPacket, DataLen, FALSE);
      NetCopyMem (Packet->Data.Data, (UINT8 *) Token->Buffer + Start, DataLen);
    }

  } else {
    //
    // Get data from PacketNeeded
    //
    DataBuf = NULL;
    Status  = Token->PacketNeeded (&Instance->Mtftp4, Token, &DataLen, &DataBuf);

    if (EFI_ERROR (Status) || (DataLen > Instance->BlkSize)) {
      if (DataBuf != NULL) {
        gBS->FreePool (DataBuf);
      }

      Mtftp4SendError (
        Instance, 
        EFI_MTFTP4_ERRORCODE_REQUEST_DENIED,
        "User aborted the transfer"
        );
      
      return EFI_ABORTED;
    }

    if (DataLen < Instance->BlkSize) {
      Instance->LastBlock = BlockNum;
      Mtftp4SetLastBlockNum (&Instance->Blocks, BlockNum);
    }

    if (DataLen > 0) {
      NetbufAllocSpace (UdpPacket, DataLen, FALSE);
      NetCopyMem (Packet->Data.Data, DataBuf, DataLen);
      gBS->FreePool (DataBuf);
    }  
  }

  return Mtftp4SendPacket (Instance, UdpPacket);
}

EFI_STATUS
Mtftp4WrqHandleAck (
  IN  MTFTP4_PROTOCOL       *Instance,
  IN  EFI_MTFTP4_PACKET     *Packet,
  IN  UINT32                Len,
  OUT BOOLEAN               *Completed
  )
/*++

Routine Description:

  Function to handle received ACK packet. If the ACK number matches the
  expected block number, and there are more data pending, send the next
  block. Otherwise tell the caller that we are done.

Arguments:

  Instance  - The MTFTP upload session
  Packet    - The MTFTP packet received
  Len       - The packet length
  Completed - Return whether the upload has finished.

Returns:

  EFI_SUCCESS    - The ACK is successfully processed.
  EFI_TFTP_ERROR - The block number loops back.
  Others         - Failed to transmit the next data packet.

--*/
{
  UINT16                    AckNum;
  INTN                      Expected;

  *Completed  = FALSE;
  AckNum      = NTOHS (Packet->Ack.Block[0]);
  Expected    = Mtftp4GetNextBlockNum (&Instance->Blocks);

  ASSERT (Expected >= 0);

  //
  // Get an unwanted ACK, return EFI_SUCCESS to let Mtftp4WrqInput 
  // restart receive.
  //
  if (Expected != AckNum) {
    return EFI_SUCCESS;
  }
  
  //
  // Remove the acked block number, if this is the last block number,
  // tell the Mtftp4WrqInput to finish the transfer. This is the last
  // block number if the block range are empty..
  //
  Mtftp4RemoveBlockNum (&Instance->Blocks, AckNum);

  Expected = Mtftp4GetNextBlockNum (&Instance->Blocks);

  if (Expected < 0) {
    //
    // The block range is empty. It may either because the the last
    // block has been ACKed, or the sequence number just looped back,
    // that is, there is more than 0xffff blocks.
    //
    if (Instance->LastBlock == AckNum) {
      ASSERT (Instance->LastBlock >= 1);
      *Completed = TRUE;
      return EFI_SUCCESS;

    } else {
      Mtftp4SendError (
        Instance, 
        EFI_MTFTP4_ERRORCODE_REQUEST_DENIED,
        "Block number rolls back, not supported, try blksize option"
        );
      
      return EFI_TFTP_ERROR;
    }
  }

  return Mtftp4WrqSendBlock (Instance, (UINT16) Expected);
}

BOOLEAN
Mtftp4WrqOackValid (
  IN MTFTP4_OPTION              *Reply,
  IN MTFTP4_OPTION              *Request
  )
/*++

Routine Description:

  Check whether the received OACK is valid. The OACK is valid
  only if:
    1. It only include options requested by us
    2. It can only include a smaller block size
    3. It can't change the proposed time out value.
    4. Other requirements of the individal MTFTP options as required.s

Arguments:

  Reply   - The options included in the OACK
  Request - The options we requested

Returns:

  TRUE if the options included in OACK is valid, otherwise FALSE.

--*/
{
  //
  // It is invalid for server to return options we don't request
  //
  if ((Reply->Exist &~Request->Exist) != 0) {
    return FALSE;
  }
  
  //
  // Server can only specify a smaller block size to be used and
  // return the timeout matches that requested.
  //
  if (((Reply->Exist & MTFTP4_BLKSIZE_EXIST) && (Reply->BlkSize > Request->BlkSize)) ||
      ((Reply->Exist & MTFTP4_TIMEOUT_EXIST) && (Reply->Timeout != Request->Timeout))) {
    return FALSE;
  }

  return TRUE;
}

EFI_STATUS
Mtftp4WrqHandleOack (
  IN  MTFTP4_PROTOCOL       *Instance,
  IN  EFI_MTFTP4_PACKET     *Packet,
  IN  UINT32                Len,
  OUT BOOLEAN               *Completed
  )
/*++

Routine Description:

  Function to handle the MTFTP OACK packet. It parses the packet's
  options, and update the internal states of the session

Arguments:

  Instance  - The MTFTP session 
  Packet    - The received OACK packet
  Len       - The length of the packet
  Completed - Whether the transmisson has completed. NOT used by this function.

Returns:

  EFI_SUCCESS    - The OACK process is OK
  EFI_TFTP_ERROR - Some error occured, and the session reset.

--*/
{
  MTFTP4_OPTION             Reply;
  EFI_MTFTP4_PACKET         Bogus;
  EFI_STATUS                Status;
  INTN                      Expected;

  *Completed = FALSE;

  //
  // Ignore the OACK if already started the upload
  //
  Expected = Mtftp4GetNextBlockNum (&Instance->Blocks);

  if (Expected != 0) {
    return EFI_SUCCESS;
  }
  
  //
  // Parse and validate the options from server
  //
  NetZeroMem (&Reply, sizeof (MTFTP4_OPTION));
  Status = Mtftp4ParseOptionOack (Packet, Len, &Reply);

  if (EFI_ERROR (Status) || !Mtftp4WrqOackValid (&Reply, &Instance->RequestOption)) {
    //
    // Don't send a MTFTP error packet when out of resource, it can 
    // only make it worse.
    //
    if (Status != EFI_OUT_OF_RESOURCES) {
      Mtftp4SendError (
        Instance, 
        EFI_MTFTP4_ERRORCODE_ILLEGAL_OPERATION,
        "Mal-formated OACK packet"
        );
    }

    return EFI_TFTP_ERROR;
  }

  if (Reply.BlkSize != 0) {
    Instance->BlkSize = Reply.BlkSize;
  }

  if (Reply.Timeout != 0) {
    Instance->Timeout = Reply.Timeout;
  }
  
  //
  // Build a bogus ACK0 packet then pass it to the Mtftp4WrqHandleAck,
  // which will start the transmission of the first data block. 
  //
  Bogus.Ack.OpCode    = HTONS (EFI_MTFTP4_OPCODE_ACK);
  Bogus.Ack.Block[0]  = 0;

  return Mtftp4WrqHandleAck (Instance, &Bogus, sizeof (EFI_MTFTP4_ACK_HEADER), Completed);
}

VOID
Mtftp4WrqInput (
  IN NET_BUF                *UdpPacket,
  IN UDP_POINTS             *Points,
  IN EFI_STATUS             IoStatus,
  IN VOID                   *Context
  )
/*++

Routine Description:

  The input process routine for MTFTP upload.

Arguments:

  UdpPacket - The received MTFTP packet.
  Points    - The local/remote access point
  IoStatus  - The result of the packet receiving
  Context   - Opaque parameter for the callback, which is the MTFTP session.

Returns:

  None

--*/
{
  MTFTP4_PROTOCOL           *Instance;
  EFI_MTFTP4_PACKET         *Packet;
  BOOLEAN                   Completed;
  EFI_STATUS                Status;
  UINT32                    Len;
  UINT16                    Opcode;

  Instance = (MTFTP4_PROTOCOL *) Context;
  NET_CHECK_SIGNATURE (Instance, MTFTP4_PROTOCOL_SIGNATURE);

  Completed = FALSE;
  Packet    = NULL;
  Status    = EFI_SUCCESS;

  if (EFI_ERROR (IoStatus)) {
    Status = IoStatus;
    goto ON_EXIT;
  }

  ASSERT (UdpPacket != NULL);

  if (UdpPacket->TotalSize < MTFTP4_OPCODE_LEN) {
    goto ON_EXIT;
  }
  
  //
  // Client send initial request to server's listening port. Server
  // will select a UDP port to communicate with the client.
  //
  if (Points->RemotePort != Instance->ConnectedPort) {
    if (Instance->ConnectedPort != 0) {
      goto ON_EXIT;
    } else {
      Instance->ConnectedPort = Points->RemotePort;
    }
  }
  
  //
  // Copy the MTFTP packet to a continuous buffer if it isn't already so.
  //
  Len = UdpPacket->TotalSize;

  if (UdpPacket->BlockOpNum > 1) {
    Packet = NetAllocatePool (Len);

    if (Packet == NULL) {
      Status = EFI_OUT_OF_RESOURCES;
      goto ON_EXIT;
    }

    NetbufCopy (UdpPacket, 0, Len, (UINT8 *) Packet);

  } else {
    Packet = (EFI_MTFTP4_PACKET *) NetbufGetByte (UdpPacket, 0, NULL);
  }

  Opcode = NTOHS (Packet->OpCode);

  //
  // Call the user's CheckPacket if provided. Abort the transmission
  // if CheckPacket returns an EFI_ERROR code.
  //
  if ((Instance->Token->CheckPacket != NULL) &&
      ((Opcode == EFI_MTFTP4_OPCODE_OACK) || (Opcode == EFI_MTFTP4_OPCODE_ERROR))) {

    Status = Instance->Token->CheckPacket (
                                &Instance->Mtftp4,
                                Instance->Token,
                                (UINT16) Len,
                                Packet
                                );

    if (EFI_ERROR (Status)) {
      //
      // Send an error message to the server to inform it
      //
      if (Opcode != EFI_MTFTP4_OPCODE_ERROR) {
        Mtftp4SendError (
          Instance,
          EFI_MTFTP4_ERRORCODE_REQUEST_DENIED, 
          "User aborted the transfer"
          );
      }

      Status = EFI_ABORTED;
      goto ON_EXIT;
    }
  }

  switch (Opcode) {
  case EFI_MTFTP4_OPCODE_ACK:
    if (Len != MTFTP4_OPCODE_LEN + MTFTP4_BLKNO_LEN) {
      goto ON_EXIT;
    }

    Status = Mtftp4WrqHandleAck (Instance, Packet, Len, &Completed);
    break;

  case EFI_MTFTP4_OPCODE_OACK:
    if (Len <= MTFTP4_OPCODE_LEN) {
      goto ON_EXIT;
    }

    Status = Mtftp4WrqHandleOack (Instance, Packet, Len, &Completed);
    break;

  case EFI_MTFTP4_OPCODE_ERROR:
    Status = EFI_TFTP_ERROR;
    break;
  }

ON_EXIT:
  //
  // Free the resources, then if !EFI_ERROR (Status) and not completed, 
  // restart the receive, otherwise end the session.
  //
  if ((Packet != NULL) && (UdpPacket->BlockOpNum > 1)) {
    NetFreePool (Packet);
  }

  if (UdpPacket != NULL) {
    NetbufFree (UdpPacket);
  }

  if (!EFI_ERROR (Status) && !Completed) {
    Status = UdpIoRecvDatagram (Instance->UnicastPort, Mtftp4WrqInput, Instance, 0);
  }

  //
  // Status may have been updated by UdpIoRecvDatagram
  //
  if (EFI_ERROR (Status) || Completed) {
    Mtftp4CleanOperation (Instance, Status);
  }
}

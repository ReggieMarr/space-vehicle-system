// ======================================================================
// \title  CCSDSTester.cpp
// \author Reginald Marr
// \brief  cpp file for CCSDSTester component implementation class
// ======================================================================

#include "FlightComputer/CCSDSTester/CCSDSTester.hpp"
#include "Drv/ByteStreamDriverModel/RecvStatusEnumAc.hpp"
#include "Drv/ByteStreamDriverModel/SendStatusEnumAc.hpp"
#include "Drv/Ip/IpSocket.hpp"
#include "FpConfig.h"
#include "FpConfig.hpp"
#include "Fw/Buffer/Buffer.hpp"
#include "Fw/Com/ComBuffer.hpp"
#include "Fw/Com/ComPacket.hpp"
#include "Fw/Logger/Logger.hpp"
#include "Fw/Types/Assert.hpp"
#include "Fw/Types/Serializable.hpp"
#include "Svc/FrameAccumulator/FrameDetector.hpp"
#include "Svc/FrameAccumulator/FrameDetector/CCSDSFrameDetector.hpp"
#include "Svc/FramingProtocol/CCSDSProtocols/TMSpaceDataLink/Channels.hpp"
#include "Svc/FramingProtocol/CCSDSProtocols/TMSpaceDataLink/ManagedParameters.hpp"
#include "Svc/FramingProtocol/CCSDSProtocols/TMSpaceDataLink/ProtocolInterface.hpp"
#include "Svc/FramingProtocol/CCSDSProtocols/TMSpaceDataLink/TransferFrame.hpp"
#include "Utils/Types/CircularBuffer.hpp"
#include <cstddef>
#include <iomanip> // for std::hex and std::setfill
#include <iostream>

namespace FlightComputer {
// Helper to serialize data into a ComBuffer
static Fw::Buffer
createSerializedBuffer(const FwPacketDescriptorType packetType,
                       const std::array<U8, 7> &data,
                       Fw::ComBuffer &comBuffer) {

  comBuffer.resetSer();
  comBuffer.serialize(packetType);
  comBuffer.serialize(data.data(), data.size(), true);
  return Fw::Buffer(comBuffer.getBuffAddr(), comBuffer.getBuffCapacity());
}

// Helper to send a message through ProtocolEntity
static void sendMessage(TMSpaceDataLink::ProtocolEntity &protocolEntity,
                        Fw::Buffer &messageBuffer, const U8 vcIdx) {

  // Only one master channel supported
  TMSpaceDataLink::PhysicalChannelParams_t params =
      protocolEntity.m_params.physicalParams;
  FW_ASSERT(params.numSubChannels == 1, params.subChannels.size());
  constexpr FwSizeType masterChannelIdx = 0;
  TMSpaceDataLink::MCID_t mcid = {
      .SCID = params.subChannels.at(masterChannelIdx).spaceCraftId,
      .TFVN = params.transferFrameVersion};
  TMSpaceDataLink::GVCID_t gvcid = {.MCID = mcid,
                                    .VCID =
                                        params.subChannels.at(masterChannelIdx)
                                            .subChannels.at(vcIdx)
                                            .virtualChannelId};
  U32 gvcidVal;
  TMSpaceDataLink::GVCID_t::toVal(gvcid, gvcidVal);

  // NOTE manually offsetting to data for now
  FwSizeType messageSendSize =
      params.subChannels.at(0).subChannels.at(vcIdx).VCA_SDULength;
  std::string messageText(
      reinterpret_cast<const char *>(messageBuffer.getData() +
                                     sizeof(FwPacketDescriptorType)),
      messageSendSize);
  std::cout << "Sending: [" << messageText << "]" << std::endl;

  protocolEntity.UserComIn_handler(messageBuffer, gvcidVal);
}

// Helper to generate and print a response
// NOTE this is just some test code for now
static void
generateAndPrintResponse(TMSpaceDataLink::ProtocolEntity &protocolEntity,
                         Fw::ComBuffer &comResponse, const U8 vcIdx) {

  comResponse.resetSer();
  comResponse.setBuffLen(TMSpaceDataLink::FPrimeTransferFrame::SERIALIZED_SIZE);
  Fw::Buffer response(comResponse.getBuffAddr(), comResponse.getBuffCapacity());
  protocolEntity.generateNextFrame(response);
  TMSpaceDataLink::FPrimeTransferFrame frame;

  Fw::SerializeBufferBase &serBuff = response.getSerializeRepr();
  serBuff.setBuffLen(serBuff.getBuffCapacity());

  frame.extract(serBuff);
  TMSpaceDataLink::FPrimeDataField::DataField::FieldValue_t dataVal;
  frame.dataField.get(dataVal);

  // Print the response in hexadecimal format
  std::cout << "Response for VCID Idx " << static_cast<int>(vcIdx) << ": ";

  TMSpaceDataLink::PhysicalChannelParams_t params =
      protocolEntity.m_params.physicalParams;
  FwSizeType responseSize =
      params.subChannels.at(0).subChannels.at(vcIdx).VCA_SDULength;
  FW_ASSERT(responseSize <= dataVal.size());

  std::string responseText(reinterpret_cast<const char *>(
                               dataVal.data() + sizeof(FwPacketDescriptorType)),
                           responseSize);
  std::cout << "Received:[" << responseText << "]" << std::endl;

  for (size_t i = 0; i < response.getSize(); ++i) {
    std::cout << std::hex << std::uppercase << std::setfill('0') << std::setw(2)
              << static_cast<int>(response.getData()[i]) << " ";
  }
  std::cout << std::endl;
}

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

CCSDSTester ::CCSDSTester(const char *const compName)
    : CCSDSTesterComponentBase(compName) {

  if (isConnected_drvReady_OutputPort(0)) {
    this->drvReady_out(0);
    isConnected = true;
  } else {
    Fw::Logger::log("Not Ready");
  }
}

CCSDSTester ::~CCSDSTester() {}

void CCSDSTester::seqCmdBuff_handler(NATIVE_INT_TYPE portNum,
                                     Fw::ComBuffer &data, U32 context) {
  const FwSizeType size = data.getBuffLength();
  Fw::Logger::log("Received command buffer of size %d with context %d\n", size,
                  context);

  if (size < sizeof(FwOpcodeType)) {
    Fw::Logger::log("Buff too small to decode\n");
  }

  // Command opcode is typically the first field
  // FwOpcodeType opcode;
  FwOpcodeType opCode;
  Fw::SerializeStatus stat = data.deserialize(opCode);

  if (stat != Fw::FW_SERIALIZE_OK) {
    Fw::Logger::log("ERROR: Failed to deserialize opcode\n");
    return;
  }

  // Print opcode in hex
  Fw::Logger::log("Command Opcode: 0x%x\n", opCode);

  // Get remaining buffer for command arguments
  U8 *buffPtr = data.getBuffAddr();

  if (stat != Fw::FW_SERIALIZE_OK) {
    Fw::Logger::log("ERROR: Failed to get command arguments\n");
    return;
  }

  // Print command arguments as hex dump
  Fw::Logger::log("Command Arguments (%d bytes):", size);

  for (U32 i = 0; i < size; i++) {
    if (i % 16 == 0) {
      Fw::Logger::log("\n%04X: ", i);
    }
    Fw::Logger::log("%02X ", buffPtr[i]);
  }
  Fw::Logger::log("\n");

  // TODO currently we just allocate on the stack for testing
  // Add an allocator to this component and then use this to deallocate
  // FW_ASSERT(isConnected_bufferDeallocate_OutputPort(portNum));
  // Fw::Buffer buff(data.getBuffAddr(), data.getBuffCapacity());
  // bufferDeallocate_out(portNum, buff);
}

void CCSDSTester::bufferSendIn_handler(const NATIVE_INT_TYPE portNum,
                                       Fw::Buffer &fwBuffer) {
  Fw::Logger::log("Received File Buffer\n");
  const FwSizeType size = fwBuffer.getSize();
  U8 *ptr = fwBuffer.getData();
  for (U32 i = 0; i < size; i++) {
    if (i % 16 == 0) {
      Fw::Logger::log("\n%04X: ", i);
    }
    Fw::Logger::log("%02X ", ptr[i]);
  }
  Fw::Logger::log("\n");

  FW_ASSERT(isConnected_bufferDeallocate_OutputPort(portNum));
  bufferDeallocate_out(portNum, fwBuffer);
}

void CCSDSTester::comStatusIn_handler(
    FwIndexType portNum,   //!< The port number
    Fw::Success &condition //!< Condition success/failure
) {
  // if (this->isConnected_comStatusOut_OutputPort(portNum)) {
  //     this->comStatusOut_out(portNum, condition);
  // }
}

void CCSDSTester::framerLoopbackPing() {
  if (!isConnected && isConnected_drvReady_OutputPort(0)) {
    this->drvReady_out(0);
    isConnected = true;
  } else {
    Fw::Logger::log("CCSDS Tester Not Ready !!\n");
  }
  Fw::ComBuffer com;

  U32 dfltMessage = 0x9944fead;
  com.resetSer();
  Fw::ComPacket::ComPacketType packetType =
      Fw::ComPacket::ComPacketType::FW_PACKET_COMMAND;
  com.serialize(packetType);
  com.serialize(dfltMessage);

  PktSend_out(0, com, 0);
}

void CCSDSTester::protocolEntityLoopBackPing() {
  static constexpr FwSizeType MessageNum = NUM_VIRTUAL_CHANNELS;
  static constexpr FwSizeType MessageSize =
      7; // Enough to fit all, with padding for some
  std::array<std::array<U8, MessageSize>, MessageNum> channelMessages = {{
      {'B', 'O', 'N', 'J', 'O', 'U', 'R'}, // Channel 0: "BONJOUR"
      {'S', 'A', 'L', 'U', 'T', 0, 0},  // Channel 1: "SALUT" (padded with 0s)
      {0xF0, 0x9F, 0x8D, 0x81, 0, 0, 0} // Channel 2: Oh Canada (padded with 0s)
  }};
  std::array<TMSpaceDataLink::VirtualChannelParams_t, MessageNum> vcParams = {
      {{
           0,           // virtualChannelId = 0,
           MessageSize, // VCA_SDULength = 7 (channelMessages max size),
           0,           // VC_FSHLength = 0,
           false,       // isVC_OCFPresent = false,
       },
       {
           1,           // virtualChannelId = 1,
           MessageSize, // VCA_SDULength = 7 (channelMessages max size),
           0,           // VC_FSHLength = 0,
           false,       // isVC_OCFPresent = false,
       },
       {
           2,           // virtualChannelId = 2,
           MessageSize, // VCA_SDULength = 7 (channelMessages max size),
           0,           // VC_FSHLength = 0,
           false,       // isVC_OCFPresent = false,
       }}};

  TMSpaceDataLink::MasterChannelParams_t masterChannelParams = {
      .spaceCraftId = CCSDS_SCID,
      .numSubChannels = 3,
      .subChannels = vcParams,
      .vcMuxScheme = VC_MUX_TYPE::VC_MUX_TIME_DIVSION,
      .MC_FSHLength = 0,
      .isMC_OCFPresent = false,
  };

  TMSpaceDataLink::PhysicalChannelParams_t physicalChannelParams = {
      .channelName = "Loopback Channel",
      .transferFrameSize = 255,
      .transferFrameVersion = 0x00,
      .numSubChannels = 1,
      .subChannels = {masterChannelParams},
      .mcMuxScheme = MC_MUX_TYPE::MC_MUX_TIME_DIVSION,
      .isFrameErrorControlEnabled = true,
  };

  TMSpaceDataLink::ManagedParameters_t managedParams = {
      .physicalParams = physicalChannelParams,
  };

  TMSpaceDataLink::ProtocolEntity protocolEntity(managedParams);

  std::array<Fw::ComBuffer, MessageNum> comBuffers;
  std::array<Fw::Buffer, MessageNum> plainBuffers;
  std::array<FwPacketDescriptorType, MessageNum> packetTypeVals = {
      Fw::ComPacket::ComPacketType::FW_PACKET_COMMAND,
      Fw::ComPacket::ComPacketType::FW_PACKET_TELEM,
      Fw::ComPacket::ComPacketType::FW_PACKET_PACKETIZED_TLM,
  };

  for (int i = 0; i < MessageNum; i++) {
    plainBuffers.at(i) = createSerializedBuffer(
        packetTypeVals.at(i), channelMessages.at(i), comBuffers.at(i));
    Fw::Logger::log("Sending %d\n", i);
    sendMessage(protocolEntity, plainBuffers.at(i), i);
    Fw::Logger::log("Sent %d\n", i);
  }

  std::nullptr_t null_arg = nullptr;
  // Transfer data after all messages have been sent
  protocolEntity.m_physicalChannel.m_subChannels.at(0).transfer(null_arg);
  Fw::Logger::log("master tranfered \n");

  // Generate and handle responses
  std::array<Fw::ComBuffer, MessageNum> responses;
  for (int i = 0; i < MessageNum; i++) {
    generateAndPrintResponse(protocolEntity, responses.at(i), i);
  }
}

void CCSDSTester::PING_cmdHandler(const FwOpcodeType opCode, const U32 cmdSeq) {

  protocolEntityLoopBackPing();

  // cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}
void CCSDSTester::MESSAGE_cmdHandler(const FwOpcodeType opCode,
                                     const U32 cmdSeq,
                                     const Fw::CmdStringArg &str1) {
  if (!isConnected && isConnected_drvReady_OutputPort(0)) {
    this->drvReady_out(0);
    isConnected = true;
  } else {
    Fw::Logger::log("Not Ready");
  }
  Fw::ComBuffer com;

  com.resetSer();

  // U32 starter = 0xFFFF;
  // com.serialize(starter);
  U8 packetType = Fw::ComPacket::ComPacketType::FW_PACKET_FILE;

  com.serialize(packetType);
  com.serialize(str1);

  // TODO add support for checking the port's connected
  PktSend_out(0, com, 0);
  // cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

Drv::SendStatus CCSDSTester::drvSend_handler(FwIndexType portNum,
                                             Fw::Buffer &buffer) {

  Types::CircularBuffer circBoi(buffer.getData(), buffer.getSize());
  Fw::SerializeStatus stat =
      circBoi.serialize(buffer.getData(), buffer.getSize());
  circBoi.print();
  U8 btt = 0;
  circBoi.peek(btt, 0);
  Fw::Logger::log("circBoi %x %d alloc %d cap %d\n", btt, stat, "THis is bad",
                  circBoi.get_allocated_size(), circBoi.get_capacity());

  Fw::Logger::log("I dont now");

  circBoi.print();

  // Svc::FrameDetector::Status status =
  // Svc::FrameDetector::Status::FRAME_DETECTED;
  // Svc::FrameDetectors::TMSpaceDataLinkDetector ccsdsFrameDetector;

  // FwSizeType size_out = 0;
  // status = ccsdsFrameDetector.detect(circBoi, size_out);
  // Fw::Logger::log("Status %d out %d\n", status, size_out);

  if (!isConnected_drvRcv_OutputPort(portNum)) {
    return Drv::SendStatus::SEND_ERROR;
  }

  drvRcv_out(0, buffer, Drv::RecvStatus::RECV_OK);

  return Drv::SendStatus::SEND_OK;
}

} // namespace FlightComputer

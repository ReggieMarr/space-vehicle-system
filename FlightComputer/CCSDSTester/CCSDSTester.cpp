// ======================================================================
// \title  CCSDSTester.cpp
// \author Reginald Marr
// \brief  cpp file for CCSDSTester component implementation class
// ======================================================================

#include "FlightComputer/CCSDSTester/CCSDSTester.hpp"
#include "Drv/ByteStreamDriverModel/RecvStatusEnumAc.hpp"
#include "Drv/ByteStreamDriverModel/SendStatusEnumAc.hpp"
#include "FpConfig.h"
#include "FpConfig.hpp"
#include "Fw/Buffer/Buffer.hpp"
#include "Fw/Com/ComBuffer.hpp"
#include "Fw/Com/ComPacket.hpp"
#include "Fw/Logger/Logger.hpp"
#include "Fw/Types/Assert.hpp"
#include "Fw/Types/Serializable.hpp"
#include "Svc/FrameAccumulator/FrameDetector/CCSDSFrameDetector.hpp"
#include "Svc/FramingProtocol/CCSDSProtocols/TMSpaceDataLink/Channels.hpp"
#include "Svc/FramingProtocol/CCSDSProtocols/TMSpaceDataLink/ManagedParameters.hpp"
#include "Svc/FramingProtocol/CCSDSProtocols/TMSpaceDataLink/ProtocolInterface.hpp"
#include "Svc/FramingProtocol/CCSDSProtocols/TMSpaceDataLink/TransferFrame.hpp"
#include "Utils/Types/CircularBuffer.hpp"
#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <iomanip> // for std::hex and std::setfill
#include <iostream>
#include <iterator>
#include <string>
#include "CCSDSConfig.hpp"

namespace FlightComputer {
// Helper to send a message through ProtocolEntity

static std::string createJsonMessage(const loopbackMsgHeader_t& header,
                                     const std::string& channelMessage) {
    // Verify size constraint
    char jsonCStr[MessageSize];

    std::snprintf(jsonCStr, sizeof(jsonCStr)/sizeof(jsonCStr[0]),
                  "{opCode: %u, cmdSeq: %u, testCnt: %u, pld: '%s'}",
                  header.opCode, header.cmdSeq, header.testCnt, channelMessage.c_str());
    std::string json(jsonCStr, sizeof(jsonCStr)/sizeof(jsonCStr[0]));
    json.shrink_to_fit();

    return json;
}


// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

// Helper to serialize data into a ComBuffer
CCSDSTester ::CCSDSTester(const char *const compName)
: CCSDSTesterComponentBase(compName), m_protocolEntity(createProtocolEntity()) {

  if (isConnected_drvReady_OutputPort(0)) {
    this->drvReady_out(0);
    this->m_IsConnected = true;
  } else {
    Fw::Logger::log("Not Ready");
  }
}

CCSDSTester ::~CCSDSTester() {}

Fw::Buffer CCSDSTester::createSerializedBuffer(const FwPacketDescriptorType packetType,
                                               const std::array<U8, MessageSize> &data,
                                               Fw::ComBuffer &comBuffer) {
  comBuffer.resetSer();
  comBuffer.serialize(packetType);
  comBuffer.serialize(data.data(), data.size(), true);
  return Fw::Buffer(comBuffer.getBuffAddr(), comBuffer.getBuffCapacity());
}

void CCSDSTester::routeMessage(Fw::Buffer &messageBuffer, const U8 vcIdx) {

  // Only one master channel supported
  TMSpaceDataLink::PhysicalChannelParams_t params =
      this->m_protocolEntity.m_params.physicalParams;
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

  this->m_protocolEntity.UserComIn_handler(messageBuffer, gvcidVal);
}

// Helper to generate and print a response
// NOTE this is just some test code for now
void CCSDSTester::runPipeline() {
  for (NATIVE_UINT_TYPE vcIdx = 0; vcIdx < MessageNum; vcIdx++) {
    this->m_pipelineBuffer.resetSer();
    this->m_pipelineBuffer.setBuffLen(TMSpaceDataLink::FPrimeTransferFrame::SERIALIZED_SIZE);

    Fw::Buffer response(this->m_pipelineBuffer.getBuffAddr(), this->m_pipelineBuffer.getBuffCapacity());
    this->m_protocolEntity.generateNextFrame(response);

    Fw::SerializeBufferBase &serBuff = response.getSerializeRepr();
    serBuff.setBuffLen(serBuff.getBuffCapacity());

    TMSpaceDataLink::FPrimeTransferFrame frame;
    frame.extract(serBuff);
    frame.dataField.print();
  }
}

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

void CCSDSTester::run_handler(
      const NATIVE_INT_TYPE portNum,
      NATIVE_UINT_TYPE context
) {
  // If we haven't enabled running the pipeline then return
  if (!this->m_ShouldRunPipeline) {
    return;
  }

  runPipeline();
}

void CCSDSTester::sendLoopbackMsg(loopbackMsgHeader_t &header) {
  std::array<Fw::ComBuffer, MessageNum> comBuffers;
  std::array<Fw::Buffer, MessageNum> plainBuffers;

  std::array<FwPacketDescriptorType, MessageNum> packetTypeVals = {
      Fw::ComPacket::ComPacketType::FW_PACKET_COMMAND,
      Fw::ComPacket::ComPacketType::FW_PACKET_TELEM,
      Fw::ComPacket::ComPacketType::FW_PACKET_PACKETIZED_TLM,
  };

  for (int i = 0; i < MessageNum; i++) {
    std::string msg(ChannelMsgs.at(i).begin(), ChannelMsgs.at(i).end());
    std::string msgJson(createJsonMessage(header, msg));
    std::array<U8, MessageSize> msgJsonArr;
    std::copy(msgJson.begin(), msgJson.end(), msgJsonArr.data());

    // plainBuffers.at(i).set(0, plainBuffers.at(i).getSize());

    plainBuffers.at(i) = createSerializedBuffer(packetTypeVals.at(i), msgJsonArr, comBuffers.at(i));
    routeMessage(plainBuffers.at(i), i);
  }

  std::nullptr_t null_arg = nullptr;
  // Transfer data after all messages have been sent
  this->m_protocolEntity.m_physicalChannel.m_subChannels.at(0).transfer(null_arg);

  // // Generate and handle responses
  // runPipeline();
}

void CCSDSTester::RUN_PIPELINE_cmdHandler(const FwOpcodeType opCode, const U32 cmdSeq) {
  runPipeline();
}

void CCSDSTester::TOGGLE_RUN_PIPELINE_cmdHandler(const FwOpcodeType opCode, const U32 cmdSeq) {
  this->m_ShouldRunPipeline = !this->m_ShouldRunPipeline;
}

void CCSDSTester::PING_cmdHandler(const FwOpcodeType opCode, const U32 cmdSeq) {

  loopbackMsgHeader_t msgHeader{opCode, cmdSeq, this->m_MsgCnt++};
  sendLoopbackMsg(msgHeader);

  cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

void CCSDSTester::MESSAGE_cmdHandler(const FwOpcodeType opCode,
                                     const U32 cmdSeq,
                                     const Fw::CmdStringArg &str1) {
  if (!this->m_IsConnected && isConnected_drvReady_OutputPort(0)) {
    this->drvReady_out(0);
    this->m_IsConnected = true;
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

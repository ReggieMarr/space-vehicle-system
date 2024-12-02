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
#include "Utils/Types/CircularBuffer.hpp"

namespace FlightComputer {

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
  TMSpaceDataLink::VirtualChannelParams_t virtualChannelParams = {
      .virtualChannelId = 0,
      .VCA_SDULength = 4,
      .VC_FSHLength = 0,
      .isVC_OCFPresent = false,
  };

  TMSpaceDataLink::MasterChannelParams_t masterChannelParams = {
      .spaceCraftId = CCSDS_SCID,
      .numSubChannels = 1,
      .subChannels = {virtualChannelParams},
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

  Fw::ComBuffer com;

  U32 dfltMessage = 0x9944fead;
  com.resetSer();
  Fw::ComPacket::ComPacketType packetType =
      Fw::ComPacket::ComPacketType::FW_PACKET_COMMAND;
  com.serialize(packetType);
  com.serialize(dfltMessage);
  U32 gvcidVal;
  TMSpaceDataLink::MCID_t mcid = {
      .SCID = CCSDS_SCID,
      .TFVN = 0x00,
  };
  TMSpaceDataLink::GVCID_t gvcidSrc = {
      .MCID = mcid,
      .VCID = 0,
  };
  TMSpaceDataLink::GVCID_t gvcidDst;
  TMSpaceDataLink::GVCID_t::toVal(gvcidSrc, gvcidVal);
  TMSpaceDataLink::GVCID_t::fromVal(gvcidDst, gvcidVal);
  FW_ASSERT(gvcidSrc == gvcidDst);

  Fw::Buffer buff(com.getBuffAddr(), com.getBuffCapacity());
  TMSpaceDataLink::ProtocolEntity protocolEntity(managedParams);
  protocolEntity.UserComIn_handler(buff, gvcidVal);
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

// ======================================================================
// \title  CCSDSTester.cpp
// \author Reginald Marr
// \brief  cpp file for CCSDSTester component implementation class
// ======================================================================

#include "FlightComputer/CCSDSTester/CCSDSTester.hpp"
#include "Drv/ByteStreamDriverModel/RecvStatusEnumAc.hpp"
#include "Drv/Ip/IpSocket.hpp"
#include "FpConfig.h"
#include "FpConfig.hpp"
#include "Fw/Com/ComBuffer.hpp"
#include "Fw/Logger/Logger.hpp"

namespace FlightComputer {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

CCSDSTester ::CCSDSTester(const char *const compName)
    : CCSDSTesterComponentBase(compName) {
  this->drvReady_out(0);
}

CCSDSTester ::~CCSDSTester() {}

void CCSDSTester::seqCmdBuff_handler(NATIVE_INT_TYPE portNum, Fw::ComBuffer &data, U32 context) {
    const FwSizeType size = data.getBuffLength();
    Fw::Logger::log("Received command buffer of size %d with context %d\n", size, context);

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
    U8* buffPtr = data.getBuffAddr();

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

    // Send command response (success)
    // this->cmdResponseIn_out(0, context, Fw::CmdResponse::OK);
}

void CCSDSTester::bufferSendIn_handler(const NATIVE_INT_TYPE portNum,
                                       Fw::Buffer &fwBuffer) {
  Fw::Logger::log("Handling ccsds buffer");
  // Pass through for now
  this->bufferSendOut_out(0, fwBuffer);
}

// void CCSDSTester ::dataIn_handler(FwIndexType portNum, Fw::Buffer& buffer,
// const Drv::RecvStatus& status) {
//     // Check whether there is data to process

//     // Fw::Logger::log("FrameAccumulator often send");
//     // if (status.e == Drv::RecvStatus::RECV_OK) {
//     //     // There is: process the data
//     //     this->processBuffer(buffer);
//     // }
//     // // Deallocate the buffer
//     // this->dataDeallocate_out(0, buffer);
// }
void CCSDSTester::comStatusIn_handler(
    FwIndexType portNum,   //!< The port number
    Fw::Success &condition //!< Condition success/failure
) {
  // if (this->isConnected_comStatusOut_OutputPort(portNum)) {
  //     this->comStatusOut_out(portNum, condition);
  // }
}

void CCSDSTester::PING_cmdHandler(const FwOpcodeType opCode, const U32 cmdSeq) {

  U32 dfltMessage = 0xdeadbeef;
  com.resetSer();
  com.serialize(dfltMessage);
  this->PktSend_out(0, com, 0);

  this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}
void CCSDSTester::MESSAGE_cmdHandler(const FwOpcodeType opCode,
                                     const U32 cmdSeq,
                                     const Fw::CmdStringArg &str1) {
  Fw::ComBuffer com;
  com.serialize(str1);
  this->PktSend_out(0, com, 0);

  this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

  Drv::SendStatus CCSDSTester::drvSend_handler(FwIndexType, Fw::Buffer&) {

  }

} // namespace FlightComputer

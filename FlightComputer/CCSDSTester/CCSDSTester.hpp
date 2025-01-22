// ======================================================================
// \title  CCSDSTester.hpp
// \author Reginald Marr
// \brief  hpp file for CCSDSTester component implementation class
// ======================================================================

#ifndef FlightComputer_CCSDSTester_HPP
#define FlightComputer_CCSDSTester_HPP

#include "Drv/ByteStreamDriverModel/SendStatusEnumAc.hpp"
#include "Drv/Ip/IpSocket.hpp"
#include "FlightComputer/CCSDSTester/CCSDSTesterComponentAc.hpp"
#include "Fw/Cmd/CmdString.hpp"
#include "Fw/Logger/Logger.hpp"
#include "Fw/Types/String.hpp"
#include "Svc/FramingProtocol/CCSDSProtocols/TMSpaceDataLink/ManagedParameters.hpp"
#include "Svc/FramingProtocol/CCSDSProtocols/TMSpaceDataLink/ProtocolInterface.hpp"
#include "CCSDSConfig.hpp"

namespace FlightComputer {

class CCSDSTester : public CCSDSTesterComponentBase {

public:
  // ----------------------------------------------------------------------
  // Component construction and destruction
  // ----------------------------------------------------------------------

  //! Construct CCSDSTester object
  CCSDSTester(const char *const compName //!< The component name
  );

  //! Destroy CCSDSTester object
  ~CCSDSTester();

private:
  TMSpaceDataLink::ProtocolEntity m_protocolEntity;

  U32 m_MsgCnt = 0;
  bool m_IsConnected = false;

  void bufferSendIn_handler(const NATIVE_INT_TYPE portNum,
                            Fw::Buffer &fwBuffer);
  Fw::Buffer createSerializedBuffer(const FwPacketDescriptorType packetType,
                                    const std::array<U8, MessageSize> &data,
                                    Fw::ComBuffer &comBuffer);
  void sendLoopbackMsg(loopbackMsgHeader_t &header);
  void routeMessage(Fw::Buffer &messageBuffer, const U8 vcIdx);
  void runPipeline(Fw::ComBuffer &comResponse, const U8 vcIdx);

  // Handler functions
  void seqCmdBuff_handler(NATIVE_INT_TYPE portNum, Fw::ComBuffer &data,
                          U32 context);
  void comStatusIn_handler(FwIndexType portNum,   //!< The port number
                           Fw::Success &condition //!< Condition success/failure
  );

  Drv::SendStatus drvSend_handler(FwIndexType, Fw::Buffer &);
  void PING_cmdHandler(const FwOpcodeType opCode, const U32 cmdSeq);
  void MESSAGE_cmdHandler(const FwOpcodeType opCode, const U32 cmdSeq,
                          const Fw::CmdStringArg &str1);
};
} // namespace FlightComputer

#endif

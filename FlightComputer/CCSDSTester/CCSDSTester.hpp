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
#include <array>

namespace FlightComputer {

typedef struct {
  const FwOpcodeType opCode;
  const U32 cmdSeq;
  const U32 testCnt;
} loopbackMsgHeader_t;
static constexpr FwSizeType MessageNum = NUM_VIRTUAL_CHANNELS;
static constexpr FwSizeType MessageSize = 55;

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
  bool isConnected = false;
  void bufferSendIn_handler(const NATIVE_INT_TYPE portNum,
                            Fw::Buffer &fwBuffer);

  //!  \brief component command buffer handler
  //!
  //!  The command buffer handler is called to submit a new
  //!  command packet to be decoded
  //!
  //!  \param portNum the number of the incoming port.
  //!  \param data the buffer containing the command.
  //!  \param context a user value returned with the status
  void seqCmdBuff_handler(NATIVE_INT_TYPE portNum, Fw::ComBuffer &data,
                          U32 context);
  void comStatusIn_handler(FwIndexType portNum,   //!< The port number
                           Fw::Success &condition //!< Condition success/failure
  );

  Drv::SendStatus drvSend_handler(FwIndexType, Fw::Buffer &);

  U32 msgCnt = 0;

  // Config
  // Assumes largest message looks like this "{opCode: 123, testCnt: 1234, cmdSeq: 1234, pld: '12345678'}"
  // Provides enough space for that + padding for others
  std::array<std::array<U8, MessageSize>, MessageNum> channelMsgs = {{
      {'B', 'O', 'N', 'J', 'O', 'U', 'R'}, // Channel 0: "BONJOUR"
      {'S', 'A', 'L', 'U', 'T', 0, 0},  // Channel 1: "SALUT" (padded with 0s)
      {0xF0, 0x9F, 0x8D, 0x81, 0, 0, 0} // Channel 2: Oh Canada (padded with 0s)
  }};
  static constexpr std::array<TMSpaceDataLink::VirtualChannelParams_t, MessageNum> vcParams = {
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

  const TMSpaceDataLink::MasterChannelParams_t masterChannelParams = {
      .spaceCraftId = CCSDS_SCID,
      .numSubChannels = 3,
      .subChannels = vcParams,
      .vcMuxScheme = VC_MUX_TYPE::VC_MUX_TIME_DIVSION,
      .MC_FSHLength = 0,
      .isMC_OCFPresent = false,
  };

  const TMSpaceDataLink::PhysicalChannelParams_t physicalChannelParams = {
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


  // Commands
  Fw::Buffer createSerializedBuffer(const FwPacketDescriptorType packetType,
                                    const std::array<U8, MessageSize> &data,
                                    Fw::ComBuffer &comBuffer);
  void sendLoopbackMsg(loopbackMsgHeader_t &header);
  void PING_cmdHandler(const FwOpcodeType opCode, const U32 cmdSeq);
  void MESSAGE_cmdHandler(const FwOpcodeType opCode, const U32 cmdSeq,
                          const Fw::CmdStringArg &str1);
};
} // namespace FlightComputer

#endif

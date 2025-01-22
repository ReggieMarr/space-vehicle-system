#ifndef FlightComputer_CCSDSCONFIG_HPP
#define FlightComputer_CCSDSCONFIG_HPP
#include "FpConfig.h"
#include "Svc/FramingProtocol/CCSDSProtocols/TMSpaceDataLink/ManagedParameters.hpp"
#include "Svc/FramingProtocol/CCSDSProtocols/TMSpaceDataLink/ProtocolInterface.hpp"
#include <array>

namespace FlightComputer {

typedef struct {
  const FwOpcodeType opCode;
  const U32 cmdSeq;
  const U32 testCnt;
} loopbackMsgHeader_t;

constexpr FwSizeType MessageNum = NUM_VIRTUAL_CHANNELS;
constexpr FwSizeType MessageSize = 55;

// Config
// Assumes largest message looks like this "{opCode: 123, testCnt: 1234, cmdSeq: 1234, pld: '12345678'}"
// Provides enough space for that + padding for others
constexpr std::array<std::array<U8, MessageSize>, MessageNum> ChannelMsgs = {{
    {'B', 'O', 'N', 'J', 'O', 'U', 'R'}, // Channel 0: "BONJOUR"
    {'S', 'A', 'L', 'U', 'T', 0, 0},  // Channel 1: "SALUT" (padded with 0s)
    {0xF0, 0x9F, 0x8D, 0x81, 0, 0, 0} // Channel 2: Oh Canada (padded with 0s)
}};

TMSpaceDataLink::ProtocolEntity createProtocolEntity();

} // namespace FlightComputer
#endif

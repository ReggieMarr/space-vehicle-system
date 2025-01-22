#include "CCSDSConfig.hpp"
#include "FpConfig.h"
#include "Svc/FramingProtocol/CCSDSProtocols/TMSpaceDataLink/ManagedParameters.hpp"
#include "Svc/FramingProtocol/CCSDSProtocols/TMSpaceDataLink/ProtocolInterface.hpp"
#include <array>

namespace FlightComputer {

// std::array<std::array<U8, MessageSize>, MessageNum> ChannelMsgs = {{
//     {'B', 'O', 'N', 'J', 'O', 'U', 'R'}, // Channel 0: "BONJOUR"
//     {'S', 'A', 'L', 'U', 'T', 0, 0},  // Channel 1: "SALUT" (padded with 0s)
//     {0xF0, 0x9F, 0x8D, 0x81, 0, 0, 0} // Channel 2: Oh Canada (padded with 0s)
// }};

static constexpr std::array<TMSpaceDataLink::VirtualChannelParams_t, MessageNum> VCParams = {
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

static constexpr TMSpaceDataLink::MasterChannelParams_t MasterChannelParams = {
    .spaceCraftId = CCSDS_SCID,
    .numSubChannels = 3,
    .subChannels = VCParams,
    .vcMuxScheme = VC_MUX_TYPE::VC_MUX_TIME_DIVSION,
    .MC_FSHLength = 0,
    .isMC_OCFPresent = false,
};

TMSpaceDataLink::ProtocolEntity createProtocolEntity() {

  // NOTE this must be defined on the stack as a consequence of using the non-trivial type Fw::String
  // TODO consider removing that constraint
  const TMSpaceDataLink::PhysicalChannelParams_t PhysicalChannelParams = {
      .channelName = "Loopback Channel",
      .transferFrameSize = 255,
      .transferFrameVersion = 0x00,
      .numSubChannels = 1,
      .subChannels = {MasterChannelParams},
      .mcMuxScheme = MC_MUX_TYPE::MC_MUX_TIME_DIVSION,
      .isFrameErrorControlEnabled = true,
  };

  TMSpaceDataLink::ManagedParameters_t ManagedParams = {
      .physicalParams = PhysicalChannelParams,
  };

  return TMSpaceDataLink::ProtocolEntity(ManagedParams);
}

}

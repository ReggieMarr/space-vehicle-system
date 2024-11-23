#ifndef FlightComputerTopologyDefs_HPP
#define FlightComputerTopologyDefs_HPP

#include "Drv/BlockDriver/BlockDriver.hpp"
#include "FlightComputer/Top/FppConstantsAc.hpp"
#include "Fw/Types/MallocAllocator.hpp"
#include "Svc/FramingProtocol/FprimeProtocol.hpp"

namespace FlightComputer {

namespace Allocation {

// Malloc allocator for topology construction
extern Fw::MallocAllocator mallocator;

} // namespace Allocation

// State for topology construction
struct TopologyState {
  TopologyState()
      : // Should set defaults here
        hostName(""), uplinkPort(0), downlinkPort(0) {}
  TopologyState(const char *hostName, U32 uplinkPort, U32 downlinkPort)
      : hostName(hostName), uplinkPort(uplinkPort), downlinkPort(downlinkPort) {

  }
  const char *hostName;
  U32 uplinkPort;
  U32 downlinkPort;
};

// Health ping entries
namespace PingEntries {
namespace FlightComputer_blockDrv {
enum { WARN = 3, FATAL = 5 };
}
namespace FlightComputer_chanTlm {
enum { WARN = 3, FATAL = 5 };
}
namespace FlightComputer_gdsChanTlm {
enum { WARN = 3, FATAL = 5 };
}
namespace FlightComputer_cmdDisp {
enum { WARN = 3, FATAL = 5 };
}
namespace FlightComputer_cmdSeq {
enum { WARN = 3, FATAL = 5 };
}
namespace FlightComputer_eventLogger {
enum { WARN = 3, FATAL = 5 };
}
namespace FlightComputer_fileDownlink {
enum { WARN = 3, FATAL = 5 };
}
namespace FlightComputer_fileManager {
enum { WARN = 3, FATAL = 5 };
}
namespace FlightComputer_fileUplink {
enum { WARN = 3, FATAL = 5 };
}
namespace FlightComputer_pingRcvr {
enum { WARN = 3, FATAL = 5 };
}
namespace FlightComputer_prmDb {
enum { WARN = 3, FATAL = 5 };
}
namespace FlightComputer_rateGroup1Comp {
enum { WARN = 3, FATAL = 5 };
}
namespace FlightComputer_rateGroup2Comp {
enum { WARN = 3, FATAL = 5 };
}
namespace FlightComputer_rateGroup3Comp {
enum { WARN = 3, FATAL = 5 };
}
} // namespace PingEntries

} // namespace FlightComputer

#endif

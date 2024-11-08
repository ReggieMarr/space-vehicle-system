module FlightComputer {

  # ----------------------------------------------------------------------
  # Defaults
  # ----------------------------------------------------------------------

  module Default {

    constant queueSize = 10 
    constant stackSize = 64 * 1024

  }

  # ----------------------------------------------------------------------
  # Active component instances
  # ----------------------------------------------------------------------

  instance blockDrv: Drv.BlockDriver base id 0x0100 \
    queue size Default.queueSize \
    stack size Default.stackSize \
    priority 99

  instance rateGroup1Comp: Svc.ActiveRateGroup base id 0x0200 \
    queue size Default.queueSize \
    stack size Default.stackSize \
    priority 79

  instance rateGroup2Comp: Svc.ActiveRateGroup base id 0x0300 \
    queue size Default.queueSize \
    stack size Default.stackSize \
    priority 78

  instance rateGroup3Comp: Svc.ActiveRateGroup base id 0x0400 \
    queue size Default.queueSize \
    stack size Default.stackSize \
    priority 77

  instance cmdDisp: Svc.CommandDispatcher base id 0x0500 \
    queue size 20 \
    stack size Default.stackSize \
    priority 60

  instance cmdSeq: Svc.CmdSequencer base id 0x0600 \
    queue size Default.queueSize \
    stack size Default.stackSize \
    priority 59

  instance fileDownlink: Svc.FileDownlink base id 0x0700 \
    queue size 30 \
    stack size Default.stackSize \
    priority 59

  instance fileManager: Svc.FileManager base id 0x0800 \
    queue size 30 \
    stack size Default.stackSize \
    priority 59

  instance fileUplink: Svc.FileUplink base id 0x0900 \
    queue size 30 \
    stack size Default.stackSize \
    priority 59

  instance pingRcvr: FlightComputer.PingReceiver base id 0x0A00 \
    queue size Default.queueSize \
    stack size Default.stackSize \
    priority 99

  instance eventLogger: Svc.ActiveLogger base id 0x0B00 \
    queue size Default.queueSize \
    stack size Default.stackSize \
    priority 57

  instance gdsChanTlm: Svc.TlmChan base id 0x0C00 \
    queue size Default.queueSize \
    stack size Default.stackSize \
    priority 54

  instance prmDb: Svc.PrmDb base id 0x0D00 \
    queue size Default.queueSize \
    stack size Default.stackSize \
    priority 55

  # ----------------------------------------------------------------------
  # Queued component instances
  # ----------------------------------------------------------------------

  instance $health: Svc.Health base id 0x2000 \
    queue size 25

  # ----------------------------------------------------------------------
  # Passive component instances
  # ----------------------------------------------------------------------

  instance comm: Drv.TcpClient base id 0x4000

  instance framer: Svc.Framer base id 0x4100

  instance fatalAdapter: Svc.AssertFatalAdapter base id 0x4200

  instance fatalHandler: Svc.FatalHandler base id 0x4300

  instance commsBufferManager: Svc.BufferManager base id 0x4400

  instance posixTime: Svc.PosixTime base id 0x4500

  instance rateGroupDriverComp: Svc.RateGroupDriver base id 0x4600

  instance textLogger: Svc.PassiveTextLogger base id 0x4900

  instance systemResources: Svc.SystemResources base id 0x4A00

  instance frameAccumulator: Svc.FrameAccumulator base id 0x4C00

  instance deframer: Svc.Deframer base id 0x4D00

  instance uplinkRouter: Svc.Router base id 0x4E00

  instance flightSequencer: FlightComputer.FlightSequencer base id 0x4F00 \
    queue size Default.queueSize \
    stack size Default.stackSize \
    priority 59
}

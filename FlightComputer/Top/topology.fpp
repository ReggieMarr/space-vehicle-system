module FlightComputer {

  # ----------------------------------------------------------------------
  # Symbolic constants for port numbers
  # ----------------------------------------------------------------------

    enum Ports_RateGroups {
      rateGroup1
      rateGroup2
      rateGroup3
    }

    enum Ports_StaticMemory {
      downlink
      uplink
      accumulator
    }

  topology FlightComputer {

    # ----------------------------------------------------------------------
    # Instances used in the topology
    # ----------------------------------------------------------------------

    instance $health
    instance blockDrv
    instance gdsChanTlm
    instance cmdDisp
    instance cmdSeq
    instance comm
    instance deframer
    instance eventLogger
    instance fatalAdapter
    instance fatalHandler
    instance fileDownlink
    instance fileManager
    instance fileUplink
    instance commsBufferManager
    instance frameAccumulator
    instance framer
    instance posixTime
    instance pingRcvr
    instance prmDb
    instance rateGroup1Comp
    instance rateGroup2Comp
    instance rateGroup3Comp
    instance rateGroupDriverComp
    instance uplinkRouter
    instance textLogger
    instance systemResources
    instance flightSequencer

    # ----------------------------------------------------------------------
    # Pattern graph specifiers
    # ----------------------------------------------------------------------

    command connections instance cmdDisp

    event connections instance eventLogger

    param connections instance prmDb

    telemetry connections instance gdsChanTlm

    text event connections instance textLogger

    time connections instance posixTime

    health connections instance $health

    # ----------------------------------------------------------------------
    # Direct graph specifiers
    # ----------------------------------------------------------------------

    connections GDSDownlink {

      gdsChanTlm.PktSend -> framer.comIn
      eventLogger.PktSend -> framer.comIn
      fileDownlink.bufferSendOut -> framer.bufferIn

      framer.framedAllocate -> commsBufferManager.bufferGetCallee
      framer.framedOut -> comm.$send
      framer.bufferDeallocate -> fileDownlink.bufferReturn

      comm.deallocate -> commsBufferManager.bufferSendIn

    }

    connections FaultProtection {
      eventLogger.FatalAnnounce -> fatalHandler.FatalReceive
    }

    connections RateGroups {

      # Block driver
      blockDrv.CycleOut -> rateGroupDriverComp.CycleIn

      # Rate group 1 (100Hz)
      rateGroupDriverComp.CycleOut[Ports_RateGroups.rateGroup1] -> rateGroup1Comp.CycleIn

      # Rate group 2 (1Hz)
      rateGroupDriverComp.CycleOut[Ports_RateGroups.rateGroup2] -> rateGroup2Comp.CycleIn
      rateGroup2Comp.RateGroupMemberOut[0] -> cmdSeq.schedIn
      rateGroup2Comp.RateGroupMemberOut[1] -> gdsChanTlm.Run
      rateGroup2Comp.RateGroupMemberOut[2] -> fileDownlink.Run
      rateGroup2Comp.RateGroupMemberOut[3] -> systemResources.run
      rateGroup2Comp.RateGroupMemberOut[4] -> blockDrv.Sched
      rateGroup2Comp.RateGroupMemberOut[5] -> commsBufferManager.schedIn
      rateGroup2Comp.RateGroupMemberOut[6] -> $health.Run

      # Rate group 3 (10Hz)
      rateGroupDriverComp.CycleOut[Ports_RateGroups.rateGroup3] -> rateGroup3Comp.CycleIn
      rateGroup1Comp.RateGroupMemberOut[0] -> flightSequencer.run
    }

    # NOTE this is not really used atm and is here more to match closer to the Ref
    connections Sequencer {
      cmdSeq.comCmdOut -> cmdDisp.seqCmdBuff
      cmdDisp.seqCmdStatus -> cmdSeq.cmdResponseIn
    }

    connections Uplink {

      comm.allocate -> commsBufferManager.bufferGetCallee
      comm.$recv -> frameAccumulator.dataIn

      frameAccumulator.frameOut -> deframer.framedIn
      frameAccumulator.frameAllocate -> commsBufferManager.bufferGetCallee
      frameAccumulator.dataDeallocate -> commsBufferManager.bufferSendIn
      deframer.deframedOut -> uplinkRouter.dataIn

      uplinkRouter.commandOut -> cmdDisp.seqCmdBuff
      uplinkRouter.fileOut -> fileUplink.bufferSendIn
      uplinkRouter.bufferDeallocate -> commsBufferManager.bufferSendIn

      cmdDisp.seqCmdStatus -> uplinkRouter.cmdResponseIn

      fileUplink.bufferSendOut -> commsBufferManager.bufferSendIn
    }

  }
}

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
    instance fprimeTcpLink
    instance deframer
    instance eventLogger
    instance fatalAdapter
    instance fatalHandler
    instance fileDownlink
    instance fileManager
    instance fileUplink
    instance commsBufferManager
    instance fprimeFrameAccumulator
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

    instance sppDataLinkDeframer
    instance ccsdsLink
    instance ccsdsFrameAccumulator
    instance ccsdsUplinkRouter
    instance ccsdsNode
    instance ccsdsFramer

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
      framer.framedOut -> fprimeTcpLink.$send
      framer.bufferDeallocate -> fileDownlink.bufferReturn

      fprimeTcpLink.deallocate -> commsBufferManager.bufferSendIn

    }

    connections FaultProtection {
      eventLogger.FatalAnnounce -> fatalHandler.FatalReceive
    }

    connections RateGroups {

      # Block driver
      blockDrv.CycleOut -> rateGroupDriverComp.CycleIn

      # Rate group 1 (1Hz)
      rateGroupDriverComp.CycleOut[Ports_RateGroups.rateGroup1] -> rateGroup1Comp.CycleIn
      rateGroup1Comp.RateGroupMemberOut[0] -> gdsChanTlm.Run
      rateGroup1Comp.RateGroupMemberOut[1] -> blockDrv.Sched
      rateGroup1Comp.RateGroupMemberOut[2] -> commsBufferManager.schedIn
      rateGroup1Comp.RateGroupMemberOut[3] -> flightSequencer.run

      # Rate group 2 (1/2Hz)
      rateGroupDriverComp.CycleOut[Ports_RateGroups.rateGroup2] -> rateGroup2Comp.CycleIn
      rateGroup2Comp.RateGroupMemberOut[0] -> cmdSeq.schedIn
      rateGroup2Comp.RateGroupMemberOut[1] -> $health.Run

      # Rate group 3 (1/4Hz)
      rateGroupDriverComp.CycleOut[Ports_RateGroups.rateGroup3] -> rateGroup3Comp.CycleIn
      rateGroup3Comp.RateGroupMemberOut[0] -> systemResources.run
      rateGroup3Comp.RateGroupMemberOut[1] -> fileDownlink.Run
      rateGroup3Comp.RateGroupMemberOut[2] -> ccsdsNode.run
    }

    # NOTE this is not really used atm and is here more to match closer to the Ref
    connections Sequencer {
      cmdSeq.comCmdOut -> cmdDisp.seqCmdBuff
      cmdDisp.seqCmdStatus -> cmdSeq.cmdResponseIn
    }

    connections fprimeUplink {

      fprimeTcpLink.allocate -> commsBufferManager.bufferGetCallee
      fprimeTcpLink.$recv -> fprimeFrameAccumulator.dataIn

      fprimeFrameAccumulator.frameOut -> deframer.framedIn
      fprimeFrameAccumulator.frameAllocate -> commsBufferManager.bufferGetCallee
      fprimeFrameAccumulator.dataDeallocate -> commsBufferManager.bufferSendIn
      deframer.deframedOut -> uplinkRouter.dataIn

      uplinkRouter.commandOut -> cmdDisp.seqCmdBuff
      uplinkRouter.fileOut -> fileUplink.bufferSendIn
      uplinkRouter.bufferDeallocate -> commsBufferManager.bufferSendIn

      cmdDisp.seqCmdStatus -> uplinkRouter.cmdResponseIn

      fileUplink.bufferSendOut -> commsBufferManager.bufferSendIn
    }

    connections ccsds {

      ccsdsNode.bufferSendOut -> ccsdsFramer.bufferIn
      ccsdsNode.PktSend -> ccsdsFramer.comIn

      ccsdsFramer.framedAllocate -> commsBufferManager.bufferGetCallee
      ccsdsFramer.framedOut -> ccsdsLink.comDataIn
      ccsdsLink.comStatus -> ccsdsNode.comStatusIn
      # ccsdsNode.allocate -> commsBufferManager.bufferGetCallee
      ccsdsNode.drvReady -> ccsdsLink.drvConnected
      ccsdsLink.drvDataOut -> ccsdsNode.drvSend
      ccsdsNode.drvRcv -> ccsdsLink.drvDataIn

      ccsdsLink.comDataOut -> ccsdsFrameAccumulator.dataIn

      ccsdsFrameAccumulator.frameOut -> sppDataLinkDeframer.framedIn
      ccsdsFrameAccumulator.frameAllocate -> commsBufferManager.bufferGetCallee
      ccsdsFrameAccumulator.dataDeallocate -> commsBufferManager.bufferSendIn
      sppDataLinkDeframer.deframedOut -> ccsdsUplinkRouter.dataIn

      ccsdsUplinkRouter.commandOut -> ccsdsNode.seqCmdBuff
      ccsdsUplinkRouter.fileOut -> ccsdsNode.bufferSendIn
      ccsdsUplinkRouter.bufferDeallocate -> commsBufferManager.bufferSendIn
      ccsdsNode.bufferDeallocate -> commsBufferManager.bufferSendIn

      ccsdsNode.seqCmdStatus -> ccsdsUplinkRouter.cmdResponseIn
    }

  }
}

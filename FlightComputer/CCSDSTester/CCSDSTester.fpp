module FlightComputer {
    @ Loopback CCSDS Testing component
    active component CCSDSTester {
        ###############################################################################
        # Standard AC Ports: Required for Channels, Events, Commands, and Parameters  #
        ###############################################################################
        @ Port for requesting the current time
        time get port timeCaller

        @ Port for sending command registrations
        command reg port cmdRegOut

        @ Port for receiving commands
        command recv port cmdIn

        @ Port for sending command responses
        command resp port cmdResponseOut

        @ Port for sending textual representation of events
        text event port logTextOut

        @ Port for sending events to downlink
        event port logOut

        @ Port for sending telemetry channels to downlink
        telemetry port tlmOut

        @ Port to return the value of a parameter
        param get port prmGetOut

        @Port to set the value of a parameter
        param set port prmSetOut


        ##############################################################################
        #### Test interface                                                       ####
        ##############################################################################

        @ Output Command Status Port
        output port seqCmdStatus: [CmdDispatcherSequencePorts] Fw.CmdResponse

        @ Command buffer input port for sequencers or other sources of command buffers
        async input port seqCmdBuff: [CmdDispatcherSequencePorts] Fw.Com

        # ----------------------------------------------------------------------
        # Port matching specifiers
        # ----------------------------------------------------------------------
        match seqCmdStatus with seqCmdBuff

        # ----------------------------------------------------------------------
        # General Ports
        # ----------------------------------------------------------------------

        @ Buffer send in
        async input port bufferSendIn: Fw.BufferSend

        @ Buffer send out
        output port bufferSendOut: Fw.BufferSend

        @ Packet send port
        output port PktSend: Fw.Com

        @ Port for receiving the status signal
        async input port comStatusIn: Fw.SuccessCondition


        # DrvMockPorts
        @ Port invoked when the driver is ready to send/receive data
        output port drvReady: Drv.ByteStreamReady

        @ Port invoked when driver has received data
        output port drvRecv: Drv.ByteStreamRecv

        @ Port invoked to send data out the driver
        guarded input port drvSend: Drv.ByteStreamSend

        # @ Buffer return input port
        # async input port bufferReturn: Fw.BufferSend

        # @ Receives raw data from a ByteStreamDriver, ComStub, or other buffer producing component
        # guarded input port dataIn: Drv.ByteStreamRecv

        # Loopback output
        # @ Port invoked when driver has received data
        # output port $recv: Drv.ByteStreamRecv

        # @ Port invoked to send data out the driver
        # guarded input port $send: Drv.ByteStreamSend

        # output port allocate: Fw.BufferGet
        # output port deallocate: Fw.BufferSend

        @ Simple command received interface
        async command PING

        @ Simple command received interface
        async command MESSAGE(str1: string size 50)
        # async command LONG_MESSAGE(str1: string)
    }
}

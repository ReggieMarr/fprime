module Svc {
    @ Accumulates packets into a message payload buffer
    active component PacketAccumulator {

        # One async command/port is required for active components
        # This should be overridden by the developers with a useful command/port
        @ TODO
        async input port TODO: Svc.Sched

        ##############################################################################
        #### Uncomment the following examples to start customizing your component ####
        ##############################################################################

        # @ Example async command
        # async command COMMAND_NAME(param_name: U32)

        # @ Example telemetry counter
        # telemetry ExampleCounter: U64

        # @ Example event
        # event ExampleStateEvent(example_state: Fw.On) severity activity high id 0 format "State set to {}"

        # @ Example port: receiving calls from the rate group
        # sync input port run: Svc.Sched

        # @ Example parameter
        # param PARAMETER_NAME: U32

        ###############################################################################
        # Standard AC Ports: Required for Channels, Events, Commands, and Parameters  #
        ###############################################################################
        @ Port for requesting the current time
        time get port timeCaller

        @ Port to return the value of a parameter
        param get port prmGetOut

        @Port to set the value of a parameter
        param set port prmSetOut

        # NOTE this comes from FramerInterface.fppi
        # ----------------------------------------------------------------------
        # Receiving packets
        # ----------------------------------------------------------------------
        @ Port for receiving data packets of any type stored in statically-sized
        @ Fw::Com buffers
        guarded input port comIn: Fw.Com

        @ Port for receiving file packets stored in dynamically-sized
        @ Fw::Buffer objects
        guarded input port bufferIn: Fw.BufferSend
    }
}

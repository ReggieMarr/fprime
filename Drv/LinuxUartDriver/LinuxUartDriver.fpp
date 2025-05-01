module Drv {

  passive component LinuxUartDriver {
    # # /**
    # #  * @brief Baud rate options
    # #  */
    # enum BaudRate {
    #   BAUD_9600,
    #   BAUD_19200,
    #   BAUD_38400,
    #   BAUD_57600,
    #   BAUD_115K,
    #   BAUD_230K,
    #   BAUD_460K,
    #   BAUD_921K,
    #   BAUD_1000K,
    #   BAUD_1152K,
    #   BAUD_1500K,
    #   BAUD_2000K,
    #   BAUD_2500K,
    #   BAUD_3000K,
    #   BAUD_3500K,
    #   BAUD_4000K
    # }

    # # /**
    # #  * @brief Data bits options
    # #  */
    # enum DataBits {
    #     DATA_5 = 5,
    #     DATA_6 = 6,
    #     DATA_7 = 7,
    #     DATA_8 = 8
    # }

    # # /**
    # #  * @brief Stop bits options
    # #  */
    # enum StopBits {
    #     STOP_1 = 1,
    #     STOP_2 = 2
    # }

    # # /**
    # #  * @brief Flow control options
    # #  */
    # enum FlowControl {
    #     FLOW_NONE,
    #     FLOW_HARDWARE,
    #     FLOW_SOFTWARE
    # }

    # # /**
    # #  * @brief Parity options
    # #  */
    # enum Parity {
    #     PARITY_NONE,
    #     PARITY_ODD,
    #     PARITY_EVEN
    # }
    # struct UartConfig {
    #     baudRate :U32
    #     dataBits :DataBits
    #     stopBits :StopBits
    #     parity :Parity
    #     enableSoftwareFlowControl: bool
    #     enableHardwareFlowControl: bool
    #     localMode: bool
    #     receiverEnable: bool
    #     enableInputProcessingMode: bool
    #     enableEchoMode: bool
    #     minChars: U8
    #     timeoutMs: I8
    # }

    # ----------------------------------------------------------------------
    # General ports
    # ----------------------------------------------------------------------

    import ByteStreamDriver

    @ Port invoked to send data out the driver
    guarded input port readPoll: Drv.ByteStreamPoll

    @ Port invoked when the driver is no longer ready to send/receive data
    output port notReady: Drv.ByteStreamReady

    @ Allocation port used for allocating memory in the receive task
    output port allocate: Fw.BufferGet

    @ Deallocation of allocated buffers
    output port deallocate: Fw.BufferSend

    @ The rate group input for sending telemetry
    sync input port run: Svc.Sched

    # ----------------------------------------------------------------------
    # Special ports
    # ----------------------------------------------------------------------

    event port Log

    telemetry port Tlm

    text event port LogText

    time get port Time

    # ----------------------------------------------------------------------
    # Events
    # ----------------------------------------------------------------------

    include "Events.fppi"

    # ----------------------------------------------------------------------
    # Telemetry
    # ----------------------------------------------------------------------

    include "Telemetry.fppi"

  }

}

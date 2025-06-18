#ifndef UARTCONFIG_H_
#define UARTCONFIG_H_
#include "Fw/FPrimeBasicTypes.hpp"
#include <Drv/LinuxUartDriver/LinuxUartDriverComponentAc.hpp>
#include <Os/Task.hpp>
#include <asm-generic/termbits.h>

/**
 * @struct UartConfig
 * @brief Structure to store UART configuration parameters
 */
struct UartConfig {
    /**
     * @brief Baud rate options
     */

    enum BaudRate {
      BAUD_9600,
      BAUD_19200,
      BAUD_38400,
      BAUD_57600,
      BAUD_115K,
      BAUD_230K,
#ifdef TGT_OS_TYPE_LINUX
      BAUD_460K,
      BAUD_921K,
      BAUD_1000K,
      BAUD_1152K,
      BAUD_1500K,
      BAUD_2000K,
#ifdef B2500000
      BAUD_2500K,
#endif
#ifdef B3000000
      BAUD_3000K,
#endif
#ifdef B3500000
      BAUD_3500K,
#endif
#ifdef B4000000
      BAUD_4000K
#endif
#endif
    };

    /**
     * @brief Data bits options
     */
    enum DataBits {
        DATA_5 = 5,
        DATA_6 = 6,
        DATA_7 = 7,
        DATA_8 = 8
    };

    /**
     * @brief Stop bits options
     */
    enum StopBits {
        STOP_1 = 1,
        STOP_2 = 2
    };

    /**
     * @brief Parity options
     */
    enum Parity {
        PARITY_NONE,
        PARITY_ODD,
        PARITY_EVEN
    };

    /**
     * @brief Flow control options
     */
    enum FlowControl {
        FLOW_NONE,
        FLOW_HARDWARE,
        FLOW_SOFTWARE
    };

    // Configuration members
    U32         baudRate;                  // Baud rate
    DataBits    dataBits;                  // Number of data bits
    StopBits    stopBits;                  // Number of stop bits
    Parity      parity;                    // Parity type
    bool        enableSoftwareFlowControl; // Whether to use Hardware flow control
    bool        enableHardwareFlowControl; // Whether to use Software based flow control
    bool        localMode;                 // Ignore modem control lines
    bool        receiverEnable;            // Enable receiver
    bool        enableInputProcessingMode; // Input processing mode
    bool        enableEchoMode;            // Echo input characters
    U8          minChars;                  // Min chars for read
    I16         timeoutMs;                 // Timeout in milliseconds
};

#endif // UARTCONFIG_H_

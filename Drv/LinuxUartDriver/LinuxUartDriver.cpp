// ======================================================================
// \title  LinuxUartDriverImpl.cpp
// \author tcanham
// \brief  cpp file for LinuxUartDriver component implementation class
//
// \copyright
// Copyright 2009-2015, by the California Institute of Technology.
// ALL RIGHTS RESERVED.  United States Government Sponsorship
// acknowledged.
//
// ======================================================================

#include <Drv/LinuxUartDriver/LinuxUartDriver.hpp>
#include <Os/TaskString.hpp>

#include "Drv/ByteStreamDriverModel/PollStatusEnumAc.hpp"
#include "Drv/ByteStreamDriverModel/RecvStatusEnumAc.hpp"
#include "Drv/LinuxUartDriver/UartConfig.hpp"
#include "FpConfig.h"
#include "Fw/Logger/Logger.hpp"
#include "Fw/Time/TimeInterval.hpp"
#include "Fw/Types/BasicTypes.hpp"
#include "Fw/Types/String.hpp"

// Linux headers
// #include <fcntl.h> // Contains file controls like O_RDWR
// #include <errno.h> // Error integer and strerror() function
// // Contains POSIX terminal control definitions
// // #include <termios.h> This must be removed, otherwise we'll get "redefinition of ‘struct termios’" errors
// #include <sys/ioctl.h> // Used for TCGETS2/TCSETS2, which is required for custom baud rates
// #include <unistd.h> // write(), read(), close()

// old
#include <fcntl.h>
// #include <termios.h>
#include <cerrno>

// #include <termios.h>

#include <asm-generic/termbits.h>
#include <sys/ioctl.h>
#include <unistd.h>

namespace Drv {

// ----------------------------------------------------------------------
// Construction, initialization, and destruction
// ----------------------------------------------------------------------

LinuxUartDriver ::LinuxUartDriver(const char* const compName)
    : LinuxUartDriverComponentBase(compName),
      m_fd(-1),
      m_allocationSize(0),
      m_device("NOT_EXIST"),
      m_bytesSent(0),
      m_bytesReceived(0),
      m_quitReadThread(false) {}

bool LinuxUartDriver::resetHardware() {
    if (this->m_fd < 0) {
        return false;
    }

    // Get current state
    int status;
    if (ioctl(this->m_fd, TIOCMGET, &status) < 0) {
        return false;
    }

    // Toggle DTR (Data Terminal Ready)
    status &= ~TIOCM_DTR;  // Clear DTR
    if (ioctl(this->m_fd, TIOCMSET, &status) < 0) {
        return false;
    }

    // Wait briefly
    usleep(200000);  // 200ms

    // Set DTR back
    status |= TIOCM_DTR;
    if (ioctl(this->m_fd, TIOCMSET, &status) < 0) {
        return false;
    }

    return true;
}

bool LinuxUartDriver::open(const char* const device) {
    this->m_fd = ::open(device, O_RDWR | O_NOCTTY | O_SYNC);

    if (this->m_fd == -1) {
        Fw::LogStringArg _arg = device;
        Fw::LogStringArg _err = strerror(errno);
        this->log_WARNING_HI_OpenError(_arg, this->m_fd, _err);
        return false;
    }
    this->m_device = device;
    // Use termios2 for custom baud rate support
    struct termios2 txTty;
    int ret = ioctl(this->m_fd, TCGETS2, &txTty);
    if (ret < 0) {
        // Handle error
        close(this->m_fd);
        return false;
    }

    // Set custom baud rate using BOTHER
    txTty.c_cflag &= ~CBAUD;
    txTty.c_cflag |= BOTHER;
    txTty.c_ispeed = 3000000;
    txTty.c_ospeed = 3000000;

    // Set data bits, parity, stop bits: 8N1
    txTty.c_cflag &= ~PARENB;  // No parity
    txTty.c_cflag &= ~CSTOPB;  // 1 stop bit
    txTty.c_cflag &= ~CSIZE;
    txTty.c_cflag |= CS8;  // 8 data bits

    // Disable hardware flow control if needed (or enable if fc == HW_FLOW)
    txTty.c_cflag &= ~CRTSCTS;  // Disable hardware flow control by default
    // Configure control flags for local connection and enabling receiver
    txTty.c_cflag |= CLOCAL | CREAD;  // turn on READ & ignore ctrl lines

    // turn off s/w flow ctrl
    txTty.c_iflag &= ~(IXON | IXOFF | IXANY);

    // Set raw mode (non-canonical, no echo, etc.)
    txTty.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    txTty.c_oflag &= ~OPOST;

    // Configure timeout: don't wait for a character, 2.5s timeout
    txTty.c_cc[VMIN] = 0;
    txTty.c_cc[VTIME] =
        1;  // Timeout at 1 decisecond or 100ms // static_cast<cc_t>(2500 / 100.0 + 0.5); // Convert ms to deciseconds

    // Flush and apply the settings
    // tcflush(this->m_fd, TCIFLUSH);
    ret = ioctl(this->m_fd, TCSETS2, &txTty);
    if (ret < 0) {
        // Handle error
        Fw::LogStringArg _arg = device;
        Fw::LogStringArg _err = strerror(errno);
        this->log_WARNING_HI_OpenError(_arg, this->m_fd, _err);
        close(this->m_fd);
        return false;
    }

    // Now we're ready
    Fw::LogStringArg _arg = this->m_device;
    this->log_ACTIVITY_HI_PortOpened(_arg);
    if (this->isConnected_ready_OutputPort(0)) {
        this->ready_out(0);  // Indicate the driver is connected
    }

    // All done!
    Fw::LogStringArg _arg = device;
    this->log_ACTIVITY_HI_PortOpened(_arg);
    if (this->isConnected_ready_OutputPort(0)) {
        this->ready_out(0);  // Indicate the driver is connected
    }
    return true;
}

static bool setTtyConfig(termios2* tty, UartConfig& cfg) {
    // Reset these flags
    // TODO determine what the behaviour is here
    tty->c_iflag = (ICRNL | ONLCR);
    tty->c_oflag = 0;
    tty->c_lflag = 0;

    // See https://man7.org/linux/man-pages/man3/tcflush.3.html
    tty->c_cflag &= ~CSIZE;  // CSIZE is a mask for the number of bits per character
    switch (cfg.dataBits) {
        case UartConfig::DataBits::DATA_5:
            tty->c_cflag |= CS5;
            break;
        case UartConfig::DataBits::DATA_6:
            tty->c_cflag |= CS6;
            break;
        case UartConfig::DataBits::DATA_7:
            tty->c_cflag |= CS7;
            break;
        case UartConfig::DataBits::DATA_8:
            tty->c_cflag |= CS8;
            break;
        default:
            return false;
    }

    // 1 Stop bit is standard
    // Set num. stop bits
    switch (cfg.stopBits) {
        case UartConfig::StopBits::STOP_1:
            tty->c_cflag &= ~CSTOPB;
            break;
        case UartConfig::StopBits::STOP_2:
            tty->c_cflag |= CSTOPB;
            break;
        default:
            return false;
    }

    if (cfg.enableHardwareFlowControl) {
        tty->c_cflag |= CRTSCTS;
    } else {
        tty->c_cflag &= ~CRTSCTS;
    }

    if (cfg.enableSoftwareFlowControl) {
        tty->c_iflag |= (IXON | IXOFF | IXANY);
    } else {
        tty->c_iflag &= ~(IXON | IXOFF | IXANY);
    }

    // Ignore modem
    if (cfg.localMode) {
        tty->c_cflag |= CLOCAL;
    } else {
        tty->c_cflag &= ~CLOCAL;
    }

    // Enable receiver
    if (cfg.receiverEnable) {
        tty->c_cflag |= CREAD;
    } else {
        tty->c_cflag &= ~CREAD;
    }

    // Set parity
    // See https://man7.org/linux/man-pages/man3/tcflush.3.html
    switch (cfg.parity) {
        case UartConfig::Parity::PARITY_NONE:
            tty->c_cflag &= ~PARENB;
            break;
        case UartConfig::Parity::PARITY_EVEN:
            tty->c_cflag |= PARENB;
            tty->c_cflag &= ~PARODD;  // Clearing PARODD makes the parity even
            break;
        case UartConfig::Parity::PARITY_ODD:
            tty->c_cflag |= (PARENB | PARODD);
            break;
        default:
            return false;
    }

    // Use the custom baud rate mechanism
    tty->c_cflag &= ~CBAUD;
    // According to this CBAUDEX is often more reliable than BOTHER
    // https://blog.mbedded.ninja/programming/operating-systems/linux/linux-serial-ports-using-c-cpp/
    // tty->c_cflag |= CBAUDEX;
    tty->c_cflag |= BOTHER;
    // Set baud rate values
    switch (cfg.baudRate) {
        case UartConfig::BaudRate::BAUD_9600:
            tty->c_ispeed = tty->c_ospeed = 9600;
            break;
        case UartConfig::BaudRate::BAUD_19200:
            tty->c_ispeed = tty->c_ospeed = 19200;
            break;
        case UartConfig::BaudRate::BAUD_38400:
            tty->c_ispeed = tty->c_ospeed = 38400;
            break;
        case UartConfig::BaudRate::BAUD_57600:
            tty->c_ispeed = tty->c_ospeed = 57600;
            break;
        case UartConfig::BaudRate::BAUD_115K:
            tty->c_ispeed = tty->c_ospeed = 115200;
            break;
        case UartConfig::BaudRate::BAUD_230K:
            tty->c_ispeed = tty->c_ospeed = 230400;
            break;
#ifdef TGT_OS_TYPE_LINUX
        case UartConfig::BaudRate::BAUD_460K:
            tty->c_ispeed = tty->c_ospeed = 460800;
            break;
        case UartConfig::BaudRate::BAUD_921K:
            tty->c_ispeed = tty->c_ospeed = 921600;
            break;
        case UartConfig::BaudRate::BAUD_1000K:
            tty->c_ispeed = tty->c_ospeed = 1000000;
            break;
        case UartConfig::BaudRate::BAUD_1152K:
            tty->c_ispeed = tty->c_ospeed = 1152000;
            break;
        case UartConfig::BaudRate::BAUD_1500K:
            tty->c_ispeed = tty->c_ospeed = 1500000;
            break;
        case UartConfig::BaudRate::BAUD_2000K:
            tty->c_ispeed = tty->c_ospeed = 2000000;
            break;
#ifdef B2500000
        case UartConfig::BaudRate::BAUD_2500K:
            tty->c_ispeed = tty->c_ospeed = 2500000;
            break;
#endif
#ifdef B3000000
        case UartConfig::BaudRate::BAUD_3000K:
            tty->c_ispeed = tty->c_ospeed = 3000000;
            break;
#endif
#ifdef B3500000
        case UartConfig::BaudRate::BAUD_3500K:
            tty->c_ispeed = tty->c_ospeed = 3500000;
            break;
#endif
#ifdef B4000000
        case UartConfig::BaudRate::BAUD_4000K:
            tty->c_ispeed = tty->c_ospeed = 4000000;
            break;
#endif
#endif
        default:
            return false;
    }

    // Canonical input is when read waits for EOL or EOF characters before returning. In non-canonical mode, the rate at
    // which read() returns is instead controlled by c_cc[VMIN] and c_cc[VTIME] Configure input processing modes
    if (cfg.enableInputProcessingMode) {
        tty->c_lflag |= ICANON;  // Enable canonical mode
        tty->c_lflag |= ISIG;    // Enable signals
        tty->c_lflag |= IEXTEN;  // Enable extended functions
    } else {
        tty->c_lflag &= ~ICANON;  // Disable canonical mode
        tty->c_lflag &= ~ISIG;    // Disable signals
        tty->c_lflag &= ~IEXTEN;  // Disable extended functions
    }

    // Turn off echo erase (echo erase only relevant if canonical input is active)
    if (cfg.enableEchoMode) {
        tty->c_lflag |= (ECHO | ECHOE | ECHOK);
    } else {
        tty->c_lflag &= ~(ECHO | ECHOE | ECHOK | ECHONL);
    }

    /* In non canonical mode (Ctrl-C and other disabled, no echo,...) VMIN and VTIME work this way:
    if the function read() has'nt read at least VMIN chars it waits until has read at least VMIN
    chars (even if VTIME timeout expires); once it has read at least vmin chars, if subsequent
    chars do not arrive before VTIME expires, it returns error; if a char arrives, it resets the
    timeout, so the internal timer will again start from zero (for the nex char,if any)*/
    // tty->c_cc[VMIN]=1;// Minimum number of characters to read before returning error
    // tty->c_cc[VTIME]=1;// Set timeouts in tenths of second
    if (cfg.timeoutMs == -1) {
        // Always wait for at least one byte, this could
        // block indefinitely
        tty->c_cc[VTIME] = 0;
    } else if (cfg.timeoutMs == 0) {
        // Setting both to 0 will give a non-blocking read
        tty->c_cc[VTIME] = 0;
    } else if (cfg.timeoutMs > 0) {
        // round up to nearest 100ms
        // NOTE there was some comments that the max should be 25500
        tty->c_cc[VTIME] = (cc_t)((cfg.timeoutMs + 99) / 100);
    }

    // TODO theres some mutual exclusion to the above options that we should account for
    tty->c_cc[VMIN] = cfg.minChars;

    // Configure echo depending on echo_ boolean
    tty->c_lflag &= ~ECHOE;   // Turn off echo erase (echo erase only relevant if canonical input is active)
    tty->c_lflag &= ~ECHONL;  //
    tty->c_lflag &= ~ISIG;    // Disables recognition of INTR (interrupt), QUIT and SUSP (suspend) characters

    return true;
}

bool LinuxUartDriver::open(const char* const device, UartConfig& cfg) {
    Fw::LogStringArg deviceArg = device;

    // Check if device exists first
    if (access(device, F_OK) == -1) {
        Fw::LogStringArg errorStr = strerror(errno);
        this->log_WARNING_HI_OpenError(deviceArg, -1, errorStr);
        return false;
    }

    I32 ret = ::open(device, O_RDWR | O_NOCTTY | O_SYNC);
    if (ret == -1) {
        Fw::LogStringArg errorStr = strerror(errno);
        this->log_WARNING_HI_OpenError(deviceArg, ret, errorStr);
        return false;
    }
    this->m_fd = ret;

    // Using termios2 for configuration to support both standard and non-standard baud rates
    struct termios2 tty;

    // Get the initial config
    ret = ioctl(this->m_fd, TCGETS2, &tty);
    if (ret < 0) {
        // Handle error
        ::close(this->m_fd);
        return false;
    }

    if (!setTtyConfig(&tty, cfg)) {
        Fw::LogStringArg errorStr = strerror(errno);
        this->log_WARNING_HI_OpenError(deviceArg, this->m_fd, errorStr);
        ::close(this->m_fd);
        return false;
    }

    // Flush and apply the settings
    // tcflush(this->m_fd, TCIFLUSH);
    ioctl(this->m_fd, TCIFLUSH);
    ret = ioctl(this->m_fd, TCSETS2, &tty);
    if (ret < 0) {
        // Handle error
        Fw::LogStringArg errorStr = strerror(errno);
        this->log_WARNING_HI_OpenError(deviceArg, this->m_fd, errorStr);
        ::close(this->m_fd);
        return false;
    }

    // Now we're ready
    this->m_device = device;
    Fw::LogStringArg _arg = this->m_device;
    this->log_ACTIVITY_HI_PortOpened(_arg);
    if (this->isConnected_ready_OutputPort(0)) {
        this->ready_out(0);  // Indicate the driver is connected
    }

    return true;
}

bool LinuxUartDriver ::stop() {
    if (this->m_fd != -1) {
        (void)close(this->m_fd);
    }

    bool ret = true;
    if (this->m_quitReadThread) {
        this->m_quitReadThread = true;
        Os::Task::Status status = this->join();
        ret = status == Os::Task::Status::OP_OK;
    }

    // Closed device
    Fw::LogStringArg _arg = this->m_device;
    this->log_ACTIVITY_HI_PortClosed(_arg);

    return ret;
}

LinuxUartDriver ::~LinuxUartDriver() {
    if (this->m_fd != -1) {
        (void)close(this->m_fd);
    }
}

// ----------------------------------------------------------------------
// Handler implementations for user-defined typed input ports
// ----------------------------------------------------------------------

void LinuxUartDriver ::run_handler(FwIndexType portNum, U32 context) {
    this->tlmWrite_BytesSent(this->m_bytesSent);
    this->tlmWrite_BytesRecv(this->m_bytesReceived);
}

Drv::ByteStreamStatus LinuxUartDriver ::send_handler(const FwIndexType portNum, Fw::Buffer& serBuffer) {
    Drv::ByteStreamStatus status = Drv::ByteStreamStatus::OP_OK;
    if (this->m_fd == -1 || serBuffer.getData() == nullptr || serBuffer.getSize() == 0) {
        status = Drv::ByteStreamStatus::OTHER_ERROR;
    } else {
        unsigned char* data = serBuffer.getData();
        NATIVE_INT_TYPE xferSize = static_cast<NATIVE_INT_TYPE>(serBuffer.getSize());
        Fw::String byteStr;
        Fw::String buffStr;
        buffStr += "->(0x";
        for (FwIndexType i = 0; i < serBuffer.getSize(); ++i) {
            byteStr.format("%x", *static_cast<U8*>(serBuffer.getData() + i));
            buffStr += byteStr;
        }
        buffStr += ")";
        byteStr.format("%u/%u", serBuffer.getSerializeRepr().getBuffLength(), serBuffer.getSize());
        buffStr += byteStr;
        buffStr += "\n";

#ifdef DEBUG
        Fw::Logger::log("Sending %p %d %s", data, serBuffer.getSize(), buffStr.toChar());
#endif

        ssize_t stat = ::write(this->m_fd, data, xferSize);

        if (-1 == stat || static_cast<size_t>(stat) != xferSize) {
            Fw::LogStringArg _arg = this->m_device;
            this->log_WARNING_HI_WriteError(_arg, static_cast<I32>(stat));
            status = Drv::ByteStreamStatus::OTHER_ERROR;
        } else {
            this->m_bytesSent += static_cast<FwSizeType>(stat);
        }
    }
    return status;
}

void LinuxUartDriver::recvReturnIn_handler(FwIndexType portNum, Fw::Buffer& fwBuffer) {
    this->deallocate_out(0, fwBuffer);
}

void LinuxUartDriver ::serialReadToRecvOutTaskEntry(void* ptr) {
    FW_ASSERT(ptr != nullptr);
    Drv::ByteStreamStatus status = ByteStreamStatus::OTHER_ERROR;  // added by m.chase 03.06.2017
    LinuxUartDriver* comp = reinterpret_cast<LinuxUartDriver*>(ptr);
    while (!comp->m_quitReadThread) {
        Fw::Buffer buff = comp->allocate_out(0, comp->m_allocationSize);

        // On failed allocation, error
        if (buff.getData() == nullptr) {
            Fw::LogStringArg _arg = comp->m_device;
            comp->log_WARNING_HI_NoBuffers(_arg);
            status = ByteStreamStatus::OTHER_ERROR;
            comp->recv_out(0, buff, status);
            // to avoid spinning, wait 50 ms
            Os::Task::delay(Fw::TimeInterval(0, 50000));
            continue;
        }

        int stat = 0;

        // Read until something is received or an error occurs. Only loop when
        // stat == 0 as this is the timeout condition and the read should spin
        FW_ASSERT_NO_OVERFLOW(buff.getSize(), size_t);
        while ((stat == 0) && !comp->m_quitReadThread) {
            stat = static_cast<int>(::read(comp->m_fd, buff.getData(), static_cast<size_t>(buff.getSize())));
        }
        buff.setSize(0);

        // On error stat (-1) must mark the read as error
        // On normal stat (>0) pass a recv ok
        // On timeout stat (0) and m_quitReadThread, error to return the buffer
        if (stat == -1) {
            Fw::LogStringArg _arg = comp->m_device;
            comp->log_WARNING_HI_ReadError(_arg, stat);
            status = ByteStreamStatus::OTHER_ERROR;
        } else if (stat > 0) {
            buff.setSize(static_cast<U32>(stat));
            status = ByteStreamStatus::OP_OK;  // added by m.chase 03.06.2017
            comp->m_bytesReceived += static_cast<FwSizeType>(stat);
        } else {
            status = ByteStreamStatus::OTHER_ERROR;  // Simply to return the buffer
        }
        status = comp->readIntoBuff(comp, buff) ? RecvStatus::RECV_OK : RecvStatus::RECV_ERROR;

        comp->recv_out(0, buff, status);  // added by m.chase 03.06.2017
    }
}

bool LinuxUartDriver ::readIntoBuff(LinuxUartDriver* comp, Fw::Buffer& buff) {
    int stat = 0;

    size_t bytesRead = 0;
    size_t numBytes = buff.getSize();
    uint8_t* ptr = buff.getData();
#ifdef DEBUG
    Fw::Logger::log("Reading %p %d for %s - %s %d\n", ptr, numBytes, comp->m_objName.toChar(), comp->m_device,
                    comp->m_fd);
#endif

    // Read until something is received or an error occurs. Only loop when
    while (bytesRead < numBytes) {
        stat = ::read(comp->m_fd, ptr + bytesRead, numBytes - bytesRead);

#ifdef DEBUG
        Fw::Logger::log("read with %d %d\n", stat, bytesRead);
#endif
        if (stat > 0) {
            bytesRead += stat;
        } else if (stat == 0) {
            // End of file reached
            break;
        } else if (stat == -1) {
            buff.setSize(0);
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // No data available at the moment, try again
                continue;
            } else {
                // Error occurred
                Fw::LogStringArg _arg = comp->m_device;
                comp->log_WARNING_HI_ReadError(_arg, stat);
                break;
            }
        }
    }

#ifdef DEBUG
    Fw::Logger::log("Completed read with %d %d\n", stat, bytesRead);
#endif

    // On error stat (-1) must mark the read as error
    // On normal stat (>0) pass a recv ok
    // On timeout stat (0) and m_quitReadThread, error to return the buffer
    if (stat == -1) {
        Fw::LogStringArg _arg = comp->m_device;
        comp->log_WARNING_HI_ReadError(_arg, stat);
        return false;
    } else if (!stat) {
        return false;
    }

    buff.setSize(static_cast<U32>(bytesRead));
    return true;
}

Drv::PollStatus LinuxUartDriver ::readPoll_handler(FwIndexType portNum, Fw::Buffer& pollBuffer) {
    return this->readIntoBuff(this, pollBuffer) ? Drv::PollStatus::POLL_OK : Drv::PollStatus::POLL_ERROR;
}
typedef void (*taskRoutine)(void* ptr);

void LinuxUartDriver ::start(FwTaskPriorityType priority, Os::Task::ParamType stackSize, Os::Task::ParamType cpuAffinity) {
    Os::TaskString task("SerReader");

    FW_ASSERT(this->isConnected_recv_OutputPort(0));
    // must be connected for this routine
    FW_ASSERT(this->isConnected_allocate_OutputPort(0));
    Os::Task::Arguments arguments(task, serialReadToRecvOutTaskEntry, this, priority, stackSize, cpuAffinity);
    Os::Task::Status stat = this->m_readTask.start(arguments);
    FW_ASSERT(stat == Os::Task::OP_OK, stat);
}

void LinuxUartDriver ::quitReadThread() {
    this->m_quitReadThread = true;
}

Os::Task::Status LinuxUartDriver ::join() {
    return m_readTask.join();
}

}  // end namespace Drv

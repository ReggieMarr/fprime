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
#include "Fw/FPrimeBasicTypes.hpp"
#include "Fw/Logger/Logger.hpp"
#include "Fw/Time/TimeInterval.hpp"
#include "Fw/Types/BasicTypes.hpp"
#include "Fw/Types/String.hpp"
#include "Platform/PlatformTypes.h"

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

// #define DEBUG
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

static void printTtyConfig(termios2 const *tty) {
    // Check baud rate
    Fw::Logger::log("ispeed/ospeed: %u/%u\n", tty->c_ispeed, tty->c_ospeed);

    // Check flags
    Fw::Logger::log("c_cflag: requested=0x%X\n", tty->c_cflag);
    Fw::Logger::log("c_iflag: requested=0x%X\n", tty->c_iflag);
    Fw::Logger::log("c_lflag: requested=0x%X\n", tty->c_lflag);

    // Check specific important flags
    Fw::Logger::log("Hardware flow control: %s\n",
                       (tty->c_cflag & CRTSCTS) ? "ON" : "OFF");
    Fw::Logger::log("Software flow control: %s\n",
                       (tty->c_iflag & IXON) ? "ON" : "OFF");
    Fw::Logger::log("Canonical mode: %s\n",
                       (tty->c_lflag & ICANON) ? "ON" : "OFF");
    Fw::Logger::log("Echo: %s\n",
                       (tty->c_lflag & ECHO) ? "ON" : "OFF");
    Fw::Logger::log("VMIN: %u, VTIME: %u\n",
                       tty->c_cc[VMIN], tty->c_cc[VTIME]);
}

static void printUartConfig(UartConfig& cfg) {
    // Check baud rate
    Fw::Logger::log("baudrate: %u\n", cfg.baudRate);

    // Check flags
    Fw::Logger::log("dataBits: requested=0x%X\n", cfg.dataBits);
    Fw::Logger::log("stopBits_iflag: requested=0x%X\n", cfg.dataBits);

    // Check specific important flags
    Fw::Logger::log("Hardware flow control: %s\n", cfg.enableHardwareFlowControl ? "ON" : "OFF");
    Fw::Logger::log("Software flow control: %s\n", cfg.enableSoftwareFlowControl ? "ON" : "OFF");
    Fw::Logger::log("Canonical mode: %s\n", cfg.enableInputProcessingMode ? "ON" : "OFF");
    Fw::Logger::log("Echo: %s\n", cfg.enableEchoMode ? "ON" : "OFF");
    Fw::Logger::log("MinChars: %u, TimeoutMs: %u\n", cfg.minChars, cfg.timeoutMs);
}

static bool setTtyConfig(termios2* tty, UartConfig& cfg) {
    // Start by getting current flags rather than resetting them
    // We'll modify specific bits rather than overwriting everything

    // Clear and set appropriate bits for data bits
    tty->c_cflag &= ~CSIZE;  // Clear the data bits field
    switch (cfg.dataBits) {
        case UartConfig::DataBits::DATA_5: tty->c_cflag |= CS5; break;
        case UartConfig::DataBits::DATA_6: tty->c_cflag |= CS6; break;
        case UartConfig::DataBits::DATA_7: tty->c_cflag |= CS7; break;
        case UartConfig::DataBits::DATA_8: tty->c_cflag |= CS8; break;
        default: return false;
    }

    // Set stop bits
    if (cfg.stopBits == UartConfig::StopBits::STOP_1) {
        tty->c_cflag &= ~CSTOPB;
    } else {
        tty->c_cflag |= CSTOPB;
    }

    // Set flow control
    if (cfg.enableHardwareFlowControl) {
        tty->c_cflag |= CRTSCTS;
    } else {
        tty->c_cflag &= ~CRTSCTS;
    }

    if (cfg.enableSoftwareFlowControl) {
        tty->c_iflag |= (IXON | IXOFF);
    } else {
        tty->c_iflag &= ~(IXON | IXOFF | IXANY);
    }

    // Set local mode and receiver enabling
    if (cfg.localMode) {
        tty->c_cflag |= CLOCAL;
    } else {
        tty->c_cflag &= ~CLOCAL;
    }

    if (cfg.receiverEnable) {
        tty->c_cflag |= CREAD;
    } else {
        tty->c_cflag &= ~CREAD;
    }

    // Set parity
    switch (cfg.parity) {
        case UartConfig::Parity::PARITY_NONE:
            tty->c_cflag &= ~PARENB;
            break;
        case UartConfig::Parity::PARITY_EVEN:
            tty->c_cflag |= PARENB;
            tty->c_cflag &= ~PARODD;
            break;
        case UartConfig::Parity::PARITY_ODD:
            tty->c_cflag |= PARENB;
            tty->c_cflag |= PARODD;
            break;
        default:
            return false;
    }

    // Determine numeric baud speed
    speed_t baudspeed;
    switch (cfg.baudRate) {
        case UartConfig::BaudRate::BAUD_9600:   baudspeed = 9600;   break;
        case UartConfig::BaudRate::BAUD_19200:  baudspeed = 19200;  break;
        case UartConfig::BaudRate::BAUD_38400:  baudspeed = 38400;  break;
        case UartConfig::BaudRate::BAUD_57600:  baudspeed = 57600;  break;
        case UartConfig::BaudRate::BAUD_115K:   baudspeed = 115200; break;
        case UartConfig::BaudRate::BAUD_230K:   baudspeed = 230400; break;
        case UartConfig::BaudRate::BAUD_460K:   baudspeed = 460800; break;
        case UartConfig::BaudRate::BAUD_921K:   baudspeed = 921600; break;
        case UartConfig::BaudRate::BAUD_1000K:  baudspeed = 1000000;break;
        case UartConfig::BaudRate::BAUD_1152K:  baudspeed = 1152000;break;
        case UartConfig::BaudRate::BAUD_1500K:  baudspeed = 1500000;break;
        case UartConfig::BaudRate::BAUD_2000K:  baudspeed = 2000000;break;
#ifdef B2500000
        case UartConfig::BaudRate::BAUD_2500K:  baudspeed = 2500000;break;
#endif
#ifdef B3000000
        case UartConfig::BaudRate::BAUD_3000K:  baudspeed = 3000000;break;
#endif
#ifdef B3500000
        case UartConfig::BaudRate::BAUD_3500K:  baudspeed = 3500000;break;
#endif
#ifdef B4000000
        case UartConfig::BaudRate::BAUD_4000K:  baudspeed = 4000000;break;
#endif
        default:
            return false;
    }

    // Clear old baud bits and set new
    tty->c_cflag &= ~CBAUD;
    switch (cfg.baudRate) {
        case UartConfig::BaudRate::BAUD_9600:   tty->c_cflag |= B9600;   break;
        case UartConfig::BaudRate::BAUD_19200:  tty->c_cflag |= B19200;  break;
        case UartConfig::BaudRate::BAUD_38400:  tty->c_cflag |= B38400;  break;
        case UartConfig::BaudRate::BAUD_57600:  tty->c_cflag |= B57600;  break;
        case UartConfig::BaudRate::BAUD_115K:   tty->c_cflag |= B115200; break;
        case UartConfig::BaudRate::BAUD_230K:   tty->c_cflag |= B230400; break;
        case UartConfig::BaudRate::BAUD_460K:   tty->c_cflag |= B460800; break;
        case UartConfig::BaudRate::BAUD_921K:   tty->c_cflag |= B921600; break;
        default:
            // non-standard rate: use BOTHER
            tty->c_cflag |= BOTHER;
            break;
    }
    // Apply numeric speed regardless
    tty->c_ispeed = baudspeed;
    tty->c_ospeed = baudspeed;

    // Set canonical mode and echo settings
    if (cfg.enableInputProcessingMode) {
        tty->c_lflag |= ICANON;
        tty->c_lflag |= ISIG;
        tty->c_lflag |= IEXTEN;
    } else {
        tty->c_lflag &= ~ICANON;
        tty->c_lflag &= ~ISIG;
        tty->c_lflag &= ~IEXTEN;
    }

    if (cfg.enableEchoMode) {
        tty->c_lflag |= (ECHO | ECHOE | ECHOK);
    } else {
        tty->c_lflag &= ~(ECHO | ECHOE | ECHOK | ECHONL);
    }

    // Set VMIN and VTIME
    tty->c_cc[VMIN] = cfg.minChars;

    if (cfg.timeoutMs == -1) {
        tty->c_cc[VTIME] = 0;
    } else if (cfg.timeoutMs == 0) {
        tty->c_cc[VTIME] = 0;
    } else {
        tty->c_cc[VTIME] = (cc_t)((cfg.timeoutMs + 99) / 100);
    }

    // These were causing issues, setting specific terminal input/output settings
    if (cfg.enableInputProcessingMode) {
        // Canonical mode typically wants ICRNL
        tty->c_iflag |= ICRNL;
    } else {
        // Raw mode wants minimal processing
        tty->c_iflag &= ~(ICRNL | INLCR | IGNCR);
    }

    // Disable output processing for raw mode
    if (cfg.enableInputProcessingMode) {
        // If in canonical mode, you might want some output processing
        tty->c_oflag |= ONLCR;  // Map NL to CR-NL on output
    } else {
        // Raw output
        tty->c_oflag &= ~OPOST;
    }

    return true;
}

bool LinuxUartDriver::open(const char* const device, UartConfig& cfg, FwSizeType allocationSize) {
    Fw::LogStringArg deviceArg = device;
    this->m_allocationSize = allocationSize;

    // Check if device exists first
    if (access(device, F_OK) > 0) {
        Fw::LogStringArg errorStr = strerror(errno);
        this->log_WARNING_HI_OpenError(deviceArg, -1, errorStr);
        return false;
    }

    I32 ret = ::open(device, O_RDWR | O_NOCTTY | O_SYNC);
    if (ret < 0) {
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
    printUartConfig(cfg);
    printTtyConfig(&tty);

    if (!setTtyConfig(&tty, cfg)) {
        Fw::LogStringArg errorStr = strerror(errno);
        this->log_WARNING_HI_OpenError(deviceArg, this->m_fd, errorStr);
        ::close(this->m_fd);
        return false;
    }

    // Apply the settings
    ret = ioctl(this->m_fd, TCSETS2, &tty);
    if (ret < 0) {
        // Handle error
        Fw::LogStringArg errorStr = strerror(errno);
        this->log_WARNING_HI_OpenError(deviceArg, this->m_fd, errorStr);
        ::close(this->m_fd);
        return false;
    }

    Fw::Logger::log("Post set settings:\n");
    printTtyConfig(&tty);

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
        PlatformIntType xferSize = static_cast<PlatformIntType>(serBuffer.getSize());
        Fw::String byteStr;
        Fw::String buffStr;

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

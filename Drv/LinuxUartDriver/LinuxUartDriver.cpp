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

#include <unistd.h>
#include <Drv/LinuxUartDriver/LinuxUartDriver.hpp>
#include <Os/TaskString.hpp>

#include "Drv/ByteStreamDriverModel/PollStatusEnumAc.hpp"
#include "Drv/ByteStreamDriverModel/RecvStatusEnumAc.hpp"
#include "FpConfig.h"
#include "Fw/Logger/Logger.hpp"
#include "Fw/Time/TimeInterval.hpp"
#include "Fw/Types/BasicTypes.hpp"

#include <fcntl.h>
// #include <termios.h>
#include <cerrno>

// #include <termios.h>

#include <sys/ioctl.h>
#include <asm-generic/termbits.h>
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

bool LinuxUartDriver::open(const char* const device,
                          UartBaudRate baud,
                          UartFlowControl fc,
                          UartParity parity,
                          U32 allocationSize) {
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
    txTty.c_cflag &= ~PARENB; // No parity
    txTty.c_cflag &= ~CSTOPB; // 1 stop bit
    txTty.c_cflag &= ~CSIZE;
    txTty.c_cflag |= CS8;     // 8 data bits


    // Disable hardware flow control if needed (or enable if fc == HW_FLOW)
    txTty.c_cflag &= ~CRTSCTS;  // Disable hardware flow control by default
    // Configure control flags for local connection and enabling receiver
    txTty.c_cflag |= CLOCAL | CREAD; // turn on READ & ignore ctrl lines

    txTty.c_iflag &= ~(IXON | IXOFF | IXANY); // turn off s/w flow ctrl

    // Set raw mode (non-canonical, no echo, etc.)
    txTty.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    txTty.c_oflag &= ~OPOST;

    // Configure timeout: don't wait for a character, 2.5s timeout
    txTty.c_cc[VMIN] = 0;
    txTty.c_cc[VTIME] = 1; // Timeout at 1 decisecond or 100ms // static_cast<cc_t>(2500 / 100.0 + 0.5); // Convert ms to deciseconds

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
        this->ready_out(0); // Indicate the driver is connected
    }

    // All done!
    Fw::LogStringArg _arg = device;
    this->log_ACTIVITY_HI_PortOpened(_arg);
    if (this->isConnected_ready_OutputPort(0)) {
        this->ready_out(0);  // Indicate the driver is connected
    }
    return true;
}

// bool LinuxUartDriver::open(const char* const device,
//                           UartBaudRate baud,
//                           UartFlowControl fc,
//                           UartParity parity,
//                           U32 allocationSize) {
//     FW_ASSERT(device != nullptr);
//     NATIVE_INT_TYPE fd = -1;
//     NATIVE_INT_TYPE stat = -1;
//     this->m_allocationSize = allocationSize;
//     this->m_device = device;

//     fd = ::open(device, O_RDWR | O_NOCTTY | O_SYNC);
//     if (fd == -1) {
//         Fw::LogStringArg _arg = device;
//         Fw::LogStringArg _err = strerror(errno);
//         this->log_WARNING_HI_OpenError(_arg, this->m_fd, _err);
//         return false;
//     }
//     this->m_fd = fd;

//     // Get the current terminal settings
//     struct termios newtio;
//     stat = tcgetattr(fd, &newtio);
//     if (stat == -1) {
//         close(fd);
//         Fw::LogStringArg _arg = device;
//         Fw::LogStringArg _err = strerror(errno);
//         this->log_WARNING_HI_OpenError(_arg, fd, _err);
//         return false;
//     }

//     // Start with a clean state by zeroing the structure
//     // This is safer than modifying existing flags
//     memset(&newtio, 0, sizeof(newtio));

//     // Configure baud rate using BOTHER approach (like the old implementation)
// #ifdef BOTHER
//     // If BOTHER is defined, use the custom baud rate approach
//     struct termios2 tio2;
//     stat = ioctl(fd, TCGETS2, &tio2);
//     if (stat != -1) {
//         tio2.c_cflag &= ~CBAUD;
//         tio2.c_cflag |= BOTHER;

//         // Set the actual numeric baud rate based on the enum
//         switch (baud) {
//             case BAUD_3000K:
//                 tio2.c_ispeed = tio2.c_ospeed = 3000000;
//                 break;
//             // Add other cases as needed
//             default:
//                 // Handle default case
//                 close(fd);
//                 return false;
//         }

//         // Apply the settings
//         stat = ioctl(fd, TCSETS2, &tio2);
//         if (stat == -1) {
//             close(fd);
//             Fw::LogStringArg _arg = device;
//             Fw::LogStringArg _err = strerror(errno);
//             this->log_WARNING_HI_OpenError(_arg, fd, _err);
//             return false;
//         }
//     } else
// #endif
//     {
//         // Standard baud rate setting as fallback
//         speed_t relayRate = B0;
//         switch (baud) {
//             // ... your existing cases ...
// #ifdef B3000000
//             case BAUD_3000K:
//                 relayRate = B3000000;
//                 break;
// #endif
//             default:
//                 FW_ASSERT(0, static_cast<FwAssertArgType>(baud));
//                 break;
//         }

//         // Set standard baud rate
//         stat = cfsetispeed(&newtio, relayRate);
//         if (stat) {
//             close(fd);
//             Fw::LogStringArg _arg = device;
//             Fw::LogStringArg _err = strerror(errno);
//             this->log_WARNING_HI_OpenError(_arg, fd, _err);
//             return false;
//         }
//         stat = cfsetospeed(&newtio, relayRate);
//         if (stat) {
//             close(fd);
//             Fw::LogStringArg _arg = device;
//             Fw::LogStringArg _err = strerror(errno);
//             this->log_WARNING_HI_OpenError(_arg, fd, _err);
//             return false;
//         }
//     }

//     // Set data bits - IMPORTANT: Set CS8 AFTER clearing CSIZE
//     newtio.c_cflag &= ~CSIZE;  // Clear size bits
//     newtio.c_cflag |= CS8;     // Set 8 data bits

//     // Set parity
//     switch (parity) {
//         case PARITY_ODD:
//             newtio.c_cflag |= (PARENB | PARODD);
//             break;
//         case PARITY_EVEN:
//             newtio.c_cflag |= PARENB;
//             break;
//         case PARITY_NONE:
//             newtio.c_cflag &= ~PARENB;
//             break;
//         default:
//             FW_ASSERT(0, parity);
//             break;
//     }

//     // Set flow control
//     if (fc == HW_FLOW) {
//         newtio.c_cflag |= CRTSCTS;  // Enable hardware flow control
//     } else {
//         newtio.c_cflag &= ~CRTSCTS; // Disable hardware flow control
//     }

//     // Set other flags exactly as in the old implementation
//     newtio.c_cflag |= CLOCAL | CREAD;  // Enable receiver and ignore modem control lines
//     newtio.c_cflag &= ~CSTOPB;         // 1 stop bit

//     // Input flags - CRITICAL: Don't overwrite with INPCK alone
//     newtio.c_iflag &= ~(IXON | IXOFF | IXANY); // Disable software flow control
//     if (parity != PARITY_NONE) {
//         newtio.c_iflag |= INPCK;  // Only enable parity checking if parity is enabled
//     } else {
//         newtio.c_iflag &= ~INPCK; // Disable parity checking for no parity
//     }

//     // Local flags - Set raw mode
//     newtio.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);

//     // Output flags - Raw output
//     newtio.c_oflag &= ~OPOST;

//     // Set timeouts
//     FwSizeType timeoutMs = 5000;
//     newtio.c_cc[VMIN] = 1;  // Return when at least 1 character is available
//     newtio.c_cc[VTIME] = static_cast<cc_t>(timeoutMs / 100.0 + 0.5); // Timeout in deciseconds

//     // Flush old data
//     tcflush(fd, TCIFLUSH);

//     // Apply the settings
//     stat = tcsetattr(fd, TCSANOW, &newtio);
//     if (stat == -1) {
//         close(fd);
//         Fw::LogStringArg _arg = device;
//         Fw::LogStringArg _err = strerror(errno);
//         this->log_WARNING_HI_OpenError(_arg, fd, _err);
//         return false;
//     }

//     // Now we're ready
//     Fw::LogStringArg _arg = this->m_device;
//     this->log_ACTIVITY_HI_PortOpened(_arg);
//     if (this->isConnected_ready_OutputPort(0)) {
//         this->ready_out(0); // Indicate the driver is connected
//     }

//     return true;
// }

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
        FW_ASSERT_NO_OVERFLOW(serBuffer.getSize(), size_t);
        size_t xferSize = static_cast<size_t>(serBuffer.getSize());

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

bool LinuxUartDriver ::readIntoBuff(LinuxUartDriver* comp, Fw::Buffer &buff) {
    int stat = 0;

    size_t bytesRead = 0;
    size_t numBytes = buff.getSize();
    uint8_t* ptr = buff.getData();
#ifdef DEBUG
    Fw::Logger::log("Reading %p %d for %s - %s %d\n", ptr, numBytes, comp->m_objName.toChar(), comp->m_device, comp->m_fd);
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

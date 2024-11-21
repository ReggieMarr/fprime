// ======================================================================
// \title  TMSpaceDataLink ProtocolInterface.cpp
// \author Reginald Marr
// \brief  Implementation of protocol interface to support TMSpaceDataLink
//         This includes the implementation a protocol entity which based off
//         of CCSDS 132.0-B-3 TM Space Data Link Protocol
//
// This implementation follows the protocol entity specification from section 2.3
// of CCSDS 132.0-B-3, implementing the required functions while explicitly
// excluding those listed as not performed by the protocol entity.
//
// \copyright
// Copyright 2009-2021, by the California Institute of Technology.
// ALL RIGHTS RESERVED.  United States Government Sponsorship
// acknowledged.
//
// ======================================================================

#ifndef SVC_TM_SPACE_DATA_LINK_PROTOCOL_HPP
#define SVC_TM_SPACE_DATA_LINK_PROTOCOL_HPP

#include <Svc/FramingProtocol/FramingProtocol.hpp>
#include <cstddef>
#include "Svc/FrameAccumulator/FrameDetector/StartLengthCrcDetector.hpp"
#include "Fw/Com/ComBuffer.hpp"
#include "Fw/Types/SerialStatusEnumAc.hpp"
#include "Fw/Types/Serializable.hpp"
#include "Svc/FramingProtocol/CCSDSProtocols/CCSDSProtocolDefs.hpp"
#include "Svc/FrameAccumulator/FrameDetector/StartLengthCrcDetector.hpp"
#include "config/FpConfig.h"
#include "Os/Queue.hpp"
#include "TransferFrame.hpp"
#include "ManagedParameters.hpp"
#include <vector>
#include <queue>
#include <map>
#include <memory>
#include <cstdint>

namespace TMSpaceDataLink {

enum class ServiceType { VCP, VCA };

class BaseChannel {
protected:
    FwSizeType m_frameLength;
    Os::Queue m_queue;  // Queue for inter-task communication
public:
    BaseChannel(FwSizeType frameLength) : m_frameLength(frameLength) {}
    virtual ~BaseChannel() {}

    virtual bool propogate(const Fw::Buffer& sdu) = 0;  // Abstract process method
};

// Virtual Channel Implementation
// The Segmenting and blocking functionality described in 2.3.1(b)
// is implemented by the PacketProccessing_handler (if the VCP service is registered)
// And the VirtualChannelGeneration_handler.
class VirtualChannelSender : public BaseChannel {
public:
  VirtualChannelSender(VirtualChannelParams_t const &params, FwSizeType const transferFrameSize);

    // Register a service for the virtual channel
    void registerService(ServiceType type) {
        m_registeredServices[type] = true;
    }

    // Handles incoming user data, first attempts to process packets (if supported)
    // by calling PacketProcessing_handler then creates a frame representation of
    // the virtual channel via VirtualChannelGeneration_handler
    bool propogate(const Fw::Buffer& sdu) override;
private:
    bool PacketProcessing_handler(const Fw::Buffer& sdu);
    bool VirtualChannelGeneration_handler(const Fw::Buffer& sdu);
    std::map<ServiceType, bool> m_registeredServices;  // Registered services
};

// Master Channel Implementation
class MasterChannelSender : public BaseChannel {
public:
    MasterChannelSender(MasterChannelParams_t const &params, FwSizeType const transferFrameLength);

    bool VirtualChannelMultiplexing_handler(const std::array<Fw::Buffer, 8>& vcFrames);
    bool MasterChannelGeneration_handler(Fw::Buffer& frame);
    bool propogate(const Fw::Buffer& sdu) override;
private:
    std::array<VirtualChannelSender, MAX_VIRTUAL_CHANNELS> m_subChannels;
};

// Physical Channel Implementation
class PhysicalChannelSender : public BaseChannel {
public:
    PhysicalChannelSender(PhysicalChannelParams_t const &params);

    bool MasterChannelMultiplexing_handler(const std::array<Fw::Buffer, 8>& mcFrames);
    bool AllFramesGeneration_handler(Fw::Buffer& frame);
    bool propogate(const Fw::Buffer& sdu) override;
private:
    std::array<MasterChannelSender, MAX_MASTER_CHANNELS> m_subChannels;
};

/**
 * @class ProtocolEntity
 * @brief Implementation of the TM Space Data Link Protocol Entity
 *
 * This class implements the protocol functions specified in CCSDS 132.0-B-3
 * Section 2.3.1. It handles the generation and processing of protocol data units
 * (TM Transfer Frames) at a constant rate.
 */
class ProtocolEntity {
public:
    ProtocolEntity(ManagedParameters_t const &params);

    // Process incoming telemetry data
    bool UserComIn_handler(Fw::Buffer data, U32 context);

    // Generate the next TM Transfer Frame for transmission
    // Implements constant rate transfer requirement from 2.3.1
    void generateNextFrame();

    // Internal helper for idle frame generation
    void generateIdleData(Fw::Buffer& frame);
private:
    PhysicalChannelSender m_physicalChannel;
    MasterChannelSender m_masterChannels[8];         // Support for up to 8 master channels
    VirtualChannelSender m_virtualChannels[8][8];   // Support for 8 virtual channels per master channel

};

}  // namespace TMSpaceDataLink

namespace Svc {
class TMSpaceDataLinkProtocol: public FramingProtocol {

  public:

    TMSpaceDataLinkProtocol(const TMSpaceDataLink::MissionPhaseParameters_t& missionParams, const FwSizeType dataFieldSize):
        m_transferFrame(TMSpaceDataLink::TransferFrame(missionParams)), m_dataFieldSize(dataFieldSize) {}

    void setup(const TMSpaceDataLink::MissionPhaseParameters_t& missionParams, const FwSizeType dataFieldSize);

    void frame(const U8* const data, const U32 size, Fw::ComPacket::ComPacketType packet_type) override;

  private:
    TMSpaceDataLink::TransferData_t m_transferData = {0};
    TMSpaceDataLink::TransferFrame m_transferFrame;
    const FwSizeType m_dataFieldSize{Svc::TM_DATA_FIELD_DFLT_SIZE};
};
}  // namespace Svc
#endif  // SVC_TM_SPACE_DATA_LINK_PROTOCOL_HPP

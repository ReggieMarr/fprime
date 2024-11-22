#ifndef SVC_TM_SPACE_DATA_LINK_CHANNELS_HPP
#define SVC_TM_SPACE_DATA_LINK_CHANNELS_HPP

#include <Svc/FramingProtocol/FramingProtocol.hpp>
#include <cstddef>
#include "Svc/FrameAccumulator/FrameDetector/StartLengthCrcDetector.hpp"
#include "Fw/Com/ComBuffer.hpp"
#include "Fw/Types/SerialStatusEnumAc.hpp"
#include "Fw/Types/Serializable.hpp"
#include "Svc/FramingProtocol/CCSDSProtocols/CCSDSProtocolDefs.hpp"
#include "Svc/FrameAccumulator/FrameDetector/StartLengthCrcDetector.hpp"
#include "Svc/FramingProtocol/CCSDSProtocols/TMSpaceDataLink/Services.hpp"
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

// Addresses derived from CCSDS 132.0-B-3 2.1.3
// Master Channel Identifier (MCID)
typedef struct MCID_s {
    // Spacecraft Identifier
    NATIVE_UINT_TYPE SCID;
    // Transfer Version Number
    NATIVE_UINT_TYPE TFVN;
} MCID_t;

// Global Virtual Channel Identifier (GVCID)
typedef struct GVCID_s {
    // Master Channel Identifier (MCID)
    MCID_t MCID;
    // Virtual Channel Identifier (VCID)
    NATIVE_UINT_TYPE VCID;
} GVCID_t;

class BaseChannel {
protected:
    FwSizeType m_transferFrameSize;
    Os::Queue m_queue;  // Queue for inter-task communication
public:
    BaseChannel(FwSizeType transferFrameSize) : m_transferFrameSize(transferFrameSize) {}
    BaseChannel(BaseChannel const &other) :
        m_transferFrameSize(other.m_transferFrameSize) {}

    BaseChannel& operator=(const BaseChannel& other) {
        // Can't do this
        // TODO fix queue swapping
        // this->m_queue = other.m_queue;
        if (this != &other) {
            this->m_transferFrameSize = other.m_transferFrameSize;
        }
        return *this;
    }

    virtual ~BaseChannel() {}

    virtual bool propogate(const Fw::Buffer& sdu) = 0;  // Abstract process method
};

// Virtual Channel Implementation
// The Segmenting and blocking functionality described in 2.3.1(b)
// is implemented by the PacketProccessing_handler (if the VCP service is registered)
// And the VirtualChannelGeneration_handler.
class VirtualChannelSender : public BaseChannel {
public:
    VirtualChannelSender(VirtualChannelParams_t const &params,
                         FwSizeType const transferFrameSize, GVCID_t id) :
                           BaseChannel(transferFrameSize) {}

    VirtualChannelSender(): BaseChannel(0) {};
    VirtualChannelSender(VirtualChannelSender const &other) :
        BaseChannel(other) {}

    VirtualChannelSender& operator=(const VirtualChannelSender& other) {
        if (this != &other) {
            BaseChannel::operator=(other);
        }
        return *this;
    }

    GVCID_t getId() {return m_id;  }

    // Handles incoming user data, first attempts to process packets (if supported)
    // by calling PacketProcessing_handler then creates a frame representation of
    // the virtual channel via VirtualChannelGeneration_handler
    bool propogate(const Fw::Buffer& sdu) override;
private:
    bool PacketProcessing_handler(const Fw::Buffer& sdu);
    bool VirtualChannelGeneration_handler(const Fw::Buffer& sdu);

    // NOTE should be const
    GVCID_t m_id;
};

// Master Channel Implementation
class MasterChannelSender : public BaseChannel {
public:
    MasterChannelSender(MasterChannelParams_t const &params,
                        FwSizeType const transferFrameLength, MCID_t id) :
                    BaseChannel(transferFrameLength),
                    m_subChannels(createSubChannels(params, transferFrameLength, id)),
                    m_id(id) {}

    MasterChannelSender(): BaseChannel(0) {};
    MasterChannelSender(const MasterChannelSender& other) :
        BaseChannel(other),
        m_subChannels(other.m_subChannels), m_id(other.m_id) {}

    MasterChannelSender& operator=(const MasterChannelSender& other) {
        if (this != &other) {
            BaseChannel::operator=(other);
            // TODO set members
            // m_registeredServices = other.m_registeredServices;
            m_subChannels = other.m_subChannels;
            m_id = other.m_id;
        }
        return *this;
    }
    MCID_t getId() {return m_id;  }

    bool propogate(const Fw::Buffer& sdu) override;

private:
    bool VirtualChannelMultiplexing_handler(const std::array<Fw::Buffer, 8>& vcFrames);
    bool MasterChannelGeneration_handler(Fw::Buffer& frame);

    static std::array<VirtualChannelSender, MAX_VIRTUAL_CHANNELS>
        createSubChannels(MasterChannelParams_t const &params,
                          NATIVE_UINT_TYPE const transferFrameSize,
                          MCID_t const mcid)
    {
        std::array<VirtualChannelSender, MAX_VIRTUAL_CHANNELS> channels;
        for (NATIVE_UINT_TYPE i = 0; i < params.numSubChannels; i++) {
            GVCID_t vcid = {mcid, i};
            channels.at(i) = VirtualChannelSender(params.subChannels[i], transferFrameSize, vcid);
        }
        return channels;
    }

    // NOTE should be const
    MCID_t m_id;
    std::array<VirtualChannelSender, MAX_VIRTUAL_CHANNELS> m_subChannels;
};

// Physical Channel Implementation
class PhysicalChannelSender : public BaseChannel {
public:
    PhysicalChannelSender(PhysicalChannelParams_t const &params) :
        BaseChannel(params.transferFrameSize),
        m_subChannels(createSubChannels(params)) {}
    PhysicalChannelSender(): BaseChannel(0) {};
    PhysicalChannelSender(const PhysicalChannelSender& other) :
        BaseChannel(other),
        m_subChannels(other.m_subChannels) {}

    PhysicalChannelSender& operator=(const PhysicalChannelSender& other) {
        if (this != &other) {
            BaseChannel::operator=(other);
            m_subChannels = other.m_subChannels;
        }
        return *this;
    }

    bool propogate(const Fw::Buffer& sdu) override;
private:
    bool MasterChannelMultiplexing_handler(const std::array<Fw::Buffer, 8>& mcFrames);
    bool AllFramesGeneration_handler(Fw::Buffer& frame);

    static std::array<MasterChannelSender, MAX_MASTER_CHANNELS> createSubChannels(
        const PhysicalChannelParams_t& params)
    {
        std::array<MasterChannelSender, MAX_MASTER_CHANNELS> channels;
        for (NATIVE_UINT_TYPE i = 0; i < params.numSubChannels; i++) {
            MCID_t mcid = {params.subChannels.at(i).spaceCraftId, params.transferFrameVersion};
            channels.at(i) = MasterChannelSender(params.subChannels.at(i), params.transferFrameSize, mcid);
        }
        return channels;
    }

    std::array<MasterChannelSender, MAX_MASTER_CHANNELS> m_subChannels;
};

}  // namespace TMSpaceDataLink

#endif  // SVC_TM_SPACE_DATA_LINK_CHANNELS_HPP

#ifndef SVC_TM_SPACE_DATA_LINK_CHANNELS_HPP
#define SVC_TM_SPACE_DATA_LINK_CHANNELS_HPP

#include <Svc/FramingProtocol/FramingProtocol.hpp>
#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <queue>
#include <vector>
#include "FpConfig.h"
#include "Fw/Com/ComBuffer.hpp"
#include "Fw/Types/Assert.hpp"
#include "Fw/Types/SerialStatusEnumAc.hpp"
#include "Fw/Types/Serializable.hpp"
#include "Fw/Types/String.hpp"
#include "ManagedParameters.hpp"
#include "Os/Queue.hpp"
#include "Svc/FrameAccumulator/FrameDetector/StartLengthCrcDetector.hpp"
#include "Svc/FramingProtocol/CCSDSProtocols/CCSDSProtocolDefs.hpp"
#include "Svc/FramingProtocol/CCSDSProtocols/TMSpaceDataLink/ManagedParameters.hpp"
#include "Svc/FramingProtocol/CCSDSProtocols/TMSpaceDataLink/Services.hpp"
#include "Svc/FramingProtocol/CCSDSProtocols/TMSpaceDataLink/TransferFrameQueue.hpp"
#include "TransferFrame.hpp"

namespace TMSpaceDataLink {
constexpr FwSizeType CHANNEL_Q_DEPTH = 10;

template <FwSizeType TransferFrameSize = 255>
class VirtualChannel : public VCAService, public FrameService {
  public:
    using TransferInType = Fw::ComBuffer;
    using TransferOutType = TransferFrame<TransferFrameSize>;
    static constexpr FwSizeType TransferOutSize = TransferFrame<TransferFrameSize>::SIZE;

    // require parameterless construction to make an array of virtual channels
    VirtualChannel() : VCAService({}), FrameService({}), id({}), m_externalQueue() {}

    VirtualChannel(VirtualChannelParams_t const& params, GVCID_t id)
        : VCAService(id), FrameService(id), id(id), m_externalQueue() {
        Os::Queue::Status status;
        Fw::String name = "Virtual Channel";
        m_externalQueue.create(name, CHANNEL_Q_DEPTH);
        FW_ASSERT(status == Os::Queue::Status::OP_OK, status);
    }

    VirtualChannel& operator=(const VirtualChannel& other) {
        // NOTE can't really support this at the moment because we can't copy queues
        FW_ASSERT(this == &other);
        id = other.id;
        // m_q = other.m_q;
        return *this;
    }

    // NOTE should be const
    GVCID_t id;

    // NOTE we should probably be able to pass a const buffer here
    bool transfer(TransferInType& transferBuffer) {
        // Specific implementation for VCAService with UserTransferFrame
        VCA_SDU_t vcaSDU;
        VCA_Request_t vcaRequest;

        bool status = false;

        status = generateVCAPrimitive(transferBuffer, vcaRequest);
        FW_ASSERT(status, status);

        TransferOutType frame;
        status = ChannelGeneration_handler(vcaRequest, frame);
        FW_ASSERT(status, status);

        FwQueuePriorityType priority = 0;
        Os::Queue::Status qStatus = Os::Queue::Status::OP_OK;
        qStatus = m_externalQueue.send(frame, priority, Os::QueueInterface::BlockingType::BLOCKING);
        return qStatus == Os::Queue::Status::OP_OK;
    }

    Os::Generic::TransformFrameQueue<TransferFrameSize> m_externalQueue;  // Queue for inter-task communication
  protected:
    FwSizeType m_transferFrameSize;
    U8 channelCount = 0;
    bool ChannelGeneration_handler(VCA_Request_t const& request, TransferOutType& channelOut) {
        bool status = true;
        // Was something like this originally
        // FrameSDU_t framePrim;
        // status = sender->generateFramePrimitive(request, framePrim);
        DataField dataField(request.sdu);
        // TODO the ManagedParameters should propogate to this
        MissionPhaseParameters_t missionParams;
        TransferData_t transferData;
        // TODO do this
        // transferData.dataFieldDesc = request.statusFields
        // NOTE could just as easily get this from the id but this matches the spec better
        transferData.virtualChannelId = request.sap.VCID;
        // Explicitly do not set this
        // transferData.masterChannelFrameCount
        // TODO this should some how be propogated from the transfer call or the masterChannel parent
        static U8 vcFrameCount = 0;
        transferData.virtualChannelFrameCount = vcFrameCount++;
        // NOTE since we don't have the OCF or FSH services defined we don't
        // do anything to the secondary header or operational control frame
        PrimaryHeader primaryHeader(missionParams, transferData);
        primaryHeader.setVirtualChannelCount(channelCount++);
        TransferOutType frame(primaryHeader, dataField);
        // FIXME this I think is bad memory management
        channelOut = frame;

        FW_ASSERT(status, status);
        return true;
    };
};

// Master Channel Implementation
template <FwSizeType TransferInSize = 255>
class MasterChannel {
    MasterChannelParams_t dfltParams{};

  public:
    using TransferInType = typename VirtualChannel<TransferInSize>::TransferOutType;
    // it's actually a q but each item contains all virtual channels that we are connected to
    // NOTE replace MAX_VIRTUAL_CHANNELS with some init param
    // using TransferOutItemType = std::array<TransferInType, MAX_VIRTUAL_CHANNELS>;
    // how many transfers we'll hold internally before we have to dump
    // note this is basically just an internal queue
    static constexpr FwSizeType InternalTransferOutLength = MAX_VIRTUAL_CHANNELS;
    using InternalTransferOutType = std::array<TransferInType, InternalTransferOutLength>;
    // TODO create a queue that can handle a queue of master channels
    using TransferOutType = typename VirtualChannel<TransferInSize>::TransferOutType;

    // NOTE should be const if a public member
    MCID_t id;
    // This should really hold a queue containing sets of transform frames
    Os::Generic::TransformFrameQueue<TransferInSize> m_externalQueue;  // Queue for inter-task communication
    MasterChannel() : id({}), m_params(dfltParams) {}

    MasterChannel(MasterChannelParams_t& params, MCID_t id) : id(id), m_params(params) {
        Os::Queue::Status status;
        Fw::String name = "Master Channel";
        // NOTE this is very wrong at the moment
        m_externalQueue.create(name, CHANNEL_Q_DEPTH);
        FW_ASSERT(status == Os::Queue::Status::OP_OK, status);

        for (U8 i = 0; i < params.numSubChannels; i++) {
            GVCID_t vcid = {id, i};
            m_subChannels.at(i) = VirtualChannel<TransferInSize>(params.subChannels[i], vcid);
        }
    }

    MasterChannel(const MasterChannel& other) : m_params(other.m_params), id(other.id) {
        for (U8 i = 0; i < m_params.numSubChannels; i++) {
            GVCID_t vcid = {id, i};
            m_subChannels.at(i) = VirtualChannel<TransferInSize>(m_params.subChannels[i], vcid);
        }
    }

    MasterChannel& operator=(const MasterChannel& other) {
        if (this != &other) {
            // TODO set members
            // m_registeredServices = other.m_registeredServices;
            m_subChannels = other.m_subChannels;
            id = other.id;
        }
        return *this;
    }

    bool getChannel(GVCID_t const gvcid, VirtualChannel<TransferInSize>& vc) {
        FW_ASSERT(gvcid.MCID == id);
        NATIVE_UINT_TYPE i = 0;
        while (i++ < m_subChannels.size()) {
            if (m_subChannels.at(i).id == gvcid) {
                vc = m_subChannels.at(i);
                return true;
            }
        }
        return false;
    }

    bool transfer() {
        // Specific implementation for VirtualChannel with no underlying services
        // TransferOutType masterChannelTransferItems;
        std::array<TransferInType, MAX_VIRTUAL_CHANNELS> masterChannelTransferItems;
        FwQueuePriorityType priority = 0;
        for (NATIVE_UINT_TYPE i = 0; i < m_params.numSubChannels; i++) {
            // This collects frames from various virtual channels and muxes them
            TransferInType frame;
            FwSizeType actualsize;
            Os::Queue::Status qStatus = Os::Queue::Status::OP_OK;
            qStatus = m_subChannels.at(i).m_externalQueue.receive(frame, Os::QueueInterface::BlockingType::BLOCKING,
                                                                  priority);
            // If this were to fail we should create an only idle data (OID) transfer frame as per CCSDS 4.2.4.4
            FW_ASSERT(qStatus == Os::Queue::Status::OP_OK, qStatus);
            // TODO do some sort of id matching
            VirtualChannelMultiplexing_handler(frame, i, masterChannelTransferItems);
        }
        MasterChannelGeneration_handler(masterChannelTransferItems);

        Os::Queue::Status qStatus = Os::Queue::Status::OP_OK;
        for (NATIVE_UINT_TYPE i = 0; i < m_params.numSubChannels; i++) {
            qStatus = m_externalQueue.send(masterChannelTransferItems.at(i), priority,
                                           Os::QueueInterface::BlockingType::BLOCKING);
            FW_ASSERT(qStatus == Os::Queue::Status::OP_OK, qStatus);
        }

        return true;
    }

  private:
    // NOTE should be const
    std::array<VirtualChannel<TransferInSize>, MAX_VIRTUAL_CHANNELS> m_subChannels{};
    MasterChannelParams_t& m_params;
    U8 m_channelCount = 0;

    bool VirtualChannelMultiplexing_handler(TransferInType& virtualChannelOut,
                                            NATIVE_UINT_TYPE vcid,
                                            InternalTransferOutType& masterChannelTransfer) {
        // Trivial for now
        masterChannelTransfer.at(vcid) = virtualChannelOut;
        // TODO implement 4.2.4.4 here
        return true;
    }

    bool MasterChannelGeneration_handler(InternalTransferOutType& masterChannelTransfer) {
        for (NATIVE_UINT_TYPE i = 0; i < masterChannelTransfer.size(); i++) {
            TransferInType inItem = masterChannelTransfer.at(i);
            PrimaryHeader primHeader = inItem.getPrimaryHeader();
            primHeader.setMasterChannelCount(m_channelCount++);
        }
        return true;
    };
};

// Physical Channel Implementation
template <FwSizeType TransferFrameSize>
class PhysicalChannel {
    using TransferInType = typename MasterChannel<TransferFrameSize>::TransferOutType;
    // it's actually a q but each item contains all virtual channels that we are connected to
    // NOTE replace MAX_VIRTUAL_CHANNELS with some init param
    // using TransferOutItemType = std::array<TransferInType, MAX_VIRTUAL_CHANNELS>;
    // how many transfers we'll hold internally before we have to dump
    // note this is basically just an internal queue
    static constexpr FwSizeType TransferOutLength = 10;
    using TransferOutType = std::array<TransferInType, TransferOutLength>;

  public:
    PhysicalChannel(PhysicalChannelParams_t& params) : id(params.channelName), m_params(params) {
        Os::Queue::Status status;
        Fw::String name = "Physical Channel";
        // NOTE this is very wrong at the moment
        m_externalQueue.create(name, CHANNEL_Q_DEPTH);
        FW_ASSERT(status == Os::Queue::Status::OP_OK, status);

        for (U8 i = 0; i < params.numSubChannels; i++) {
            MCID_t mcid = {m_params.subChannels.at(i).spaceCraftId, m_params.transferFrameVersion};
            m_subChannels.at(i) = MasterChannel<TransferFrameSize>(params.subChannels[i], mcid);
        }
    }

    PhysicalChannel(const PhysicalChannel& other) : m_params(other.m_params), id(other.id) {
        for (U8 i = 0; i < m_params.numSubChannels; i++) {
            MCID_t mcid = {m_params.subChannels.at(i).spaceCraftId, m_params.transferFrameVersion};
            m_subChannels.at(i) = MasterChannel<TransferFrameSize>(m_params.subChannels.at(i), mcid);
        }
    }

    PhysicalChannel& operator=(const PhysicalChannel& other) {
        if (this != &other) {
            m_subChannels = other.m_subChannels;
            id = other.id;
        }
        return *this;
    }

    bool getChannel(MCID_t const mcid, VirtualChannel<TransferFrameSize>& vc) {
        for (NATIVE_UINT_TYPE i = 0; i < m_subChannels.size(); i++) {
            if (m_subChannels.at(i).id == mcid) {
                return true;
            }
        }
        return false;
    }

    bool getChannel(GVCID_t const gvcid, VirtualChannel<TransferFrameSize>& vc) {
        for (NATIVE_UINT_TYPE i = 0; i < m_subChannels.size(); i++) {
            if (m_subChannels.at(i).getChannel(gvcid, vc)) {
                return true;
            }
        }
        return false;
    }

    bool transfer() {
        // Specific implementation for VirtualChannel with no underlying services
        TransferOutType masterChannelTransferItems;
        FwQueuePriorityType priority = 0;
        for (NATIVE_UINT_TYPE i = 0; i < m_params.numSubChannels; i++) {
            // This collects frames from various virtual channels and muxes them
            TransferInType frame;
            Os::Queue::Status qStatus = Os::Queue::Status::OP_OK;
            qStatus = m_subChannels.at(i).m_externalQueue.receive(frame, Os::QueueInterface::BlockingType::BLOCKING,
                                                                  priority);
            // If this were to fail we should create a transfer frame as per CCSDS 4.2.4.4
            FW_ASSERT(qStatus == Os::Queue::Status::OP_OK, qStatus);
            MasterChannelMultiplexing_handler(frame, i, masterChannelTransferItems);
        }
        AllFramesChannelGeneration_handler(masterChannelTransferItems);

        Os::Queue::Status qStatus = Os::Queue::Status::OP_OK;
        for (NATIVE_UINT_TYPE i = 0; i < masterChannelTransferItems.size(); i++) {
            qStatus = m_externalQueue.send(masterChannelTransferItems.at(i), priority,
                                           Os::QueueInterface::BlockingType::BLOCKING);
            FW_ASSERT(qStatus == Os::Queue::Status::OP_OK, qStatus);
        }

        return true;
    }

    // const Fw::String id;
    Fw::String id;
    Os::Generic::TransformFrameQueue<TransferFrameSize> m_externalQueue;  // Queue for inter-task communication

  private:
    PhysicalChannelParams_t& m_params;

    void generateIdleData(Fw::Buffer& frame) {
        // Generate an idle frame with appropriate First Header Pointer
    }

    bool MasterChannelMultiplexing_handler(TransferInType& masterChannelOut,
                                           NATIVE_UINT_TYPE mcid,
                                           TransferOutType& physicalChannelOut) {
        // Trivial for now
        physicalChannelOut.at(mcid) = masterChannelOut;
        // TODO implement 4.2.6.4 here
        return true;
    }

    bool AllFramesChannelGeneration_handler(TransferOutType& masterChannelTransfer) {
        for (NATIVE_UINT_TYPE i = 0; i < masterChannelTransfer.size(); i++) {
            TransferInType inItem = masterChannelTransfer.at(i);
            // inItem.setFrameErrorControlField();
        }
        return true;
    }

    std::array<MasterChannel<TransferFrameSize>, MAX_MASTER_CHANNELS> m_subChannels;
};

}  // namespace TMSpaceDataLink

#endif  // SVC_TM_SPACE_DATA_LINK_CHANNELS_HPP

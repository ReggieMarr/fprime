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
#include "TransferFrame.hpp"

namespace TMSpaceDataLink {
constexpr FwSizeType CHANNEL_Q_DEPTH = 10;

class VirtualChannel : public VCAService, public FrameService {
  public:
    using TransferInType = Fw::ComBuffer;
    using TransferOutType = TransferFrame;
    static constexpr FwSizeType TransferOutSize = sizeof(TransferOutType);

    // require parameterless construction to make an array of virtual channels
    VirtualChannel() : VCAService({}), FrameService({}), id({}), m_q() {}

    VirtualChannel(VirtualChannelParams_t const& params, FwSizeType const transferFrameSize, GVCID_t id)
        : VCAService(id), FrameService(id), id(id), m_transferFrameSize(transferFrameSize), m_q() {
        Os::Queue::Status status;
        Fw::String name = "Virtual Channel";
        // NOTE this is very wrong at the moment
        m_q.create(name, CHANNEL_Q_DEPTH, sizeof(TransferFrame));
        FW_ASSERT(status == Os::Queue::Status::OP_OK, status);
    }

    VirtualChannel& operator=(const VirtualChannel& other) {
        FW_ASSERT(this == &other);
        return *this;
        // NOTE can't really support this at the moment because we can't copy queues
    }

    // NOTE should be const
    GVCID_t id;

    // NOTE we should probably be able to pass a const buffer here
    bool transfer(TransferInType& transferBuffer) {
        // Specific implementation for VCAService with TransferFrame
        VCA_SDU_t vcaSDU;
        VCA_Request_t vcaRequest;

        bool status = false;

        status = generateVCAPrimitive(transferBuffer, vcaRequest);
        FW_ASSERT(status, status);

        TransferFrame frame;
        status = ChannelGeneration_handler(vcaRequest, frame);
        FW_ASSERT(status, status);

        FwQueuePriorityType priority = 0;
        Os::Queue::Status qStatus = Os::Queue::Status::OP_OK;
        qStatus = m_q.send(static_cast<const U8*>(transferBuffer.getBuffAddr()),
                           static_cast<FwSizeType>(transferBuffer.getBuffLength()), priority,
                           Os::QueueInterface::BlockingType::BLOCKING);
        // qStatus = m_q.send(frame, priority, Os::QueueInterface::BlockingType::BLOCKING);
        return qStatus == Os::Queue::Status::OP_OK;
    }

    Os::Queue m_q;  // Queue for inter-task communication
  protected:
    FwSizeType m_transferFrameSize;
    U8 channelCount = 0;
    bool ChannelGeneration_handler(VCA_Request_t const& request, TransferFrame& channelOut) {
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
        TransferFrame frame(primaryHeader, dataField);
        // FIXME this I think is bad memory management
        channelOut = frame;

        FW_ASSERT(status, status);
        return true;
    };
};

// Master Channel Implementation
class MasterChannel {
    using TransferInType = VirtualChannel::TransferOutType;
    // it's actually a q but each item contains all virtual channels that we are connected to
    // NOTE replace MAX_VIRTUAL_CHANNELS with some init param
    // using TransferOutItemType = std::array<TransferInType, MAX_VIRTUAL_CHANNELS>;
    // how many transfers we'll hold internally before we have to dump
    // note this is basically just an internal queue
    static constexpr FwSizeType TransferOutLength = 10;
    using TransferOutType = std::array<TransferInType, TransferOutLength>;

  public:
    // NOTE should be const if a public member
    MCID_t id;
    using MasterChannel_Q_Item_t = std::array<U8, sizeof(TransferFrame)>;
    Os::Queue m_externalQueue;  // Queue for inter-task communication

    MasterChannel(MasterChannelParams_t& params, FwSizeType const transferFrameSize, MCID_t id)
        : id(id), m_transferFrameSize(transferFrameSize), m_params(params) {
        Os::Queue::Status status;
        Fw::String name = "Master Channel";
        // NOTE this is very wrong at the moment
        m_externalQueue.create(name, CHANNEL_Q_DEPTH, sizeof(TransferFrame));
        FW_ASSERT(status == Os::Queue::Status::OP_OK, status);

        for (NATIVE_UINT_TYPE i = 0; i < params.numSubChannels; i++) {
            GVCID_t vcid = {id, i};
            m_subChannels.at(i) = VirtualChannel(params.subChannels[i], transferFrameSize, vcid);
        }
    }

    MasterChannel(const MasterChannel& other)
        : m_params(other.m_params), m_transferFrameSize(other.m_transferFrameSize), id(other.id) {
        for (NATIVE_UINT_TYPE i = 0; i < m_params.numSubChannels; i++) {
            GVCID_t vcid = {id, i};
            m_subChannels.at(i) = VirtualChannel(m_params.subChannels[i], m_transferFrameSize, vcid);
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

    bool transfer() {
        // Specific implementation for VirtualChannel with no underlying services

        TransferOutType masterChannelTransferItems;
        FwQueuePriorityType priority = 0;
        for (NATIVE_UINT_TYPE i = 0; i < m_params.numSubChannels; i++) {
            GVCID_t vcid = {id, i};
            constexpr FwSizeType size = 5;
            U8 buff[size] = {};
            VirtualChannel vc(m_params.subChannels[i], m_transferFrameSize, vcid);

            // This collects frames from various virtual channels and muxes them
            TransferFrame tFrame;
            FwSizeType actualsize;
            Os::Queue::Status qStatus = Os::Queue::Status::OP_OK;
            qStatus = vc.m_q.receive(reinterpret_cast<const U8*>(&tFrame), sizeof(tFrame),
                                     Os::QueueInterface::BlockingType::BLOCKING, actualsize, &priority);
            // If this were to fail we should create a transfer frame as per CCSDS 4.2.4.4
            FW_ASSERT(qStatus == Os::Queue::Status::OP_OK, qStatus);
            VirtualChannelMultiplexing_handler(tFrame, i, masterChannelTransferItems);
        }
        MasterChannelGeneration_handler(masterChannelTransferItems);

        Os::Queue::Status qStatus = Os::Queue::Status::OP_OK;
        qStatus = m_externalQueue.send(reinterpret_cast<const U8*>(masterChannelTransferItems),
                                       masterChannelTransferItems.size(), priority,
                                       Os::QueueInterface::BlockingType::BLOCKING);
        FW_ASSERT(qStatus == Os::Queue::Status::OP_OK, qStatus);
    }

  private:
    // NOTE should be const
    std::array<VirtualChannel, MAX_VIRTUAL_CHANNELS> m_subChannels{};
    FwSizeType const m_transferFrameSize;
    MasterChannelParams_t& m_params;
    U8 m_channelCount = 0;

    bool VirtualChannelMultiplexing_handler(TransferInType& virtualChannelOut,
                                            NATIVE_UINT_TYPE vcid,
                                            TransferOutType& masterChannelTransfer) {
        masterChannelTransfer.at(vcid) = virtualChannelOut;
    }
    bool MasterChannelGeneration_handler(TransferOutType& masterChannelTransfer) {
        for (NATIVE_UINT_TYPE i = 0; i < masterChannelTransfer.size(); i++) {
            TransferInType inItem = masterChannelTransfer.at(i);
            PrimaryHeader primHeader = inItem.getPrimaryHeader();
            primHeader.setMasterChannelCount(m_channelCount++);
        }
        return true;
    };
};

// Physical Channel Implementation
template <class Service>
class PhysicalChannelSender : public BaseChannel<Service> {
  public:
    PhysicalChannelSender(PhysicalChannelParams_t const& params, Fw::String id)
        : BaseChannel<Service>(params.transferFrameSize), m_subChannels(createSubChannels(params)), id(id) {}

    PhysicalChannelSender(const PhysicalChannelSender& other)
        : BaseChannel<Service>(other), m_subChannels(other.m_subChannels), id(other.id) {}

    PhysicalChannelSender& operator=(const PhysicalChannelSender& other) {
        if (this != &other) {
            BaseChannel<Service>::operator=(other);
            m_subChannels = other.m_subChannels;
            id = other.id;
        }
        return *this;
    }
    const Fw::String id;

    bool propogate(const Fw::Buffer& sdu) override;

  private:
    bool MasterChannelMultiplexing_handler(const std::array<Fw::Buffer, 8>& mcFrames);
    bool AllFramesGeneration_handler(Fw::Buffer& frame);

    static std::array<MasterChannelSender<Service>, MAX_MASTER_CHANNELS> createSubChannels(
        const PhysicalChannelParams_t& params) {
        std::array<MasterChannelSender<Service>, MAX_MASTER_CHANNELS> channels;
        for (NATIVE_UINT_TYPE i = 0; i < params.numSubChannels; i++) {
            MCID_t mcid = {params.subChannels.at(i).spaceCraftId, params.transferFrameVersion};
            channels.at(i) = MasterChannelSender<Service>(params.subChannels.at(i), params.transferFrameSize, mcid);
        }
        return channels;
    }

    std::array<MasterChannelSender<Service>, MAX_MASTER_CHANNELS> m_subChannels;
};

}  // namespace TMSpaceDataLink

#endif  // SVC_TM_SPACE_DATA_LINK_CHANNELS_HPP

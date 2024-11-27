#include "Channels.hpp"
#include "FpConfig.h"
#include "Fw/Com/ComBuffer.hpp"
#include "Fw/Logger/Logger.hpp"
#include "Fw/Types/Assert.hpp"
#include "Fw/Types/String.hpp"
#include "Os/Models/QueueBlockingTypeEnumAc.hpp"
#include "Os/Models/QueueStatusEnumAc.hpp"
#include "Os/Queue.hpp"
#include "Services.hpp"
#include "Svc/FramingProtocol/CCSDSProtocols/TMSpaceDataLink/ManagedParameters.hpp"
#include "Svc/FramingProtocol/CCSDSProtocols/TMSpaceDataLink/TransferFrame.hpp"

namespace TMSpaceDataLink {

template <>
BaseVirtualChannel<VCAService,
                   VCAService::RequestPrimitive_t,
                   FrameService,
                   Fw::ComBuffer,
                   TransferFrame<255>,
                   GVCID_t>::BaseVirtualChannel(GVCID_t id)
    : VCAService(id), FrameService(id), id(id), m_externalQueue() {
    Os::Queue::Status status;
    // NOTE add id to this
    Fw::String name = "Base Channel";
    status = m_externalQueue.create(name, CHANNEL_Q_DEPTH);
    Fw::Logger::log("Created VC for Id %d %d %d \n", id.MCID.TFVN, id.MCID.SCID, id.VCID);
    FW_ASSERT(status == Os::Queue::Status::OP_OK, status);
}

template <>
BaseVirtualChannel<VCAService,
                   VCAService::RequestPrimitive_t,
                   FrameService,
                   Fw::ComBuffer,
                   TransferFrame<255>,
                   GVCID_t>::BaseVirtualChannel(const BaseVirtualChannel& other)
    : VCAService(other.id)  // Initialize base classes
      ,
      FrameService(other.id),
      id(other.id),
      m_externalQueue() {  // Create a new queue for the copied object

    // Since we can't copy if there's a message in the queue we should assert
    FW_ASSERT(!other.m_externalQueue.getMessagesAvailable(), other.m_externalQueue.getMessagesAvailable());

    // Create a new queue for this instance
    Os::Queue::Status status;
    Fw::String name = "Base Channel";
    status = m_externalQueue.create(name, CHANNEL_Q_DEPTH);
    Fw::Logger::log("Created VC (copy) for Id %d %d %d \n", id.MCID.TFVN, id.MCID.SCID, id.VCID);
    FW_ASSERT(status == Os::Queue::Status::OP_OK, status);

    // Copy other member variables
    m_channelTransferCount = other.m_channelTransferCount;
}

template <>
BaseVirtualChannel<VCAService,
                   VCAService::RequestPrimitive_t,
                   FrameService,
                   Fw::ComBuffer,
                   TransferFrame<255>,
                   GVCID_t>&
BaseVirtualChannel<VCAService,
                   VCAService::RequestPrimitive_t,
                   FrameService,
                   Fw::ComBuffer,
                   TransferFrame<255>,
                   GVCID_t>::operator=(const BaseVirtualChannel& other) {
    if (this == &other) {
        return *this;
    }

    // Since we can't copy if there's a message in the queue we should assert
    FW_ASSERT(!other.m_externalQueue.getMessagesAvailable(), other.m_externalQueue.getMessagesAvailable());

    id = other.id;
    m_channelTransferCount = other.m_channelTransferCount;

    return *this;
}

bool VirtualChannel::transfer(TransferInType& transferBuffer) {
    VCA_SDU_t vcaSDU;
    VCAService::RequestPrimitive_t vcaRequest;

    bool status = false;

    status = generateVCAPrimitive(transferBuffer, vcaRequest);
    FW_ASSERT(status, status);

    TransferOutType frame;
    status = ChannelGeneration_handler(vcaRequest, frame);
    FW_ASSERT(status, status);

    FwQueuePriorityType priority = 0;
    Os::Queue::Status qStatus = Os::Queue::Status::OP_OK;
    qStatus = this->m_externalQueue.send(frame, Os::QueueInterface::BlockingType::BLOCKING, priority);
    return qStatus == Os::Queue::Status::OP_OK;
}

bool VirtualChannel::ChannelGeneration_handler(VCAService::RequestPrimitive_t const& request,
                                               TransferOutType& channelOut) {
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
    primaryHeader.setVirtualChannelCount(this->m_channelTransferCount++);
    TransferOutType frame(primaryHeader, dataField);
    // FIXME this I think is bad memory management
    channelOut = frame;

    FW_ASSERT(status, status);
    return true;
}

template <>
BaseMasterChannel<VirtualChannel,
                  typename VirtualChannel::TransferOutType,
                  typename VirtualChannel::TransferOutType,
                  MCID_t,
                  MAX_VIRTUAL_CHANNELS>::BaseMasterChannel(ChannelList<VirtualChannel, MAX_VIRTUAL_CHANNELS> const&
                                                               subChannels,
                                                           MCID_t id)
    : id(id), m_subChannels(subChannels) {
    Os::Queue::Status status;
    Fw::String name = "Master Channel";
    // NOTE this is very wrong at the moment
    status = m_externalQueue.create(name, CHANNEL_Q_DEPTH);
    FW_ASSERT(status == Os::Queue::Status::OP_OK, status);
}

template <>
BaseMasterChannel<VirtualChannel,
                  typename VirtualChannel::TransferOutType,
                  // For now we're just passing along the virtual channels
                  typename VirtualChannel::TransferOutType,
                  MCID_t,
                  MAX_VIRTUAL_CHANNELS>::BaseMasterChannel(const BaseMasterChannel& other)
    : id(other.id), m_subChannels(other.m_subChannels) {}

bool MasterChannel::getChannel(GVCID_t const gvcid, VirtualChannel& vc) {
    FW_ASSERT(gvcid.MCID == id);
    NATIVE_UINT_TYPE i = 0;
    do {
        Fw::Logger::log("%d Looking for Id %d %d %d -> %d %d %d\n", i, gvcid.MCID.TFVN, gvcid.MCID.SCID, gvcid.VCID,
                        m_subChannels.at(i).id.MCID.TFVN, m_subChannels.at(i).id.MCID.SCID,
                        m_subChannels.at(i).id.VCID);
        if (m_subChannels.at(i).id == gvcid) {
            vc = m_subChannels.at(i);
            return true;
        }
    } while (i++ < m_subChannels.size());
    return false;
}

bool MasterChannel::transfer() {
    // Specific implementation for VirtualChannel with no underlying services
    // TransferOutType masterChannelTransferItems;
    std::array<TransferInType, MAX_VIRTUAL_CHANNELS> masterChannelTransferItems;
    FwQueuePriorityType priority = 0;
    for (NATIVE_UINT_TYPE i = 0; i < m_subChannels.size(); i++) {
        // This collects frames from various virtual channels and muxes them
        TransferInType frame;
        FwSizeType actualsize;
        Os::Queue::Status qStatus = Os::Queue::Status::OP_OK;
        qStatus =
            m_subChannels.at(i).m_externalQueue.receive(frame, Os::QueueInterface::BlockingType::BLOCKING, priority);
        // If this were to fail we should create an only idle data (OID) transfer frame as per CCSDS 4.2.4.4
        FW_ASSERT(qStatus == Os::Queue::Status::OP_OK, qStatus);
        // TODO do some sort of id matching
        VirtualChannelMultiplexing_handler(frame, i, masterChannelTransferItems);
    }
    MasterChannelGeneration_handler(masterChannelTransferItems);

    Os::Queue::Status qStatus = Os::Queue::Status::OP_OK;
    for (NATIVE_UINT_TYPE i = 0; i < m_subChannels.size(); i++) {
        qStatus = m_externalQueue.send(masterChannelTransferItems.at(i), Os::QueueInterface::BlockingType::BLOCKING,
                                       this->priority);
        FW_ASSERT(qStatus == Os::Queue::Status::OP_OK, qStatus);
    }

    return true;
}

bool MasterChannel::VirtualChannelMultiplexing_handler(TransferInType& virtualChannelOut,
                                                       NATIVE_UINT_TYPE vcid,
                                                       InternalTransferOutType& masterChannelTransfer) {
    // Trivial for now
    masterChannelTransfer.at(vcid) = virtualChannelOut;
    // TODO implement 4.2.4.4 here
    return true;
}

bool MasterChannel::MasterChannelGeneration_handler(InternalTransferOutType& masterChannelTransfer) {
    for (NATIVE_UINT_TYPE i = 0; i < masterChannelTransfer.size(); i++) {
        TransferInType inItem = masterChannelTransfer.at(i);
        PrimaryHeader primHeader = inItem.getPrimaryHeader();
        primHeader.setMasterChannelCount(m_channelTransferCount++);
    }
    return true;
};

template <>
BaseMasterChannel<MasterChannel,
                  typename MasterChannel::TransferOutType,
                  std::array<typename MasterChannel::TransferOutType, 10>,
                  Fw::String,
                  MAX_MASTER_CHANNELS>::BaseMasterChannel(ChannelList<MasterChannel, MAX_MASTER_CHANNELS> const&
                                                              subChannels,
                                                          Fw::String id)
    : id(id), m_subChannels(subChannels) {
    Os::Queue::Status status;
    // NOTE this is very wrong at the moment
    m_externalQueue.create(id, CHANNEL_Q_DEPTH);
    FW_ASSERT(status == Os::Queue::Status::OP_OK, status);
}

bool PhysicalChannel::getChannel(MCID_t const mcid, VirtualChannel& vc) {
    for (NATIVE_UINT_TYPE i = 0; i < m_subChannels.size(); i++) {
        if (m_subChannels.at(i).id == mcid) {
            return true;
        }
    }
    return false;
}

bool PhysicalChannel::getChannel(GVCID_t const gvcid, VirtualChannel& vc) {
    for (NATIVE_UINT_TYPE i = 0; i < m_subChannels.size(); i++) {
        if (m_subChannels.at(i).getChannel(gvcid, vc)) {
            return true;
        }
    }
    return false;
}

bool PhysicalChannel::transfer() {
    // Specific implementation for VirtualChannel with no underlying services
    TransferOutType masterChannelTransferItems;
    FwQueuePriorityType priority = 0;
    for (NATIVE_UINT_TYPE i = 0; i < m_subChannels.size(); i++) {
        // This collects frames from various virtual channels and muxes them
        TransferInType frame;
        Os::Queue::Status qStatus = Os::Queue::Status::OP_OK;
        qStatus =
            m_subChannels.at(i).m_externalQueue.receive(frame, Os::QueueInterface::BlockingType::BLOCKING, priority);
        // If this were to fail we should create a transfer frame as per CCSDS 4.2.4.4
        FW_ASSERT(qStatus == Os::Queue::Status::OP_OK, qStatus);
        MasterChannelMultiplexing_handler(frame, i, masterChannelTransferItems);
    }
    AllFramesChannelGeneration_handler(masterChannelTransferItems);

    Os::Queue::Status qStatus = Os::Queue::Status::OP_OK;
    for (NATIVE_UINT_TYPE i = 0; i < masterChannelTransferItems.size(); i++) {
        qStatus = m_externalQueue.send(masterChannelTransferItems.at(i), Os::QueueInterface::BlockingType::BLOCKING,
                                       priority);
        FW_ASSERT(qStatus == Os::Queue::Status::OP_OK, qStatus);
    }

    return true;
}

bool PhysicalChannel::MasterChannelMultiplexing_handler(TransferInType& masterChannelOut,
                                                        NATIVE_UINT_TYPE mcid,
                                                        TransferOutType& physicalChannelOut) {
    // Trivial for now
    physicalChannelOut.at(mcid) = masterChannelOut;
    // TODO implement 4.2.6.4 here
    // Generate an idle frame with appropriate First Header Pointer
    // void generateIdleData(Fw::Buffer& frame) = 0;
    return true;
}

bool PhysicalChannel::AllFramesChannelGeneration_handler(TransferOutType& masterChannelTransfer) {
    for (NATIVE_UINT_TYPE i = 0; i < masterChannelTransfer.size(); i++) {
        TransferInType inItem = masterChannelTransfer.at(i);
        // inItem.setFrameErrorControlField();
    }
    return true;
}

}  // namespace TMSpaceDataLink

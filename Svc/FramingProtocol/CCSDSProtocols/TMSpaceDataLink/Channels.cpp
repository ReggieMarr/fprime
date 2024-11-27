#include "Channels.hpp"
#include "FpConfig.h"
#include "Fw/Com/ComBuffer.hpp"
#include "Fw/Types/String.hpp"
#include "Os/Models/QueueBlockingTypeEnumAc.hpp"
#include "Os/Models/QueueStatusEnumAc.hpp"
#include "Os/Queue.hpp"
#include "Services.hpp"
#include "Svc/FramingProtocol/CCSDSProtocols/TMSpaceDataLink/ManagedParameters.hpp"
#include "Svc/FramingProtocol/CCSDSProtocols/TMSpaceDataLink/TransferFrame.hpp"

namespace TMSpaceDataLink {

template <>
BaseVirtualChannel<VirtualChannelParams_t, VCAService, VCAService::RequestPrimitive_t, FrameService, Fw::ComBuffer, TransferFrame<255>, GVCID_t>::
    BaseVirtualChannel(VirtualChannelParams_t const& params, GVCID_t id)
    : VCAService(id), FrameService(id), id(id), m_externalQueue() {
    Os::Queue::Status status;
    // NOTE add id to this
    Fw::String name = "Base Channel";
    m_externalQueue.create(name, CHANNEL_Q_DEPTH);
    FW_ASSERT(status == Os::Queue::Status::OP_OK, status);
}

template <>
BaseVirtualChannel<VirtualChannelParams_t, VCAService, VCAService::RequestPrimitive_t, FrameService, Fw::ComBuffer, TransferFrame<255>, GVCID_t>&
BaseVirtualChannel<VirtualChannelParams_t, VCAService, VCAService::RequestPrimitive_t, FrameService, Fw::ComBuffer, TransferFrame<255>, GVCID_t>::
operator=(const BaseVirtualChannel& other) {
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

bool VirtualChannel::ChannelGeneration_handler(VCAService::RequestPrimitive_t const& request, TransferOutType& channelOut) {
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
BaseMasterChannel<MasterChannelParams_t,
                  VirtualChannel,
                  typename VirtualChannel::TransferOutType,
                  typename VirtualChannel::TransferOutType,
                  MCID_t>::BaseMasterChannel(MasterChannelParams_t& params, MCID_t id)
    : id(id), m_params(params) {
    Os::Queue::Status status;
    Fw::String name = "Master Channel";
    // NOTE this is very wrong at the moment
    m_externalQueue.create(name, CHANNEL_Q_DEPTH);
    FW_ASSERT(status == Os::Queue::Status::OP_OK, status);

    for (U8 i = 0; i < params.numSubChannels; i++) {
        GVCID_t vcid = {id, i};
        m_subChannels.at(i) = VirtualChannel(params.subChannels[i], vcid);
    }
}

template <>
BaseMasterChannel<MasterChannelParams_t,
                  VirtualChannel,
                  typename VirtualChannel::TransferOutType,
                  typename VirtualChannel::TransferOutType,
                  MCID_t>::BaseMasterChannel(const BaseMasterChannel& other)
    : m_params(other.m_params), id(other.id) {
    for (U8 i = 0; i < m_params.numSubChannels; i++) {
        GVCID_t vcid = {id, i};
        m_subChannels.at(i) = VirtualChannel(m_params.subChannels[i], vcid);
    }
}

template <>
BaseMasterChannel<MasterChannelParams_t,
                  VirtualChannel,
                  typename VirtualChannel::TransferOutType,
                  typename VirtualChannel::TransferOutType,
                  MCID_t>&
BaseMasterChannel<MasterChannelParams_t,
                  VirtualChannel,
                  typename VirtualChannel::TransferOutType,
                  typename VirtualChannel::TransferOutType,
                  MCID_t>::operator=(const BaseMasterChannel& other) {
    if (this == &other) {
        return *this;
    }

    // Since we can't copy if there's a message in the queue we should assert
    FW_ASSERT(!other.m_externalQueue.getMessagesAvailable(), other.m_externalQueue.getMessagesAvailable());

    id = other.id;
    m_channelTransferCount = other.m_channelTransferCount;

    return *this;
}

bool MasterChannel::getChannel(GVCID_t const gvcid, VirtualChannel& vc) {
    FW_ASSERT(gvcid.MCID == id);
    NATIVE_UINT_TYPE i = 0;
    do {
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
    for (NATIVE_UINT_TYPE i = 0; i < m_params.numSubChannels; i++) {
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
    for (NATIVE_UINT_TYPE i = 0; i < m_params.numSubChannels; i++) {
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
BaseMasterChannel<PhysicalChannelParams_t,
                  MasterChannel,
                  typename MasterChannel::TransferOutType,
                  std::array<typename MasterChannel::TransferOutType, 10>,
                  Fw::String>::BaseMasterChannel(PhysicalChannelParams_t& params)
    : id(params.channelName), m_params(params) {
    Os::Queue::Status status;
    Fw::String name = "Master Channel";
    // NOTE this is very wrong at the moment
    m_externalQueue.create(name, CHANNEL_Q_DEPTH);
    FW_ASSERT(status == Os::Queue::Status::OP_OK, status);

    for (U8 i = 0; i < params.numSubChannels; i++) {
        MCID_t mcid = {m_params.subChannels.at(i).spaceCraftId, m_params.transferFrameVersion};
        m_subChannels.at(i) = MasterChannel(params.subChannels[i], mcid);
    }
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
    for (NATIVE_UINT_TYPE i = 0; i < m_params.numSubChannels; i++) {
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

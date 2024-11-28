#include "Channels.hpp"
#include <memory>
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
                   TransferFrame<TRANSFER_FRAME_SIZE>,
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
                   TransferFrame<TRANSFER_FRAME_SIZE>,
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
                   TransferFrame<TRANSFER_FRAME_SIZE>,
                   GVCID_t>&
BaseVirtualChannel<VCAService,
                   VCAService::RequestPrimitive_t,
                   FrameService,
                   Fw::ComBuffer,
                   TransferFrame<TRANSFER_FRAME_SIZE>,
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
    VCA_SDU_t vcaSDU(m_userDataTemporaryBuff.data(), m_userDataTemporaryBuff.size());
    VCAService::RequestPrimitive_t vcaRequest;

    bool status = false;

    status = generateVCAPrimitive(transferBuffer, vcaRequest);
    FW_ASSERT(status, status);

    TransferOutType frame(vcaSDU);
    status = ChannelGeneration_handler(vcaRequest, frame);
    FW_ASSERT(status, status);

    FwQueuePriorityType priority = 0;
    Os::Queue::Status qStatus = Os::Queue::Status::OP_OK;
    qStatus = this->m_externalQueue.send(frame, Os::QueueInterface::BlockingType::BLOCKING, priority);
    return qStatus == Os::Queue::Status::OP_OK;
}

bool VirtualChannel::ChannelGeneration_handler(VCAService::RequestPrimitive_t& request, TransferOutType& channelOut) {
    bool status = true;
    // Was something like this originally
    // FrameSDU_t framePrim;
    // status = sender->generateFramePrimitive(request, framePrim);
    DataField<TransferOutType::DATA_FIELD_SIZE> dataField(request.sdu);
    // TODO the ManagedParameters should propogate to this
    MissionPhaseParameters_t missionParams = {
        .transferFrameVersion = 0x00,
        .spaceCraftId = CCSDS_SCID,
        .hasOperationalControlFlag = false,
        .hasSecondaryHeader = false,
        // Indicates this is for VCA
        .isSyncFlagEnabled = true,
    };
    DataFieldStatus_t dataFieldStatus = {
        // Indicates this is for VCA
        .isSyncFlagEnabled = true,
    };
    TransferData_t transferData;
    // NOTE could just as easily get this from the id but this matches the spec better
    transferData.virtualChannelId = request.sap.VCID;
    // transferData.virtualChannelId = id.VCID;
    // Explicitly do not set this
    // transferData.masterChannelFrameCount
    transferData.virtualChannelFrameCount = m_channelTransferCount++;
    // NOTE since we don't have the OCF or FSH services defined we don't
    // do anything to the secondary header or operational control frame
    PrimaryHeader primaryHeader(missionParams, transferData);
    primaryHeader.setVirtualChannelCount(this->m_channelTransferCount++);
    // NOTE this should be done by the "FramingService" to most closely adhere to the spec
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

VirtualChannel& MasterChannel::getChannel(GVCID_t const gvcid) {
    FW_ASSERT(gvcid.MCID == id);
    NATIVE_UINT_TYPE i = 0;
    do {
        Fw::Logger::log("%d Looking for Id %d %d %d -> %d %d %d\n", i, gvcid.MCID.TFVN, gvcid.MCID.SCID, gvcid.VCID,
                        m_subChannels.at(i).id.MCID.TFVN, m_subChannels.at(i).id.MCID.SCID,
                        m_subChannels.at(i).id.VCID);
        if (m_subChannels.at(i).id == gvcid) {
            return m_subChannels.at(i);
        }
    } while (i++ < m_subChannels.size());
    FW_ASSERT(0, gvcid.MCID.SCID, gvcid.MCID.TFVN, gvcid.VCID);
    // FIXME this is here so the compiler won't complain,
    // the above line should keep this from ever happenening tho
    return m_subChannels.at(0);
}

bool MasterChannel::transfer() {
    // Specific implementation for VirtualChannel with no underlying services
    // TransferOutType masterChannelTransferItems;

    // std::array<TransferInType, MAX_VIRTUAL_CHANNELS> masterChannelTransferItems;
    // FwQueuePriorityType priority = 0;
    // for (NATIVE_UINT_TYPE i = 0; i < m_subChannels.size(); i++) {
    //     // This collects frames from various virtual channels and muxes them
    //     // allocate a local buff on the stack
    //     std::array<U8, TransferInType::SERIALIZED_SIZE> frameFieldBytes;
    //     Fw::Buffer frameFieldBuff(frameFieldBytes.data(), frameFieldBytes.size());
    //     TransferInType frame(frameFieldBuff);
    //     FwSizeType actualsize;
    //     Os::Queue::Status qStatus = Os::Queue::Status::OP_OK;
    //     qStatus =
    //         m_subChannels.at(i).m_externalQueue.receive(frame, Os::QueueInterface::BlockingType::BLOCKING, priority);
    //     // If this were to fail we should create an only idle data (OID) transfer frame as per CCSDS 4.2.4.4
    //     FW_ASSERT(qStatus == Os::Queue::Status::OP_OK, qStatus);
    //     // TODO do some sort of id matching
    //     VirtualChannelMultiplexing_handler(frame, i, masterChannelTransferItems);
    // }
    // MasterChannelGeneration_handler(masterChannelTransferItems);

    // Os::Queue::Status qStatus = Os::Queue::Status::OP_OK;
    // for (NATIVE_UINT_TYPE i = 0; i < m_subChannels.size(); i++) {
    //     qStatus = m_externalQueue.send(masterChannelTransferItems.at(i), Os::QueueInterface::BlockingType::BLOCKING,
    //                                    this->priority);
    //     FW_ASSERT(qStatus == Os::Queue::Status::OP_OK, qStatus);
    // }

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
                  typename MasterChannel::TransferOutType,
                  Fw::String,
                  MAX_MASTER_CHANNELS>::BaseMasterChannel(ChannelList<MasterChannel, MAX_MASTER_CHANNELS> const&
                                                              subChannels,
                                                          Fw::String id)
    : id(id), m_subChannels(subChannels) {
    Os::Queue::Status status;
    // NOTE this is very wrong at the moment
    status = m_externalQueue.create(id, CHANNEL_Q_DEPTH);
    FW_ASSERT(status == Os::Queue::Status::OP_OK, status);
}

bool PhysicalChannel::getChannel(MCID_t const mcid, NATIVE_UINT_TYPE& channelIdx) {
    for (channelIdx = 0; channelIdx < m_subChannels.size(); channelIdx++) {
        if (m_subChannels.at(channelIdx).id == mcid) {
            return true;
        }
    }
    return false;
}

VirtualChannel& PhysicalChannel::getChannel(GVCID_t const gvcid) {
    NATIVE_UINT_TYPE i = 0;
    NATIVE_UINT_TYPE mcIdx = 0;
    // First get the Master Channel index, then use that to find
    // the Virtual Channel
    for (mcIdx = 0; mcIdx < m_subChannels.size(); i++) {
        if (getChannel(gvcid.MCID, mcIdx)) {
            break;
        }
    }
    return m_subChannels.at(mcIdx).getChannel(gvcid);
}

bool PhysicalChannel::transfer() {
    // Specific implementation for VirtualChannel with no underlying services
    // std::array<U8, TransferInType::SERIALIZED_SIZE> frameFieldBytes_1;
    // Fw::Buffer frameFieldBuff_1(frameFieldBytes_1.data(), frameFieldBytes_1.size());
    // std::array<U8, TransferInType::SERIALIZED_SIZE> frameFieldBytes_2;
    // Fw::Buffer frameFieldBuff_2(frameFieldBytes_2.data(), frameFieldBytes_2.size());
    // std::array<U8, TransferInType::SERIALIZED_SIZE> frameFieldBytes_3;
    // Fw::Buffer frameFieldBuff_3(frameFieldBytes_3.data(), frameFieldBytes_3.size());

    // TransferOutType masterChannelTransferItems(
    //     {TransferInType(frameFieldBuff_1), TransferInType(frameFieldBuff_2), TransferInType(frameFieldBuff_3)});
    // FwQueuePriorityType priority = 0;
    // for (NATIVE_UINT_TYPE i = 0; i < m_subChannels.size(); i++) {
    //     // This collects frames from various virtual channels and muxes them
    //     std::array<U8, TransferInType::SERIALIZED_SIZE> inFrameFieldBytes;
    //     Fw::Buffer inFrameFieldBuff(inFrameFieldBytes.data(), inFrameFieldBytes.size());
    //     TransferInType frame(inFrameFieldBuff);
    //     Os::Queue::Status qStatus = Os::Queue::Status::OP_OK;
    //     qStatus =
    //         m_subChannels.at(i).m_externalQueue.receive(frame, Os::QueueInterface::BlockingType::BLOCKING, priority);
    //     // If this were to fail we should create a transfer frame as per CCSDS 4.2.4.4
    //     FW_ASSERT(qStatus == Os::Queue::Status::OP_OK, qStatus);
    //     MasterChannelMultiplexing_handler(frame, i, masterChannelTransferItems);
    // }
    // AllFramesChannelGeneration_handler(masterChannelTransferItems);

    // Os::Queue::Status qStatus = Os::Queue::Status::OP_OK;
    // for (NATIVE_UINT_TYPE i = 0; i < masterChannelTransferItems.size(); i++) {
    //     qStatus = m_externalQueue.send(masterChannelTransferItems.at(i), Os::QueueInterface::BlockingType::BLOCKING,
    //                                    priority);
    //     FW_ASSERT(qStatus == Os::Queue::Status::OP_OK, qStatus);
    // }

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
template <>
BaseMasterChannel<MasterChannel,
                  TransferFrame<TRANSFER_FRAME_SIZE>,
                  std::array<TransferFrame<TRANSFER_FRAME_SIZE>, 3>,
                  Fw::String,
                  1>::BaseMasterChannel(const std::array<MasterChannel, 1>& channels, Fw::String id)
    : id(id), m_subChannels(channels), m_externalQueue() {
    Os::Queue::Status status;
    // Create queue with specified name and depth
    status = m_externalQueue.create(id, CHANNEL_Q_DEPTH);
    FW_ASSERT(status == Os::Queue::Status::OP_OK, status);

    // Log creation
    Fw::Logger::log("Created MC with ID %s\n", id.toChar());
}

template <>
BaseMasterChannel<MasterChannel,
                  TransferFrame<TRANSFER_FRAME_SIZE>,
                  std::array<TransferFrame<TRANSFER_FRAME_SIZE>, 3>,
                  Fw::String,
                  1>::BaseMasterChannel(const BaseMasterChannel& other)
    : id(other.id), m_subChannels(other.m_subChannels), m_externalQueue() {
    // Since we can't copy if there's a message in the queue we should assert
    FW_ASSERT(!other.m_externalQueue.getMessagesAvailable(), other.m_externalQueue.getMessagesAvailable());

    // Create a new queue for this instance
    Os::Queue::Status status;
    status = m_externalQueue.create(id, CHANNEL_Q_DEPTH);
    FW_ASSERT(status == Os::Queue::Status::OP_OK, status);

    // Copy other member variables
    m_channelTransferCount = other.m_channelTransferCount;
    priority = other.priority;
}

template <>
BaseMasterChannel<MasterChannel,
                  TransferFrame<TRANSFER_FRAME_SIZE>,
                  std::array<TransferFrame<TRANSFER_FRAME_SIZE>, 3>,
                  Fw::String,
                  1>&
BaseMasterChannel<MasterChannel,
                  TransferFrame<TRANSFER_FRAME_SIZE>,
                  std::array<TransferFrame<TRANSFER_FRAME_SIZE>, 3>,
                  Fw::String,
                  1>::operator=(const BaseMasterChannel& other) {
    if (this == &other) {
        return *this;
    }

    // Since we can't copy if there's a message in the queue we should assert
    FW_ASSERT(!other.m_externalQueue.getMessagesAvailable(), other.m_externalQueue.getMessagesAvailable());

    id = other.id;
    m_subChannels = other.m_subChannels;
    m_channelTransferCount = other.m_channelTransferCount;
    priority = other.priority;

    return *this;
}

}  // namespace TMSpaceDataLink

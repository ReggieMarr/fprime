#include "Channels.hpp"
#include <memory>
#include "FpConfig.h"
#include "Fw/Com/ComBuffer.hpp"
#include "Fw/Logger/Logger.hpp"
#include "Fw/Types/Assert.hpp"
#include "Fw/Types/Serializable.hpp"
#include "Fw/Types/String.hpp"
#include "Os/Models/QueueBlockingTypeEnumAc.hpp"
#include "Os/Models/QueueStatusEnumAc.hpp"
#include "Os/Queue.hpp"
#include "Services.hpp"
#include "Svc/FramingProtocol/CCSDSProtocols/TMSpaceDataLink/ManagedParameters.hpp"
#include "Svc/FramingProtocol/CCSDSProtocols/TMSpaceDataLink/TransferFrame.hpp"
#include "Svc/FramingProtocol/CCSDSProtocols/TMSpaceDataLink/TransferFrameDefs.hpp"

namespace TMSpaceDataLink {
// Default Base Class Transfer Function
template <typename ChannelTemplateConfig>
ChannelBase<ChannelTemplateConfig>::ChannelBase(Id_t const& id) : id(id), m_externalQueue() {}

template <typename ChannelTemplateConfig>
ChannelBase<ChannelTemplateConfig>::ChannelBase(const ChannelBase& other) : id(other.id), m_externalQueue() {
    // Since we can't copy if there's a message in the queue we should assert
    FW_ASSERT(!other.m_externalQueue.getMessagesAvailable(), other.m_externalQueue.getMessagesAvailable());

    // Create a new queue for this instance
    Os::Queue::Status status;
    // TODO name the channel based on the id
    Fw::String name = "Base Channel";
    status =
        m_externalQueue.create(name, CHANNEL_Q_DEPTH, static_cast<FwSizeType>(FPrimeTransferFrame::SERIALIZED_SIZE));
    //
    // Fw::Logger::log("Created VC (copy) for Id %d %d %d \n", id.MCID.TFVN, id.MCID.SCID, id.VCID);
    FW_ASSERT(status == Os::Queue::Status::OP_OK, status);

    // Copy other member variables
    m_channelTransferCount = other.m_channelTransferCount;
}

template <typename ChannelTemplateConfig>
ChannelBase<ChannelTemplateConfig>& ChannelBase<ChannelTemplateConfig>::operator=(const ChannelBase& other) {
    if (this == &other) {
        return *this;
    }

    // Since we can't copy if there's a message in the queue we should assert
    FW_ASSERT(!other.m_externalQueue.getMessagesAvailable(), other.m_externalQueue.getMessagesAvailable());

    id = other.id;
    m_channelTransferCount = other.m_channelTransferCount;

    return *this;
}

// "Pull" a frame from a queue that contains buffers which can extract them
template <typename ChannelTemplateConfig>
bool ChannelBase<ChannelTemplateConfig>::pullFrame(Queue_t &queue, FPrimeTransferFrame &frame) {
    Os::Queue::Status qStatus;
    bool status;
    // NOTE this is a hugely inefficient way of sending the channel info around.
    // This is just done for now as a convinient mechanism for testing out the architecture.
    // TODO replace with either a standard queue or queued component interfaces + buffer memory management
    serialBuffer.resetDeser();
    serialBuffer.setBuffLen(frame.SERIALIZED_SIZE);

    FwQueuePriorityType currentPriority = m_priority;
    FwSizeType actualSize;
    qStatus = queue.receive(serialBuffer.getBuffAddr(), frame.SERIALIZED_SIZE, m_blockType,
                                            actualSize, currentPriority);
    FW_ASSERT(qStatus == Os::Queue::Status::OP_OK, qStatus);
    FW_ASSERT(actualSize == static_cast<FwSizeType>(FPrimeTransferFrame::SERIALIZED_SIZE), actualSize,
                static_cast<FwSizeType>(FPrimeTransferFrame::SERIALIZED_SIZE));

    status = frame.extract(serialBuffer);
    FW_ASSERT(status);
    return true;
}

template <typename ChannelTemplateConfig>
bool ChannelBase<ChannelTemplateConfig>::pushFrame(Queue_t &queue, FPrimeTransferFrame &frame) {
    Os::Queue::Status qStatus;
    bool status;
    // NOTE this is a hugely inefficient way of sending the channel info around.
    // This is just done for now as a convinient mechanism for testing out the architecture.
    // TODO replace with either a standard queue or queued component interfaces + buffer memory management
    serialBuffer.resetSer();
    serialBuffer.setBuffLen(frame.SERIALIZED_SIZE);
    status = frame.insert(serialBuffer);
    FW_ASSERT(status);

    qStatus = queue.send(serialBuffer.getBuffAddr(), frame.SERIALIZED_SIZE, m_priority, m_blockType);
    FW_ASSERT(qStatus == Os::Queue::Status::OP_OK, qStatus);
    return true;
}

template <typename ChannelTemplateConfig>
ChannelBase<ChannelTemplateConfig>::~ChannelBase() {}

template <typename ChannelTemplateConfig>
bool ChannelBase<ChannelTemplateConfig>::transfer(TransferIn_t& transferIn) {
    bool status = false;
    TransferOut_t transferOut;

    status = receive(transferIn, transferOut);
    FW_ASSERT(status);

    status = generate(transferOut);
    FW_ASSERT(status);

    return status;
}

// NOTE based on what we saw in the header I would have assumed this would give the lsp
// autocomplete enough info to pick up the underlying type of Queue_t but that seems not to be the case
static_assert(std::is_same<typename VirtualChannel::Queue_t, Os::Generic::PriorityQueue>::value,
              "Queue_t type mismatch");

// Constructor for the Virtual Channel Template
VirtualChannel::VirtualChannel(GVCID_t const& id) : Base(id), m_receiveService(id), m_frameService(id) {
    Os::Queue::Status status;
    Fw::String name = "Channel";
    status = this->m_externalQueue.create(name, CHANNEL_Q_DEPTH,
                                          static_cast<FwSizeType>(FPrimeTransferFrame::SERIALIZED_SIZE));
    FW_ASSERT(status == Os::Queue::Status::OP_OK, status);
}

// Destructor for the Virtual Channel Template
VirtualChannel::~VirtualChannel() {}

// Note we could probably just make this a pass through then
bool VirtualChannel::receive(Fw::Buffer& buffer, VCAService::Primitive_t& result) {
    return m_receiveService.generatePrimitive(buffer, result);
}

bool VirtualChannel::generate(VCFUserData_t& arg) {
    FW_ASSERT(arg.sap == this->id);
    VCFRequestPrimitive_t prim;
    bool status;

    status = m_frameService.generatePrimitive(arg, prim);
    FW_ASSERT(status);

    PrimaryHeaderControlInfo_t primaryHeaderCI;
    // Get our primary header parameters that have been set so far.
    prim.frame.primaryHeader.get(primaryHeaderCI);
    // Update the transfer count
    primaryHeaderCI.virtualChannelFrameCount = this->m_channelTransferCount;
    primaryHeaderCI.spacecraftId = this->id.MCID.SCID;
    primaryHeaderCI.transferFrameVersion = this->id.MCID.TFVN;
    primaryHeaderCI.virtualChannelId = this->id.VCID;
    // TODO
    // primaryHeaderCI.dataFieldStatus = arg.statusFields
    prim.frame.primaryHeader.set(primaryHeaderCI);

    status = this->pushFrame(this->m_externalQueue, prim.frame);
    FW_ASSERT(status);

    this->m_channelTransferCount += 1;

    return true;
}

// Virtual Channel instantiations
// Instantiate the parameter config template
template class ChannelParameterConfig<Fw::Buffer,
                                      VCFServiceTemplateParams::Primitive_t,
                                      Os::Generic::PriorityQueue,
                                      VCAServiceTemplateParams::SAP_t>;

// Instantiate the base channel template with your params
template class ChannelBase<VirtualChannelParams>;

// NOTE based on what we saw in the header I would have assumed this would give the lsp
// autocomplete enough info to pick up the underlying type of Queue_t but that seems not to be the case
static_assert(std::is_same<typename SingleMasterChannel::Queue_t, Os::Generic::PriorityQueue>::value,
              "Queue_t type mismatch");

static_assert(std::is_same<MasterChannelSubChannelParams::Channel_t, VirtualChannel>::value, "Channel_t type mismatch");

static_assert(std::is_same<typename SingleMasterChannel::TransferOut_t, std::array<FPrimeTransferFrame, 1> >::value,
              "Channel_t type mismatch");

// Constructor for the Master Channel Template
template <FwSizeType NumSubChannels>
MasterChannel<NumSubChannels>::MasterChannel(Id_t const& id, VirtualChannelList& subChannels)
    : Base(id), m_subChannels(subChannels) {
    Os::Queue::Status status;
    Fw::String name = "Channel";
    status = this->m_externalQueue.create(name, CHANNEL_Q_DEPTH,
                                          static_cast<FwSizeType>(FPrimeTransferFrame::SERIALIZED_SIZE));
    FW_ASSERT(status == Os::Queue::Status::OP_OK, status);
}

// Destructor for the Master Channel Template
template <FwSizeType NumSubChannels>
MasterChannel<NumSubChannels>::~MasterChannel() {}

// First, define the actual function implementations
template <FwSizeType NumSubChannels>
bool MasterChannel<NumSubChannels>::receive(std::nullptr_t& _, TransferOut_t& masterChannelFrames) {
    bool status;
    for (NATIVE_UINT_TYPE vcIdx = 0; vcIdx < m_subChannels.size(); vcIdx++) {
        status = this->pullFrame(m_subChannels.at(vcIdx).m_externalQueue, masterChannelFrames.at(vcIdx));
        FW_ASSERT(status);
    }
    return true;
}

template <FwSizeType NumSubChannels>
bool MasterChannel<NumSubChannels>::generate(TransferOut_t& masterChannelFrames) {
    bool status = true;  // Initialize status
    Os::Queue::Status qStatus;

    for (NATIVE_UINT_TYPE vcIdx = 0; vcIdx < m_subChannels.size(); vcIdx++) {
        PrimaryHeaderControlInfo_t primaryHeaderCI;
        // Get our primary header parameters that have been set so far.
        masterChannelFrames.at(vcIdx).primaryHeader.get(primaryHeaderCI);
        // Update the transfer count
        primaryHeaderCI.masterChannelFrameCount = this->m_channelTransferCount;

        // Propogate the frame
        status = this->pushFrame(m_subChannels.at(vcIdx).m_externalQueue, masterChannelFrames.at(vcIdx));
        FW_ASSERT(status);
    }
    // If we got this far that indicates that the transfer succeeded and we can increment the count
    this->m_channelTransferCount += 1;
    return true;
}

template <FwSizeType NumSubChannels>
VirtualChannel& MasterChannel<NumSubChannels>::getChannel(GVCID_t const gvcid) {
    FW_ASSERT(gvcid.MCID == this->id);
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

// Master Channel instantiations
template class MasterChannel<1>;

// Constructor for the Physical Channel Template
template <FwSizeType NumSubChannels>
PhysicalChannel<NumSubChannels>::PhysicalChannel(Id_t const& id, MasterChannelList& subChannels)
    : Base(id), m_subChannels(subChannels) {
    Os::Queue::Status status;
    status = this->m_externalQueue.create(id, CHANNEL_Q_DEPTH,
                                          static_cast<FwSizeType>(FPrimeTransferFrame::SERIALIZED_SIZE));
    FW_ASSERT(status == Os::Queue::Status::OP_OK, status);
}

// Destructor for the Physical Channel Template
template <FwSizeType NumSubChannels>
PhysicalChannel<NumSubChannels>::~PhysicalChannel() {}

// First, define the actual function implementations
template <FwSizeType NumSubChannels>
bool PhysicalChannel<NumSubChannels>::receive(std::nullptr_t& _, TransferOut_t& masterChannelFrames) {
    bool status;
    // Retrieve the frames from the subChannel queues
    for (NATIVE_UINT_TYPE vcIdx = 0; vcIdx < m_subChannels.size(); vcIdx++) {
        status = this->pullFrame(m_subChannels.at(vcIdx).m_externalQueue, masterChannelFrames.at(vcIdx));
        FW_ASSERT(status);
    }
    return true;
}

template <FwSizeType NumSubChannels>
bool PhysicalChannel<NumSubChannels>::generate(TransferOut_t& masterChannelFrames) {
    bool status = true;  // Initialize status
    Os::Queue::Status qStatus;

    for (NATIVE_UINT_TYPE vcIdx = 0; vcIdx < m_subChannels.size(); vcIdx++) {
        // TODO Check the CRC here
        status = this->pushFrame(m_subChannels.at(vcIdx).m_externalQueue, masterChannelFrames.at(vcIdx));
        FW_ASSERT(status);
    }
    // If we got this far that indicates that the transfer succeeded and we can increment the count
    this->m_channelTransferCount += 1;
    return true;
}

template <FwSizeType NumSubChannels>
void PhysicalChannel<NumSubChannels>::popFrameBuff(Fw::SerializeBufferBase& frameBuff) {
    FwSizeType const transferFrameSize = FPrimeTransferFrame::SERIALIZED_SIZE;
    FW_ASSERT(transferFrameSize <= frameBuff.getBuffLength(), transferFrameSize, frameBuff.getBuffLength());

    Os::Queue::Status qStatus;
    FwQueuePriorityType currentPriority = m_priority;
    FwSizeType actualSize;

    qStatus = this->m_externalQueue.receive(frameBuff.getBuffAddr(), transferFrameSize, m_blockType,
                                            actualSize, currentPriority);
    FW_ASSERT(qStatus == Os::Queue::Status::OP_OK, qStatus);
    FW_ASSERT(actualSize == static_cast<FwSizeType>(FPrimeTransferFrame::SERIALIZED_SIZE), actualSize,
              static_cast<FwSizeType>(FPrimeTransferFrame::SERIALIZED_SIZE));

    FW_ASSERT(currentPriority == m_priority, currentPriority, m_priority);
}

// Physical Channel instantiations
template class PhysicalChannel<1>;

}  // namespace TMSpaceDataLink

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
ChannelBase<ChannelTemplateConfig>::ChannelBase(Id_t const& id) : m_id(id) {}

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
    FW_ASSERT(arg.sap == this->m_id);
    VCFRequestPrimitive_t prim;
    bool status;

    status = m_frameService.generatePrimitive(arg, prim);
    FW_ASSERT(status);

    Os::Queue::Status qStatus;
    FPrimeTransferFrame& frame = prim.frame;
    PrimaryHeaderControlInfo_t primaryHeaderCI;
    // Get our primary header parameters that have been set so far.
    frame.primaryHeader.get(primaryHeaderCI);
    // Update the transfer count
    primaryHeaderCI.virtualChannelFrameCount = this->m_channelTransferCount;
    primaryHeaderCI.spacecraftId = this->m_id.MCID.SCID;
    primaryHeaderCI.transferFrameVersion = this->m_id.MCID.TFVN;
    primaryHeaderCI.virtualChannelId = this->m_id.VCID;
    // TODO
    // primaryHeaderCI.dataFieldStatus = arg.statusFields

    std::array<U8, FPrimeTransferFrame::SERIALIZED_SIZE> m_serBuff;
    Fw::ComBuffer serialBuffer(m_serBuff.data(), m_serBuff.size());
    status = frame.insert(serialBuffer);
    FW_ASSERT(status);

    qStatus =
        this->m_externalQueue.send(serialBuffer.getBuffAddr(), serialBuffer.getBuffLength(), m_priority, m_blockType);
    FW_ASSERT(qStatus == Os::Queue::Status::OP_OK, qStatus);

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
    Os::Queue::Status qStatus;
    bool status;
    for (NATIVE_UINT_TYPE vcIdx = 0; vcIdx < m_subChannels.size(); vcIdx++) {
        VirtualChannel& vc = m_subChannels.at(vcIdx).get();

        std::array<U8, FPrimeTransferFrame::SERIALIZED_SIZE> m_serBuff;
        Fw::ComBuffer serialBuffer(m_serBuff.data(), m_serBuff.size());

        FwQueuePriorityType currentPriority = m_priority;
        FwSizeType actualSize;
        qStatus = vc.m_externalQueue.receive(serialBuffer.getBuffAddr(), serialBuffer.getBuffLength(), m_blockType,
                                             actualSize, currentPriority);
        FW_ASSERT(qStatus == Os::Queue::Status::OP_OK, qStatus);
        FW_ASSERT(actualSize == static_cast<FwSizeType>(FPrimeTransferFrame::SERIALIZED_SIZE), actualSize,
                  static_cast<FwSizeType>(FPrimeTransferFrame::SERIALIZED_SIZE));

        status = masterChannelFrames.at(vcIdx).extract(serialBuffer);
        FW_ASSERT(status);
    }
    return true;
}

template <FwSizeType NumSubChannels>
bool MasterChannel<NumSubChannels>::generate(TransferOut_t& masterChannelFrames) {
    bool status = true;  // Initialize status
    Os::Queue::Status qStatus;

    for (NATIVE_UINT_TYPE vcIdx = 0; vcIdx < m_subChannels.size(); vcIdx++) {
        VirtualChannel& vc = m_subChannels.at(vcIdx).get();
        Os::Generic::PriorityQueue& frameQ = vc.m_externalQueue;  // Use reference

        FPrimeTransferFrame& frame = masterChannelFrames.at(vcIdx);
        PrimaryHeaderControlInfo_t primaryHeaderCI;
        // Get our primary header parameters that have been set so far.
        frame.primaryHeader.get(primaryHeaderCI);
        // Update the transfer count
        primaryHeaderCI.masterChannelFrameCount = this->m_channelTransferCount;
        std::array<U8, FPrimeTransferFrame::SERIALIZED_SIZE> m_serBuff;
        Fw::ComBuffer serialBuffer(m_serBuff.data(), m_serBuff.size());
        status = masterChannelFrames.at(vcIdx).insert(serialBuffer);
        FW_ASSERT(status);

        qStatus = this->m_externalQueue.send(serialBuffer.getBuffAddr(), serialBuffer.getBuffLength(), m_priority,
                                             m_blockType);
        FW_ASSERT(qStatus == Os::Queue::Status::OP_OK, qStatus);
    }
    // If we got this far that indicates that the transfer succeeded and we can increment the count
    this->m_channelTransferCount += 1;
    return true;
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
    Os::Queue::Status qStatus;
    bool status;
    for (NATIVE_UINT_TYPE vcIdx = 0; vcIdx < m_subChannels.size(); vcIdx++) {
        SingleMasterChannel& mc = m_subChannels.at(vcIdx).get();

        std::array<U8, FPrimeTransferFrame::SERIALIZED_SIZE> m_serBuff;
        Fw::ComBuffer serialBuffer(m_serBuff.data(), m_serBuff.size());

        FwQueuePriorityType currentPriority = m_priority;
        FwSizeType actualSize;
        qStatus = mc.m_externalQueue.receive(serialBuffer.getBuffAddr(), serialBuffer.getBuffLength(), m_blockType,
                                             actualSize, currentPriority);
        FW_ASSERT(qStatus == Os::Queue::Status::OP_OK, qStatus);
        FW_ASSERT(actualSize == static_cast<FwSizeType>(FPrimeTransferFrame::SERIALIZED_SIZE), actualSize,
                  static_cast<FwSizeType>(FPrimeTransferFrame::SERIALIZED_SIZE));

        status = masterChannelFrames.at(vcIdx).extract(serialBuffer);
        FW_ASSERT(status);
    }
    return true;
}

template <FwSizeType NumSubChannels>
bool PhysicalChannel<NumSubChannels>::generate(TransferOut_t& masterChannelFrames) {
    bool status = true;  // Initialize status
    Os::Queue::Status qStatus;

    for (NATIVE_UINT_TYPE vcIdx = 0; vcIdx < m_subChannels.size(); vcIdx++) {
        SingleMasterChannel& mc = m_subChannels.at(vcIdx).get();
        Os::Generic::PriorityQueue& frameQ = mc.m_externalQueue;  // Use reference

        // TODO Check the CRC here

        std::array<U8, FPrimeTransferFrame::SERIALIZED_SIZE> m_serBuff;
        Fw::ComBuffer serialBuffer(m_serBuff.data(), m_serBuff.size());
        status = masterChannelFrames.at(vcIdx).insert(serialBuffer);
        FW_ASSERT(status);

        qStatus = this->m_externalQueue.send(serialBuffer.getBuffAddr(), serialBuffer.getBuffLength(), m_priority,
                                             m_blockType);
        FW_ASSERT(qStatus == Os::Queue::Status::OP_OK, qStatus);
    }
    // If we got this far that indicates that the transfer succeeded and we can increment the count
    this->m_channelTransferCount += 1;
    return true;
}

template <FwSizeType NumSubChannels>
void PhysicalChannel<NumSubChannels>::popFrameBuff(Fw::SerializeBufferBase& frameBuff) {
    FPrimeTransferFrame frame;

    Os::Queue::Status qStatus;
    std::array<U8, FPrimeTransferFrame::SERIALIZED_SIZE> m_serBuff;
    Fw::ComBuffer serialBuffer(m_serBuff.data(), m_serBuff.size());

    FwQueuePriorityType currentPriority = m_priority;
    FwSizeType actualSize;
    qStatus = this->m_externalQueue.receive(serialBuffer.getBuffAddr(), serialBuffer.getBuffLength(), m_blockType,
                                            actualSize, currentPriority);
    FW_ASSERT(qStatus == Os::Queue::Status::OP_OK, qStatus);
    FW_ASSERT(actualSize == static_cast<FwSizeType>(FPrimeTransferFrame::SERIALIZED_SIZE), actualSize,
              static_cast<FwSizeType>(FPrimeTransferFrame::SERIALIZED_SIZE));

    bool status = frame.extract(serialBuffer);
    FW_ASSERT(status);

    FW_ASSERT(currentPriority == m_priority, currentPriority, m_priority);

    // Fw::ComBuffer com(frameBuff.getData(), frameBuff.getSize());
    status = frame.insert(frameBuff);
    FW_ASSERT(status);
}

// Physical Channel instantiations
template class PhysicalChannel<1>;

}  // namespace TMSpaceDataLink

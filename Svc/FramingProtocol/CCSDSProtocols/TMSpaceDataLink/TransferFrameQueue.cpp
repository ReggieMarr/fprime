#include "TransferFrameQueue.hpp"
#include <Fw/Types/Assert.hpp>
#include <Os/Generic/PriorityQueue.hpp>
#include <cstring>
#include <new>
#include "Fw/Com/ComBuffer.hpp"
#include "Os/Queue.hpp"

namespace Os {
namespace Generic {

template <FwSizeType TransferFrameSize>
QueueInterface::Status TransformFrameQueue<TransferFrameSize>::create(const Fw::StringBase& name, FwSizeType depth) {
    // Use TransferFrameSize in the template parameter
    return PriorityQueue::create(
        name, depth, static_cast<FwSizeType>(TMSpaceDataLink::TransferFrame<TransferFrameSize>::SERIALIZED_SIZE));
}

template <FwSizeType TransferFrameSize>
QueueInterface::Status TransformFrameQueue<TransferFrameSize>::send(
    TMSpaceDataLink::TransferFrame<TransferFrameSize>& transferFrame,  // Match the template parameter
    BlockingType blockType,
    FwQueuePriorityType priority) {
    // Stack allocation of temporary buffer
    Fw::ComBuffer serialBuffer(m_serializationBuffer.data(), m_serializationBuffer.size());

    // Serialize the transfer frame
    Fw::SerializeStatus serStatus = transferFrame.serialize(serialBuffer);
    if (serStatus != Fw::SerializeStatus::FW_SERIALIZE_OK) {
        return QueueInterface::Status::RECEIVE_ERROR;
    }

    return PriorityQueue::send(serialBuffer.getBuffAddr(),  // Fixed: use buffer address
                               serialBuffer.getBuffLength(), priority, blockType);
}

template <FwSizeType TransferFrameSize>
QueueInterface::Status TransformFrameQueue<TransferFrameSize>::receive(
    TMSpaceDataLink::TransferFrame<TransferFrameSize>& transferFrame,  // Match the template parameter
    BlockingType blockType,
    FwQueuePriorityType& priority) {
    FwSizeType actualSize;

    Status status = PriorityQueue::receive(m_serializationBuffer.data(), m_serializationBuffer.size(), blockType,
                                           actualSize, priority);

    if (status != QueueInterface::Status::OP_OK) {
        return status;
    }

    if (actualSize != TMSpaceDataLink::TransferFrame<TransferFrameSize>::SERIALIZED_SIZE) {
        return QueueInterface::Status::RECEIVE_ERROR;
    }

    // Deserialize the received data
    Fw::ComBuffer serialBuffer(m_serializationBuffer.data(), actualSize);
    Fw::SerializeStatus serStatus = transferFrame.deserialize(serialBuffer);
    if (serStatus != Fw::SerializeStatus::FW_SERIALIZE_OK) {
        return QueueInterface::Status::RECEIVE_ERROR;
    }

    return QueueInterface::Status::OP_OK;
}

template <FwSizeType TransferFrameSize>
TransformFrameQueue<TransferFrameSize>::~TransformFrameQueue() {
    // Clean up any resources
    // If using PriorityQueue's cleanup, call parent destructor
    // Note: If PriorityQueue has virtual destructor, this might not be needed
    // PriorityQueue::cleanup();
}
template <>
TransformFrameQueue<255>::~TransformFrameQueue() {
    // Cleanup implementation
    // PriorityQueue::cleanup();
}

template <>
TransformFrameQueue<255>::TransformFrameQueue()
    : PriorityQueue()
    , m_serializationBuffer() {
}

template <>
QueueInterface::Status TransformFrameQueue<255>::create(const Fw::StringBase& name, FwSizeType depth) {
    return PriorityQueue::create(name, depth,
        static_cast<FwSizeType>(TMSpaceDataLink::TransferFrame<255>::SERIALIZED_SIZE));
}

template <>
QueueInterface::Status TransformFrameQueue<255>::send(
    TMSpaceDataLink::TransferFrame<255>& transferFrame,
    BlockingType blockType,
    FwQueuePriorityType priority) {
    // Implementation as provided earlier
    // ...
}

// Explicit instantiation
template class TransformFrameQueue<255>;

}  // namespace Generic
}  // namespace Os

#include "TransferFrameQueue.hpp"
#include <Fw/Types/Assert.hpp>
#include <Os/Generic/PriorityQueue.hpp>
#include <cstring>
#include <new>
#include "Fw/Com/ComBuffer.hpp"
#include "Os/Queue.hpp"

namespace Os {
namespace Generic {

QueueInterface::Status TransformFrameQueue::create(const Fw::StringBase& name, FwSizeType depth) {
    // Use TransferFrameSize in the template parameter
    return PriorityQueue::create(
        name, depth, static_cast<FwSizeType>(TMSpaceDataLink::FPrimeTransferFrame::SERIALIZED_SIZE));
}

QueueInterface::Status TransformFrameQueue::send(
    TMSpaceDataLink::FPrimeTransferFrame& transferFrame,  // Match the template parameter
    BlockingType blockType,
    FwQueuePriorityType priority) {
    // Stack allocation of temporary buffer
    Fw::ComBuffer serialBuffer(m_serializationBuffer.data(), m_serializationBuffer.size());

    // Serialize the transfer frame
    // bool serStatus = transferFrame.insert(serialBuffer);
    // if (!serStatus) {
    //     return QueueInterface::Status::RECEIVE_ERROR;
    // }

    return PriorityQueue::send(serialBuffer.getBuffAddr(),  // Fixed: use buffer address
                               serialBuffer.getBuffLength(), priority, blockType);
}

QueueInterface::Status TransformFrameQueue::receive(
    TMSpaceDataLink::FPrimeTransferFrame& transferFrame,  // Match the template parameter
    BlockingType blockType,
    FwQueuePriorityType& priority) {
    FwSizeType actualSize;

    Status status = PriorityQueue::receive(m_serializationBuffer.data(), m_serializationBuffer.size(), blockType,
                                           actualSize, priority);

    if (status != QueueInterface::Status::OP_OK) {
        return status;
    }

    if (actualSize != TMSpaceDataLink::FPrimeTransferFrame::SERIALIZED_SIZE) {
        return QueueInterface::Status::RECEIVE_ERROR;
    }

    // Deserialize the received data
    Fw::ComBuffer serialBuffer(m_serializationBuffer.data(), actualSize);
    bool serStatus = transferFrame.extract(serialBuffer);
    if (!serStatus) {
        return QueueInterface::Status::RECEIVE_ERROR;
    }

    return QueueInterface::Status::OP_OK;
}

}  // namespace Generic
}  // namespace Os

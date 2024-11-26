#include "TransferFrameQueue.hpp"
#include <Os/Generic/PriorityQueue.hpp>
#include <Fw/Types/Assert.hpp>
#include <cstring>
#include <new>
#include "Fw/Com/ComBuffer.hpp"
#include "Os/Queue.hpp"

namespace Os {
namespace Generic {
TransformFrameQueue::~TransformFrameQueue() {
    // Base class destructor will handle cleanup
}

QueueInterface::Status TransformFrameQueue::create(const Fw::StringBase& name, FwSizeType depth) {
    // Create the underlying priority queue with the size of TransferFrame
    return PriorityQueue::create(name, depth, static_cast<FwSizeType>(TMSpaceDataLink::TransferFrame<>::SIZE));
}

QueueInterface::Status TransformFrameQueue::send(
    TMSpaceDataLink::TransferFrame<>& transferFrame,
    FwQueuePriorityType priority,
    BlockingType blockType
) {
    // Stack allocation of temporary buffer
    U8 buffer[TMSpaceDataLink::TransferFrame<>::SIZE];
    Fw::ComBuffer serialBuffer(buffer, sizeof(buffer));

    // Serialize the transfer frame
    Fw::SerializeStatus serStatus = transferFrame.serialize(serialBuffer);
    if (serStatus != Fw::SerializeStatus::FW_SERIALIZE_OK) {
      return QueueInterface::Status::RECEIVE_ERROR;
    }

    return PriorityQueue::send(
        buffer,
        serialBuffer.getBuffLength(),
        priority,
        blockType
    );
}

QueueInterface::Status TransformFrameQueue::receive(
    TMSpaceDataLink::TransferFrame<>& transferFrame,
    BlockingType blockType,
    FwQueuePriorityType& priority
) {
    U8 buffer[TMSpaceDataLink::TransferFrame<>::SIZE];
    FwSizeType actualSize;

    Status status = PriorityQueue::receive(
        buffer,
        sizeof(buffer),
        blockType,
        actualSize,
        priority
    );

    if (status != QueueInterface::Status::OP_OK) {
        return status;
    }

    if (actualSize != sizeof(buffer)) {
        return status;
    }

    // Deserialize the received data
    Fw::ComBuffer serialBuffer(buffer, actualSize);
    Fw::SerializeStatus serStatus = transferFrame.deserialize(serialBuffer);
    if (serStatus != Fw::SerializeStatus::FW_SERIALIZE_OK) {
      return QueueInterface::Status::RECEIVE_ERROR;
    }


    return QueueInterface::Status::OP_OK;
}

}  // namespace Generic
}  // namespace Os

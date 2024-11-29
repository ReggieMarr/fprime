#ifndef TM_SPACE_DATA_LINK_TRANSFER_FRAME_QUEUE_H_
#define TM_SPACE_DATA_LINK_TRANSFER_FRAME_QUEUE_H_

#include <Os/Generic/PriorityQueue.hpp>
#include "FpConfig.h"
#include "TransferFrame.hpp"

namespace Os {
namespace Generic {

class TransformFrameQueue : public PriorityQueue {
  private:
    // Pre-allocated buffer for serialization
    std::array<U8, TMSpaceDataLink::FPrimeTransferFrame::SERIALIZED_SIZE> m_serializationBuffer;

  public:
    // FIXME this should be set at compile time and is FPRIMETMFRAME::SERIALIZED_SIZE
    enum {
        SERIALIZED_SIZE = 255 * 10,  //!< Size of DataField when serialized
    };

    //! \brief default queue interface constructor
    TransformFrameQueue() = default;

    //! \brief default queue destructor
    virtual ~TransformFrameQueue() = default;

    //! \brief copy constructor is forbidden
    TransformFrameQueue(const QueueInterface& other) = delete;

    //! \brief copy constructor is forbidden
    TransformFrameQueue(const QueueInterface* other) = delete;

    //! \brief assignment operator is forbidden
    TransformFrameQueue& operator=(const QueueInterface& other) override = delete;

    //! \brief create queue storage
    //!
    //! Creates a queue ensuring sufficient storage to hold `depth` messages of `messageSize` size each.
    //!
    //! \warning allocates memory on the heap
    //!
    //! \param name: name of queue
    //! \param depth: depth of queue in number of messages
    //! \return: status of the creation
    Status create(const Fw::StringBase& name, FwSizeType depth);

    //! \brief send a message into the queue
    //!
    //! Send a TransformFrame into the queue, providing the TransformFrame data, size, priority, and blocking type. When
    //! `blockType` is set to BLOCKING, this call will block on queue full. Otherwise, this will return an error
    //! status on queue full.
    //!
    //! \warning It is invalid to send a null buffer
    //! \warning This method will block if the queue is full and blockType is set to BLOCKING
    //!
    //! \param transformFrame: The transform frame to send.
    //! \param size: size of message data
    //! \param priority: priority of the message
    //! \param blockType: BLOCKING to block for space or NONBLOCKING to return error when queue is full
    //! \return: status of the send
    Status send(TMSpaceDataLink::FPrimeTransferFrame& transformFrame,
                BlockingType blockType,
                FwQueuePriorityType priority);

    //! \brief receive a message from the queue
    //!
    //! Receive a TransferFrame from the queue, providing the TransformFrame destination, priority, and blocking type.
    //! When `blockType` is set to BLOCKING, this call will block on queue empty. Otherwise, this will return an
    //! error status on queue empty. Actual size received and priority of message is set on success status.
    //!
    //! \warning It is invalid to send a null buffer
    //! \warning This method will block if the queue is full and blockType is set to BLOCKING
    //!
    //! \param destination: destination for message data
    //! \param blockType: BLOCKING to wait for message or NONBLOCKING to return error when queue is empty
    //! \param priority: (output) priority of message read
    //! \return: status of the send
    Status receive(TMSpaceDataLink::FPrimeTransferFrame& transformFrame,
                   BlockingType blockType,
                   FwQueuePriorityType& priority);
};
// template<FwSizeType TransferFramerSize>
// void transferQueue<>(Os::Generic::TransformFrameQueue<TransferFramerSize> &src,
//                      Os::Generic::TransformFrameQueue<TransferFramerSize> &dest) {

//         // If we really wanted to we could transfer the queues some way like this
//         Os::QueueInterface::Status other_queue_status = Os::QueueInterface::OP_OK;
//         Os::QueueInterface::Status this_queue_status = Os::QueueInterface::OP_OK;
//         // NOTE should carefully consider the perfomance implications here
//         // also since we're potentially exiting when this instance's queue is full
//         // that could result in dropped frames
//         while (other_queue_status != Os::QueueInterface::Status::EMPTY &&
//                this_queue_status != Os::QueueInterface::Status::FULL) {
//             TransferOutType frame;
//             FwQueuePriorityType priority = 0;
//             other_queue_status = other.m_externalQueue.receive(other, frame,
//             Os::QueueInterface::BlockingType::BLOCKING, priority); FW_ASSERT(other_queue_status !=
//             Os::QueueInterface::Status::EMPTY ||
//                           other_queue_status == Os::QueueInterface::Status::OP_OK,
//                       other_queue_status);
//             this_queue_status = m_externalQueue.send(frame, Os::QueueInterface::BlockingType::BLOCKING, priority);
//             FW_ASSERT(this_queue_status != Os::QueueInterface::Status::FULL ||
//                           this_queue_status == Os::QueueInterface::Status::OP_OK,
//                       this_queue_status);
//         }
//         if (this_queue_status == Os::QueueInterface::Status::FULL) {
//             Fw::Logger::log("[WARNING] Could not transfer entire queue. %d Messages left \n",
//                             other.m_externalQueue.getMessagesAvailable());
//         }
// }

}  // namespace Generic
}  // namespace Os

#endif  // TM_SPACE_DATA_LINK_TRANSFER_FRAME_QUEUE_H_

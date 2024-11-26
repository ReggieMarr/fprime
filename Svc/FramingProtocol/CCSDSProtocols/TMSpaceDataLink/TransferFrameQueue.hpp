#ifndef TM_SPACE_DATA_LINK_TRANSFER_FRAME_QUEUE_H_
#define TM_SPACE_DATA_LINK_TRANSFER_FRAME_QUEUE_H_

#include <Os/Generic/PriorityQueue.hpp>
#include "TransferFrame.hpp"

namespace Os {
namespace Generic {

class TransformFrameQueue : public PriorityQueue {
  public:
    //! \brief default queue interface constructor
    TransformFrameQueue() : m_serialBuffer(m_serializationBuffer, sizeof(m_serializationBuffer)) {}

    //! \brief default queue destructor
    virtual ~TransformFrameQueue();

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
    Status send(TMSpaceDataLink::TransferFrame<>& transformFrame, FwQueuePriorityType priority, BlockingType blockType);

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
    Status receive(TMSpaceDataLink::TransferFrame<>& transformFrame,
                   BlockingType blockType,
                   FwQueuePriorityType& priority);

  private:
    // Pre-allocated buffer for serialization
    U8 m_serializationBuffer[TMSpaceDataLink::TransferFrame<>::SIZE];
    // Fw::SerializeBufferBase m_serialBuffer;
    Fw::ComBuffer m_serialBuffer;
};

}  // namespace Generic
}  // namespace Os

#endif  // TM_SPACE_DATA_LINK_TRANSFER_FRAME_QUEUE_H_

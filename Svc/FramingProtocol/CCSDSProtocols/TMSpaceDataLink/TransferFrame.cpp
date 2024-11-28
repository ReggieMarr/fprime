// ======================================================================
// \title  TMSpaceDataLink TransferFrame.cpp
// \author Reginald Marr
// \brief  Implementation of TMSpaceDataLink TransferFrame
//
// ======================================================================
#include "TransferFrame.hpp"
#include <array>
#include "FpConfig.h"
#include "FpConfig.hpp"
#include "Fw/Logger/Logger.hpp"
#include "Fw/Types/Assert.hpp"
#include "Fw/Types/Serializable.hpp"
#include "ProtocolDataUnits.hpp"
#include "ProtocolInterface.hpp"
#include "Svc/FrameAccumulator/FrameDetector.hpp"
#include "Svc/FramingProtocol/CCSDSProtocols/CCSDSProtocolDefs.hpp"
#include "Svc/FramingProtocol/CCSDSProtocols/TMSpaceDataLink/ProtocolDataUnits.hpp"
#include "Utils/Hash/Hash.hpp"
#include "Utils/Types/CircularBuffer.hpp"

namespace TMSpaceDataLink {

template <FwSizeType FrameSize, FwSizeType SecondaryHeaderSize, FwSizeType TrailerSize>
Fw::SerializeStatus TransferFrame<FrameSize, SecondaryHeaderSize, TrailerSize>::serialize(Fw::SerializeBufferBase& buffer, TransferData_t transferData, const U8* const data, const U32 size) {
    FW_ASSERT(buffer.getBuffLeft() >= FrameSize, buffer.getBuffLeft(), FrameSize);
    m_primaryHeader.setControlInfo(transferData);

    return Fw::SerializeStatus::FW_SERIALIZE_OK;
}

template <FwSizeType FrameSize, FwSizeType SecondaryHeaderSize, FwSizeType TrailerSize>
Fw::SerializeStatus TransferFrame<FrameSize, SecondaryHeaderSize, TrailerSize>::serialize(Fw::SerializeBufferBase& buffer) {
    Fw::SerializeStatus serStatus;
    bool selfStatus;
    FW_ASSERT(buffer.getBuffLeft() >= FrameSize, buffer.getBuffLeft(), FrameSize);

    // accounts for serialization later on
    // NOTE we should be able to make this const ideally
    // but because under the hood we're using a CircularBuffer
    // to calculate the error control field we need this to be mutable
    // (even though its not mutated at the moment)
    U8 * startPtr = buffer.getBuffAddrSer();

    // Resize to just the portion of the buffer that fits a transfer frame
    // TODO this introduces a side effect (resizing the input buffer) be sure
    // that is made clear and doesn't cause problems
    // Fw::ComBuffer serBuff(startPtr, FrameSize);
    // serBuff.setBuffLen(FrameSize);

    serStatus = m_primaryHeader.serialize(buffer);
    FW_ASSERT(serStatus == Fw::FW_SERIALIZE_OK, serStatus);

    // std::array<U8, DATA_FIELD_SIZE> arr;
    // selfStatus = m_dataField.set(arr);
    // FW_ASSERT(selfStatus);

    selfStatus = m_dataField.insert(buffer);
    FW_ASSERT(selfStatus);

    // insert the error control field into the buffer
    selfStatus = m_errorControlField.insert(startPtr, buffer);
    FW_ASSERT(selfStatus, selfStatus);

    return Fw::SerializeStatus::FW_SERIALIZE_OK;
}

template <FwSizeType FrameSize, FwSizeType SecondaryHeaderSize, FwSizeType TrailerSize>
Fw::SerializeStatus TransferFrame<FrameSize, SecondaryHeaderSize, TrailerSize>::deserialize(Fw::SerializeBufferBase& buffer) {
    return Fw::SerializeStatus::FW_SERIALIZE_FORMAT_ERROR;
}

template class TransferFrame<255, 0, 2>;
template class DataField<TransferFrame<255, 0, 2>::DATA_FIELD_SIZE>;

}  // namespace TMSpaceDataLink

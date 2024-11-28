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
#include "ProtocolInterface.hpp"
#include "Svc/FrameAccumulator/FrameDetector.hpp"
#include "Svc/FramingProtocol/CCSDSProtocols/CCSDSProtocolDefs.hpp"
#include "Svc/FramingProtocol/CCSDSProtocols/TMSpaceDataLink/ProtocolDataUnits.hpp"
#include "Utils/Hash/Hash.hpp"
#include "Utils/Types/CircularBuffer.hpp"

namespace TMSpaceDataLink {

template <FwSizeType FrameSize, FwSizeType SecondaryHeaderSize, FwSizeType TrailerSize>
Fw::SerializeStatus TransferFrame<FrameSize, SecondaryHeaderSize, TrailerSize>::serialize(
    Fw::SerializeBufferBase& buffer,
    TransferData_t& transferData,
    const U8* const data,
    const U32 size) {
    FW_ASSERT(buffer.getBuffLeft() >= FrameSize, buffer.getBuffLeft(), FrameSize);
    Fw::SerializeStatus status;
    // accounts for if any serialization has been performed yet
    U8* startPtr = buffer.getBuffAddrSer();

    // Resize to just the portion of the buffer that fits a transfer frame
    // TODO this introduces a side effect (resizing the input buffer) be sure
    // that is made clear and doesn't cause problems
    Fw::SerializeBufferBase& serBuff = Fw::Buffer(startPtr, FrameSize).getSerializeRepr();

    status = m_primaryHeader.serialize(serBuff, transferData);
    FW_ASSERT(status == Fw::FW_SERIALIZE_OK, status);

    if (m_primaryHeader.getControlInfo().dataFieldStatus.hasSecondaryHeader) {
        // Not currently supported
        FW_ASSERT(0);
        status = m_secondaryHeader.serialize(serBuff);
        FW_ASSERT(status == Fw::FW_SERIALIZE_OK, status);
    }

    status = m_dataField.serialize(serBuff, data, size);
    FW_ASSERT(status == Fw::FW_SERIALIZE_OK, status);

    if (m_primaryHeader.getControlInfo().operationalControlFlag) {
        // Not currently supported
        FW_ASSERT(0);
    }
    m_errorControlField.set(startPtr, serBuff);

    return Fw::SerializeStatus::FW_SERIALIZE_OK;
}

// template <FwSizeType FrameSize, FwSizeType SecondaryHeaderSize, FwSizeType TrailerSize>
// Fw::SerializeStatus TransferFrame<FrameSize, SecondaryHeaderSize, TrailerSize>::serialize(
//     Fw::SerializeBufferBase& buffer,
//     TransferData_t& transferData,
//     const U8* const data,
//     const U32 size) {
//     FW_ASSERT(buffer.getBuffLeft() >= FrameSize, buffer.getBuffLeft(), FrameSize);
//     Fw::SerializeStatus status;
//     // accounts for if any serialization has been performed yet
//     U8* startPtr = buffer.getBuffAddrSer();

//     // Resize to just the portion of the buffer that fits a transfer frame
//     // TODO this introduces a side effect (resizing the input buffer) be sure
//     // that is made clear and doesn't cause problems
//     Fw::SerializeBufferBase& serBuff = Fw::Buffer(startPtr, FrameSize).getSerializeRepr();

//     status = m_primaryHeader.serialize(serBuff, transferData);
//     FW_ASSERT(status == Fw::FW_SERIALIZE_OK, status);

//     if (m_primaryHeader.getControlInfo().dataFieldStatus.hasSecondaryHeader) {
//         // Not currently supported
//         FW_ASSERT(0);
//         status = m_secondaryHeader.serialize(serBuff);
//         FW_ASSERT(status == Fw::FW_SERIALIZE_OK, status);
//     }

//     status = m_dataField.serialize(serBuff, data, size);
//     FW_ASSERT(status == Fw::FW_SERIALIZE_OK, status);

//     if (m_primaryHeader.getControlInfo().operationalControlFlag) {
//         // Not currently supported
//         FW_ASSERT(0);
//     }
//     m_errorControlField.set(startPtr, serBuff);

//     return Fw::SerializeStatus::FW_SERIALIZE_OK;
// }

// template <FwSizeType FrameSize, FwSizeType SecondaryHeaderSize, FwSizeType TrailerSize>
// bool TransferFrame<FrameSize, SecondaryHeaderSize, TrailerSize>::setFrameErrorControlField(
//     U8* startPtr,
//     Fw::SerializeBufferBase& buffer) const {
//     // Add frame error control (CRC-16)
//     CheckSum crc;
//     // NATIVE_UINT_TYPE serializedSize = static_cast<NATIVE_UINT_TYPE>(buffer.getBuffAddrSer() - startPtr);
//     NATIVE_UINT_TYPE serializedSize = buffer.getBuffCapacity() - sizeof(U16);
//     Types::CircularBuffer circBuff(startPtr, serializedSize);
//     Fw::SerializeStatus stat = circBuff.serialize(startPtr, serializedSize);

//     Fw::Logger::log(
//         "TM Frame: Ver %d, SC_ID %d, VC_ID %d, OCF %d, MC_CNT %d, VC_CNT %d, Len %d TF_SIZE %d\n",
//         m_primaryHeader.getControlInfo().transferFrameVersion, m_primaryHeader.getControlInfo().spacecraftId,
//         m_primaryHeader.getControlInfo().virtualChannelId, m_primaryHeader.getControlInfo().operationalControlFlag,
//         m_primaryHeader.getControlInfo().masterChannelFrameCount,
//         m_primaryHeader.getControlInfo().virtualChannelFrameCount, serializedSize, buffer.getBuffCapacity());

//     circBuff.print();

//     // Calculate the CRC based off of the buffer serialized into the circBuff
//     FwSizeType sizeOut;
//     crc.calculate(circBuff, 0, sizeOut);
//     // Ensure we've checked the whole thing
//     FW_ASSERT(sizeOut == serializedSize, sizeOut);

//     // since the CRC has to go on the end we place it there assuming that the buffer is sized correctly
//     FwSizeType skipByteNum = FW_MAX((buffer.getBuffCapacity() - 2) - buffer.getBuffLength(), 0);
//     Fw::SerializeStatus status;
//     status = buffer.serializeSkip(skipByteNum);
//     FW_ASSERT(status == Fw::FW_SERIALIZE_OK, status);

//     U16 crcValue = crc.getExpected();
//     Fw::Logger::log("framed CRC val %d\n", crcValue);
//     FW_ASSERT(crcValue);

//     status = buffer.serialize(crcValue);
//     FW_ASSERT(status == Fw::FW_SERIALIZE_OK, status);

//     return true;
// }

// template <FwSizeType FrameSize, FwSizeType SecondaryHeaderSize, FwSizeType TrailerSize>
// bool TransferFrame<FrameSize, SecondaryHeaderSize, TrailerSize>::setFrameErrorControlField() {
//     return true;
// }

// template <FwSizeType FrameSize, FwSizeType SecondaryHeaderSize, FwSizeType TrailerSize>
// Fw::SerializeStatus TransferFrame<FrameSize, SecondaryHeaderSize, TrailerSize>::serialize(
//     Fw::SerializeBufferBase& buffer) const {
//     Fw::SerializeStatus status;
//     // accounts for if any serialization has been performed yet
//     U8* startPtr = buffer.getBuffAddrSer();

//     status = m_primaryHeader.serialize(buffer);
//     FW_ASSERT(status == Fw::FW_SERIALIZE_OK, status);

//     if (m_primaryHeader.getControlInfo().dataFieldStatus.hasSecondaryHeader) {
//         // Not currently supported
//         FW_ASSERT(0);
//         status = m_secondaryHeader.serialize(buffer);
//         FW_ASSERT(status == Fw::FW_SERIALIZE_OK, status);
//     }
//     NATIVE_UINT_TYPE dataFieldSize = FrameSize - m_primaryHeader.SIZE - SecondaryHeaderSize - TrailerSize;

//     // status = m_dataField.serialize(buffer.getBuffAddrSer(), dataFieldSize);
//     FW_ASSERT(status == Fw::FW_SERIALIZE_OK, status);

//     if (m_primaryHeader.getControlInfo().operationalControlFlag) {
//         // Not currently supported
//         FW_ASSERT(0);
//     }

//     setFrameErrorControlField(startPtr, buffer);

//     return Fw::SerializeStatus::FW_SERIALIZE_OK;
// }

// template <FwSizeType FrameSize, FwSizeType SecondaryHeaderSize, FwSizeType TrailerSize>
// Fw::SerializeStatus TransferFrame<FrameSize, SecondaryHeaderSize, TrailerSize>::deserialize(
//     Fw::SerializeBufferBase& buffer) {
//     return Fw::SerializeStatus::FW_SERIALIZE_FORMAT_ERROR;
// }

}  // namespace TMSpaceDataLink

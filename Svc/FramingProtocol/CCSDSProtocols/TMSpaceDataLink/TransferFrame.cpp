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

// PrimaryHeader Control info specialization implementations
Fw::SerializeStatus ProtocolDataUnit<PRIMARY_HEADER_SERIALIZED_SIZE, PrimaryHeaderControlInfo_t>::serializeValue(
    Fw::SerializeBufferBase& buffer) const {
    Fw::SerializeStatus status;
    // Fw::ExternalSerializeBuffer buffer(buffer.getBuffAddr(), buffer.getBuffCapacity());
    U16 firstTwoOctets = 0;
    firstTwoOctets |= (m_value.transferFrameVersion & 0x03) << 14;
    firstTwoOctets |= (m_value.spacecraftId & 0x3FF) << 4;
    firstTwoOctets |= (m_value.virtualChannelId & 0x07) << 1;   // Virtual Channel ID (3 bits)
    firstTwoOctets |= (m_value.operationalControlFlag & 0x01);  // Operational Control Field Flag (1 bit)
    status = buffer.serialize(firstTwoOctets);
    FW_ASSERT(status == Fw::FW_SERIALIZE_OK, status);

    // Transfer Frame Data Field Status (16 bits) - Per spec 4.1.2.7
    U16 dataFieldStatus = 0;

    // Bit 32: Transfer Frame Secondary Header Flag (0 if no secondary header)
    // Must be static throughout Mission Phase per 4.1.2.7.2.3
    dataFieldStatus |= (m_value.dataFieldStatus.hasSecondaryHeader ? 1 : 0) << 15;

    // Bit 33: Synchronization Flag
    // 0 for octet-synchronized and forward-ordered Packets or Idle Data
    // 1 for VCA_SDU
    // Must be static within Virtual Channel per 4.1.2.7.3.3
    dataFieldStatus |= (m_value.dataFieldStatus.isSyncFlagEnabled ? 1 : 0) << 14;

    // Bit 34: Packet Order Flag
    // Set to 0 when Synchronization Flag is 0 per 4.1.2.7.4
    dataFieldStatus |= m_value.dataFieldStatus.isPacketOrdered << 13;

    // Bits 35-36: Segment Length Identifier
    // Must be set to '11' (3) when Synchronization Flag is 0 per 4.1.2.7.5.2
    dataFieldStatus |= 0x3 << 11;

    // Bits 37-47: First Header Pointer (11 bits)
    // For this implementation, assuming packet starts at beginning of data field
    // Therefore setting to 0 per 4.1.2.7.6.3
    // Set to 0x7FF (11111111111) if no packet starts in frame per 4.1.2.7.6.4
    // Set to 0x7FE (11111111110) if only idle data per 4.1.2.7.6.5
    dataFieldStatus |= m_value.dataFieldStatus.firstHeaderPointer;  // Assuming packet starts at beginning
    status = buffer.serialize(dataFieldStatus);
    FW_ASSERT(status == Fw::FW_SERIALIZE_OK, status);

    // TODO handle endianess so we can just do this
    // status = buffer.serialize(reinterpret_cast<const U8*>(&m_ci), sizeof(m_ci));
    // FW_ASSERT(status == Fw::FW_SERIALIZE_OK, status);

    return Fw::SerializeStatus::FW_SERIALIZE_OK;
}

Fw::SerializeStatus ProtocolDataUnit<PRIMARY_HEADER_SERIALIZED_SIZE, PrimaryHeaderControlInfo_t>::deserializeValue(
    Fw::SerializeBufferBase& buffer) {
    return Fw::SerializeStatus::FW_SERIALIZE_OK;
}

bool FrameErrorControlField::calc_value(U8* startPtr, Fw::SerializeBufferBase& buffer) const {
    FW_ASSERT(startPtr);

    // Assumes the startPtr indicates where the serialization started in the buffer
    // currentFrameSerializedSize represents the size of the fields serialized into the buffer so far
    FwSizeType const currentFrameSerializedSize = buffer.getBuffAddrSer() - startPtr;
    // Check the buffer has enough capacity left to hold this field
    FwSizeType const remainingCapacity = buffer.getBuffCapacity() - (buffer.getBuffAddrSer() - buffer.getBuffAddr());
    FW_ASSERT(remainingCapacity >= SERIALIZED_SIZE, remainingCapacity, currentFrameSerializedSize);

    FwSizeType const postFieldInsertionSize = currentFrameSerializedSize + SERIALIZED_SIZE;

    Types::CircularBuffer circBuff(startPtr, postFieldInsertionSize);
    Fw::SerializeStatus stat = circBuff.serialize(startPtr, postFieldInsertionSize);

    circBuff.print();

    // Calculate the CRC based off of the buffer serialized into the circBuff
    FwSizeType sizeOut;
    // Add frame error control (CRC-16)
    CheckSum crc;
    crc.calculate(circBuff, 0, sizeOut);
    // Ensure we've checked the whole thing
    FW_ASSERT(sizeOut == postFieldInsertionSize, sizeOut, postFieldInsertionSize);

    U16 crc_value = crc.getExpected();
    Fw::Logger::log("framed CRC val %d\n", crc_value);
    // Crc value should always be non-zero
    FW_ASSERT(m_value);

    return true;
}

bool FrameErrorControlField::insert(U8* startPtr, Fw::SerializeBufferBase& buffer) const {
    bool selfStatus = calc_value(startPtr, buffer);
    FW_ASSERT(selfStatus);
    return true;
}

template <typename SecondaryHeaderType,
          typename DataFieldType,
          typename OperationalControlFieldType,
          typename ErrorControlFieldType>
TransferFrameBase<SecondaryHeaderType, DataFieldType, OperationalControlFieldType, ErrorControlFieldType>::
    TransferFrameBase()
    : primaryHeader(), secondaryHeader(), dataField(), operationalControlField(), errorControlField() {}

template <typename SecondaryHeaderType,
          typename DataFieldType,
          typename OperationalControlFieldType,
          typename ErrorControlFieldType>
bool TransferFrameBase<SecondaryHeaderType, DataFieldType, OperationalControlFieldType, ErrorControlFieldType>::insert(
    Fw::SerializeBufferBase& buffer) const {
    // Store the starting position for error control calculation
    U8 const* const startPtr = buffer.getBuffAddrSer();

    // Serialize primary header
    if (!primaryHeader.insert(buffer)) {
        return false;
    }

    // Serialize secondary header
    if (!secondaryHeader.insert(buffer)) {
        return false;
    }

    // Serialize data field
    if (!dataField.insert(buffer)) {
        return false;
    }

    // Serialize operational control field if present
    // Note this should be handled by the templating but could also do this check
    // (and for the secondary header as well)
    // if (m_primaryHeader.hasOperationalControl()) {
    if (!operationalControlField.insert(buffer)) {
        return false;
    }

    // Calculate and serialize error control field
    return true;  // m_errorControlField.insert(startPtr, buffer);
}

template <typename SecondaryHeaderType,
          typename DataFieldType,
          typename OperationalControlFieldType,
          typename ErrorControlFieldType>
bool TransferFrameBase<SecondaryHeaderType, DataFieldType, OperationalControlFieldType, ErrorControlFieldType>::extract(
    Fw::SerializeBufferBase& buffer) {
    // Extract primary header first
    if (primaryHeader.extract(buffer)) {
        return false;
    }

    // Extract operational control field if present
    // if (m_primaryHeader.hasOperationalControl()) {
    if (!secondaryHeader.extract(buffer)) {
        return false;
    }
    // }

    // Extract data field
    if (!dataField.extract(buffer)) {
        return false;
    }

    // Extract operational control field if present
    // if (m_primaryHeader.hasOperationalControl()) {
    if (!operationalControlField.extract(buffer)) {
        return false;
    }
    // }

    // Extract and verify error control field
    return errorControlField.extract(buffer);
}

template class ProtocolDataUnitBase<247, std::array<U8, 247>>;
template class ProtocolDataUnit<247, std::array<U8, 247>>;

template <FwSizeType FieldSize>
DataField<FieldSize>::DataField(Fw::Buffer& srcBuff) : Base() {
    FW_ASSERT(srcBuff.getSize() >= FieldSize, srcBuff.getSize());
    bool status = false;
    // NOTE using this is ultimately the reason why we can't treat the
    // user data as const.
    // It means we can properly leverage the Fw::Buffer to pass data around
    // (one mechanism that allows us to propogate data without a copy)
    // but we should consider if this could be achieved while also treating
    // user data as const
    Fw::SerializeBufferBase &serBuffer = srcBuff.getSerializeRepr();
    serBuffer.setBuffLen(FieldSize);
    // NOTE this has a linting error but it just isn't picking up
    // that the base class provides this
    status = this->extract(serBuffer);
    FW_ASSERT(status);
}

// Instantiate DataField
// template class DataField<64>;
template class DataField<247>;

// template class DataField<FPRIME_VCA_DATA_FIELD_SIZE>;
// Explicit instantiation of the specific TransferFrameBase being used
template class TransferFrameBase<NullField, DataField<DFLT_DATA_FIELD_SIZE>, NullField, FrameErrorControlField>;

}  // namespace TMSpaceDataLink

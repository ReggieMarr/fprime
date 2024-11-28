#include "ProtocolDataUnits.hpp"

namespace TMSpaceDataLink {
PrimaryHeader::PrimaryHeader(MissionPhaseParameters_t const& params, TransferData_t& transferData) {
    // NOTE This order is important since the mission params affect how we interpret the transferData
    setControlInfo(params);
    setControlInfo(transferData);
}

PrimaryHeader::PrimaryHeader(MissionPhaseParameters_t const& params) {
    setControlInfo(params);
}

Fw::SerializeStatus PrimaryHeader::serialize(Fw::SerializeBufferBase& buffer) const {
    Fw::SerializeStatus status;
    // Fw::ExternalSerializeBuffer buffer(buffer.getBuffAddr(), buffer.getBuffCapacity());
    U16 firstTwoOctets = 0;
    firstTwoOctets |= (m_ci.transferFrameVersion & 0x03) << 14;
    firstTwoOctets |= (m_ci.spacecraftId & 0x3FF) << 4;
    firstTwoOctets |= (m_ci.virtualChannelId & 0x07) << 1;   // Virtual Channel ID (3 bits)
    firstTwoOctets |= (m_ci.operationalControlFlag & 0x01);  // Operational Control Field Flag (1 bit)
    status = buffer.serialize(firstTwoOctets);
    FW_ASSERT(status == Fw::FW_SERIALIZE_OK, status);

    // Transfer Frame Data Field Status (16 bits) - Per spec 4.1.2.7
    U16 dataFieldStatus = 0;

    // Bit 32: Transfer Frame Secondary Header Flag (0 if no secondary header)
    // Must be static throughout Mission Phase per 4.1.2.7.2.3
    dataFieldStatus |= (m_ci.dataFieldStatus.hasSecondaryHeader ? 1 : 0) << 15;

    // Bit 33: Synchronization Flag
    // 0 for octet-synchronized and forward-ordered Packets or Idle Data
    // 1 for VCA_SDU
    // Must be static within Virtual Channel per 4.1.2.7.3.3
    dataFieldStatus |= (m_ci.dataFieldStatus.isSyncFlagEnabled ? 1 : 0) << 14;

    // Bit 34: Packet Order Flag
    // Set to 0 when Synchronization Flag is 0 per 4.1.2.7.4
    dataFieldStatus |= m_ci.dataFieldStatus.isPacketOrdered << 13;

    // Bits 35-36: Segment Length Identifier
    // Must be set to '11' (3) when Synchronization Flag is 0 per 4.1.2.7.5.2
    dataFieldStatus |= 0x3 << 11;

    // Bits 37-47: First Header Pointer (11 bits)
    // For this implementation, assuming packet starts at beginning of data field
    // Therefore setting to 0 per 4.1.2.7.6.3
    // Set to 0x7FF (11111111111) if no packet starts in frame per 4.1.2.7.6.4
    // Set to 0x7FE (11111111110) if only idle data per 4.1.2.7.6.5
    dataFieldStatus |= m_ci.dataFieldStatus.firstHeaderPointer;  // Assuming packet starts at beginning
    status = buffer.serialize(dataFieldStatus);
    FW_ASSERT(status == Fw::FW_SERIALIZE_OK, status);

    // TODO handle endianess so we can just do this
    // status = buffer.serialize(reinterpret_cast<const U8*>(&m_ci), sizeof(m_ci));
    // FW_ASSERT(status == Fw::FW_SERIALIZE_OK, status);

    return Fw::SerializeStatus::FW_SERIALIZE_OK;
}

void PrimaryHeader::setControlInfo(MissionPhaseParameters_t const& params) {
    // These fields are mission parameters and must be constant over the course of the mission phase
    m_ci.transferFrameVersion = params.transferFrameVersion;
    // as per CCSDS 132.0-B-3 4.1.2.1
    // the Spacecraft Id is placed with a 4 bit offset within the first octet
    m_ci.spacecraftId = params.spaceCraftId;
    m_ci.operationalControlFlag = params.hasOperationalControlFlag;
    m_ci.dataFieldStatus.hasSecondaryHeader = params.hasSecondaryHeader;
    m_ci.dataFieldStatus.isSyncFlagEnabled = params.isSyncFlagEnabled;

    // Recommended value as per CCSDS 132.0-B-3 4.1.2.7.4
    m_ci.dataFieldStatus.isPacketOrdered = false;

    // If the sync flag field is true then segementLengthId is undefined
    // otherwise set it to 0b11
    if (!m_ci.dataFieldStatus.isSyncFlagEnabled) {
        m_ci.dataFieldStatus.segmentLengthId = 0b11;
    }
}

void PrimaryHeader::setControlInfo(TransferData_t& transferData) {
    // Set the transfer specific control info
    m_ci.virtualChannelId = transferData.virtualChannelId;
    m_ci.masterChannelFrameCount = transferData.masterChannelFrameCount;
    m_ci.virtualChannelFrameCount = transferData.virtualChannelFrameCount;

    m_ci.dataFieldStatus.firstHeaderPointer = transferData.dataFieldDesc.packetOffset;

    // The first header pointer is only defined if the sync flag is not enabled
    // We don't currently support anything other than VCAS
    if (!m_ci.dataFieldStatus.isSyncFlagEnabled) {
        if (transferData.dataFieldDesc.isFieldDataExtendedPacket) {
            FW_ASSERT(m_ci.dataFieldStatus.firstHeaderPointer == EXTEND_PACKET_TYPE);
        }
        if (transferData.dataFieldDesc.isOnlyIdleData) {
            FW_ASSERT(m_ci.dataFieldStatus.firstHeaderPointer == IDLE_TYPE);
        }
    }
}

Fw::SerializeStatus PrimaryHeader::serialize(Fw::SerializeBufferBase& buffer, TransferData_t& transferData) {
    setControlInfo(transferData);

    // Serialize the actual buffer now that the internal state is correct
    return serialize(buffer);
}

template <FwSizeType FieldSize>
Fw::SerializeStatus DataField<FieldSize>::serialize(Fw::SerializeBufferBase& buffer,
                                                    const U8* const data,
                                                    const U32 size) const {
    Fw::SerializeStatus status;

    // TODO add some extra check here, adding size maybe something else
    status = buffer.serialize(data, size);
    FW_ASSERT(status == Fw::FW_SERIALIZE_OK, status);

    return Fw::SerializeStatus::FW_SERIALIZE_OK;
}

// template <FwSizeType FieldSize>
// Fw::SerializeStatus DataField<FieldSize>::serialize(Fw::SerializeBufferBase& buffer) const {
//     Fw::SerializeStatus status;
//     FW_ASSERT(status == Fw::FW_SERIALIZE_OK, status);
//     // Serialize the raw buffer's entirity into the
//     buffer.serialize(m_data.getData(), m_data.getSize(), true);

//     return m_data.serialize(buffer);
// }

template <FwSizeType FieldSize>
Fw::SerializeStatus DataField<FieldSize>::deserialize(Fw::SerializeBufferBase& buffer) {
    return m_data.deserialize(buffer);
}

template <FwSizeType FieldSize>
Fw::SerializeStatus DataField<FieldSize>::serialize(const U8* buff, FwSizeType length) {
    // Fw::SerializeBufferBase& serBuff(buff, length);
    // return serBuff.serialize(buff, length, false);
    return Fw::FW_SERIALIZE_OK;
}

template <FwSizeType FieldSize>
Fw::SerializeStatus DataField<FieldSize>::deserialize(U8* buff, NATIVE_UINT_TYPE length) {
    // Fw::SerializeBufferBase& serBuff = m_data.getSerializeRepr();
    // return serBuff.deserialize(buff, length);
    return Fw::FW_SERIALIZE_OK;
}
template <FwSizeType Size>
DataField<Size>& DataField<Size>::operator=(const DataField& other) {
    if (this != &other) {
        // Copy the buffer data
        m_data = other.m_data;
    }
    return *this;
}

// Serialize implementation for DataField<247>
template <>
Fw::SerializeStatus DataField<247>::serialize(Fw::SerializeBufferBase& buffer) const {
    Fw::SerializeStatus status;

    // Serialize the data field content
    status = buffer.serialize(m_data.data(), m_data.size(), true);
    FW_ASSERT(status == Fw::FW_SERIALIZE_OK, status);

    return Fw::FW_SERIALIZE_OK;
}

// Deserialize implementation for DataField<247>
template <>
Fw::SerializeStatus DataField<247>::deserialize(Fw::SerializeBufferBase& buffer) {
    Fw::SerializeStatus status;

    // Get the size of data to deserialize
    NATIVE_UINT_TYPE size = FW_MIN(buffer.getBuffLeft(), m_data.size());

    FW_ASSERT(size <= m_data.size(), size); // Ensure we don't overflow

    // Deserialize into our internal buffer
    status = buffer.deserialize(m_data.data(), size);
    FW_ASSERT(status == Fw::FW_SERIALIZE_OK, status);

    return Fw::FW_SERIALIZE_OK;
}

// Assignment operator implementation for DataField<247>
template <>
DataField<247>& DataField<247>::operator=(const DataField<247>& other) {
    if (this != &other) {
        // Copy the buffer data
        m_data = other.m_data;
    }
    return *this;
}

// Explicit instantiation
template class DataField<247>;

// Explicit template instantiation
// template class DataField<247>;  // Or whatever sizes you need

bool FrameErrorControlField::set(U8* const startPtr, Fw::SerializeBufferBase& buffer) const {
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

    U16 crcValue = crc.getExpected();
    Fw::Logger::log("framed CRC val %d\n", crcValue);
    FW_ASSERT(crcValue);

    Fw::SerializeStatus status;
    status = buffer.serialize(crcValue);
    FW_ASSERT(status == Fw::FW_SERIALIZE_OK, status);

    return true;
}

}  // namespace TMSpaceDataLink

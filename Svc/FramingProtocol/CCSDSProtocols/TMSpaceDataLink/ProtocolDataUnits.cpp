#include "ProtocolDataUnits.hpp"
#include <cstddef>
#include <cstring>

namespace TMSpaceDataLink {

template <FwSizeType FieldSize, typename FieldValueType>
ProtocolDataUnitBase<FieldSize, FieldValueType>::ProtocolDataUnitBase() : m_value() {}

template <FwSizeType FieldSize, typename FieldValueType>
ProtocolDataUnitBase<FieldSize, FieldValueType>::ProtocolDataUnitBase(FieldValueType srcVal) : m_value(srcVal) {}

template <FwSizeType FieldSize, typename FieldValueType>
void ProtocolDataUnitBase<FieldSize, FieldValueType>::set(FieldValueType const& val) {
    m_value = val;
}

template <FwSizeType FieldSize, typename FieldValueType>
void ProtocolDataUnitBase<FieldSize, FieldValueType>::get(FieldValueType& val) const {
    val = m_value;
}

template <FwSizeType FieldSize, typename FieldValueType>
bool ProtocolDataUnitBase<FieldSize, FieldValueType>::insert(Fw::SerializeBufferBase& buffer) const {
    return serializeValue(buffer) == Fw::FW_SERIALIZE_OK;
}

template <FwSizeType FieldSize, typename FieldValueType>
bool ProtocolDataUnitBase<FieldSize, FieldValueType>::insert(Fw::SerializeBufferBase& buffer, FieldValueType& val) {
    FieldValueType lastVal = m_value;
    set(val);
    if (!insert(buffer)) {
        set(lastVal);
        return false;
    }
    return true;
}

template <FwSizeType FieldSize, typename FieldValueType>
bool ProtocolDataUnitBase<FieldSize, FieldValueType>::extract(Fw::SerializeBufferBase& buffer) {
    return deserializeValue(buffer) == Fw::FW_SERIALIZE_OK;
}

template <FwSizeType FieldSize, typename FieldValueType>
bool ProtocolDataUnitBase<FieldSize, FieldValueType>::extract(Fw::SerializeBufferBase& buffer, FieldValueType& val) {
    FieldValueType lastVal = m_value;
    // NOTE in practice this won't actually be handled given the way we currently setup FW_ASSERT
    // TODO configure FW_ASSERT to get something like exception handling here;
    if (!extract(buffer)) {
        m_value = lastVal;
        return false;
    }
    val = m_value;
    return true;
}

template <FwSizeType FieldSize, typename FieldValueType>
ProtocolDataUnitBase<FieldSize, FieldValueType>& ProtocolDataUnitBase<FieldSize, FieldValueType>::operator=(
    const ProtocolDataUnitBase<FieldSize, FieldValueType>& other) {
    if (this != &other) {
        set(other.m_value);
    }
    return *this;
}

// Primitive type template implementations
template <FwSizeType FieldSize, typename FieldValueType>
Fw::SerializeStatus ProtocolDataUnit<FieldSize, FieldValueType>::serializeValue(Fw::SerializeBufferBase& buffer) const {
    return buffer.serialize(this->m_value);
}

template <FwSizeType FieldSize, typename FieldValueType>
Fw::SerializeStatus ProtocolDataUnit<FieldSize, FieldValueType>::deserializeValue(Fw::SerializeBufferBase& buffer) {
    return buffer.deserialize(this->m_value);
}

// Array specialization implementations
template <FwSizeType FieldSize>
Fw::SerializeStatus ProtocolDataUnit<FieldSize, std::array<U8, FieldSize>>::serializeValue(
    Fw::SerializeBufferBase& buffer) const {
    U8 const* buffPtr = this->m_value.data();
    NATIVE_UINT_TYPE buffSize = this->m_value.size();
    return buffer.serialize(buffPtr, buffSize, true);
}

template <FwSizeType FieldSize>
Fw::SerializeStatus ProtocolDataUnit<FieldSize, std::array<U8, FieldSize>>::deserializeValue(
    Fw::SerializeBufferBase& buffer) {
    U8* buffPtr = this->m_value.data();
    NATIVE_UINT_TYPE buffSize = this->m_value.size();
    return buffer.deserialize(buffPtr, buffSize, true);
}

template <FwSizeType FieldSize>
void ProtocolDataUnit<FieldSize, std::array<U8, FieldSize>>::set(FieldValue_t const& val) {
    // To set the internal state we need to have a value that is the same size or smaller
    FW_ASSERT(val.size() <= this->m_value.size(), val.size(), this->m_value.size());
    (void)std::memcpy(this->m_value.data(), val.data(), val.size());
}

template <FwSizeType FieldSize>
void ProtocolDataUnit<FieldSize, std::array<U8, FieldSize>>::get(FieldValue_t& val) const {
    // To set the internal state we need to have a value that is the same size or smaller
    FW_ASSERT(val.size() <= this->m_value.size(), val.size(), this->m_value.size());
    (void)std::memcpy(val.data(), this->m_value.data(), val.size());
}

template <FwSizeType FieldSize>
void ProtocolDataUnit<FieldSize, std::array<U8, FieldSize>>::set(U8 const *buffPtr, FwSizeType size) {
    // To set the internal state we need to have a value that is the same size
    FW_ASSERT(buffPtr != nullptr, size);
    FW_ASSERT(size <= this->m_value.size(), size, this->m_value.size());
    (void)std::memcpy(this->m_value.data(), buffPtr, size);
}

// nullptr specialization implementations
Fw::SerializeStatus ProtocolDataUnit<0, std::nullptr_t>::serializeValue(Fw::SerializeBufferBase& buffer) const {
    return Fw::SerializeStatus::FW_SERIALIZE_OK;
    // FW_ASSERT(0);
    // return Fw::SerializeStatus::FW_SERIALIZE_FORMAT_ERROR;
}

Fw::SerializeStatus ProtocolDataUnit<0, std::nullptr_t>::deserializeValue(Fw::SerializeBufferBase& buffer) {
    // allow this no-op for now
    return Fw::SerializeStatus::FW_SERIALIZE_OK;
    // FW_ASSERT(0);
    // return Fw::SerializeStatus::FW_SERIALIZE_FORMAT_ERROR;
}

void ProtocolDataUnit<0, std::nullptr_t>::set(FieldValue_t const& buffer) {
    FW_ASSERT(0);
}

void ProtocolDataUnit<0, std::nullptr_t>::get(FieldValue_t& buffer) const {
    FW_ASSERT(0);
}

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

PrimaryHeader::PrimaryHeader(MissionPhaseParameters_t const& params, TransferData_t& transferData) {
    // NOTE This order is important since the mission params affect how we interpret the transferData
    setControlInfo(params);
    setControlInfo(transferData);
}

PrimaryHeader::PrimaryHeader(MissionPhaseParameters_t const& params) {
    setControlInfo(params);
}

void PrimaryHeader::setControlInfo(MissionPhaseParameters_t const& params) {
    // These fields are mission parameters and must be constant over the course of the mission phase
    this->m_value.transferFrameVersion = params.transferFrameVersion;
    // as per CCSDS 132.0-B-3 4.1.2.1
    // the Spacecraft Id is placed with a 4 bit offset within the first octet
    this->m_value.spacecraftId = params.spaceCraftId;
    this->m_value.operationalControlFlag = params.hasOperationalControlFlag;
    this->m_value.dataFieldStatus.hasSecondaryHeader = params.hasSecondaryHeader;
    this->m_value.dataFieldStatus.isSyncFlagEnabled = params.isSyncFlagEnabled;

    // Recommended value as per CCSDS 132.0-B-3 4.1.2.7.4
    this->m_value.dataFieldStatus.isPacketOrdered = false;

    // If the sync flag field is true then segementLengthId is undefined
    // otherwise set it to 0b11
    if (!this->m_value.dataFieldStatus.isSyncFlagEnabled) {
        this->m_value.dataFieldStatus.segmentLengthId = 0b11;
    }
}

void PrimaryHeader::setControlInfo(TransferData_t& transferData) {
    // Set the transfer specific control info
    this->m_value.virtualChannelId = transferData.virtualChannelId;
    this->m_value.masterChannelFrameCount = transferData.masterChannelFrameCount;
    this->m_value.virtualChannelFrameCount = transferData.virtualChannelFrameCount;
    this->m_value.dataFieldStatus.firstHeaderPointer = transferData.dataFieldDesc.packetOffset;

    // The first header pointer is only defined if the sync flag is not enabled
    // We don't currently support anything other than VCAS
    if (!this->m_value.dataFieldStatus.isSyncFlagEnabled) {
        if (transferData.dataFieldDesc.isFieldDataExtendedPacket) {
            FW_ASSERT(this->m_value.dataFieldStatus.firstHeaderPointer == EXTEND_PACKET_TYPE);
        }
        if (transferData.dataFieldDesc.isOnlyIdleData) {
            FW_ASSERT(this->m_value.dataFieldStatus.firstHeaderPointer == IDLE_TYPE);
        }
    }
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

// Instantiate DataField
template class DataField<64>;
template class DataField<247>;

// NOTE this should probably be done on the instatiation side of things
// Instantiate the base class
template class ProtocolDataUnitBase<247, std::array<U8, 247>>;
template class ProtocolDataUnitBase<sizeof(U16), U16>;
template class ProtocolDataUnitBase<6, PrimaryHeaderControlInfo_t>;
template class ProtocolDataUnitBase<0, std::nullptr_t>;

// Instantiate the primary template (if needed)
// FIXME I get linter errors here but they don't seem to affect compilation:
// In template: dependent using declaration resolved to type without 'typename' (lsp)
template class ProtocolDataUnit<247, std::array<U8, 247>>;
template class ProtocolDataUnit<sizeof(U16), U16>;
template class ProtocolDataUnit<6, PrimaryHeaderControlInfo_t>;
template class ProtocolDataUnit<0, std::nullptr_t>;

// If you're using other sizes, add those too, for example:
template class ProtocolDataUnitBase<64, std::array<U8, 64>>;
// FIXME I get linter errors here but they don't seem to affect compilation:
// In template: dependent using declaration resolved to type without 'typename' (lsp)
template class ProtocolDataUnit<64, std::array<U8, 64>>;

}  // namespace TMSpaceDataLink

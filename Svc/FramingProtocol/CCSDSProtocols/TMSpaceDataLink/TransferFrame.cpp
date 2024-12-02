// ======================================================================
// \title  TMSpaceDataLink TransferFrame.cpp
// \author Reginald Marr
// \brief  Implementation of TMSpaceDataLink TransferFrame
//
// ======================================================================
#include "TransferFrame.hpp"
#include <array>
#include <cstring>
#include "FpConfig.h"
#include "FpConfig.hpp"
#include "Fw/Com/ComBuffer.hpp"
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
    U8* startingSerLoc = buffer.getBuffAddrSer();
    Fw::SerializeStatus status;
    // Fw::ExternalSerializeBuffer buffer(buffer.getBuffAddr(), buffer.getBuffCapacity());
    U16 firstTwoOctets = 0;
    firstTwoOctets |= (m_value.transferFrameVersion & 0x03) << 14;
    firstTwoOctets |= (m_value.spacecraftId & 0x3FF) << 4;
    firstTwoOctets |= (m_value.virtualChannelId & 0x07) << 1;   // Virtual Channel ID (3 bits)
    firstTwoOctets |= (m_value.operationalControlFlag & 0x01);  // Operational Control Field Flag (1 bit)
    status = buffer.serialize(firstTwoOctets);
    FW_ASSERT(status == Fw::FW_SERIALIZE_OK, status);

    U16 frameCounts = 0;
    frameCounts |= (m_value.masterChannelFrameCount & 0xFF00) << 8;
    frameCounts |= (m_value.virtualChannelFrameCount & 0x00FF);

    status = buffer.serialize(frameCounts);
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
    // NOTE may replace this with an assert/enforcement later
    if (!m_value.dataFieldStatus.isSyncFlagEnabled && m_value.dataFieldStatus.segmentLengthId) {
        Fw::Logger::log("[WARNING] when sync flag is enabled segLength should be 0x00 not %d\n",
                        m_value.dataFieldStatus.segmentLengthId);
    }
    // dataFieldStatus |= (m_value.dataFieldStatus.isSyncFlagEnabled ? 0 : 0x3) << 11;
    dataFieldStatus |= (m_value.dataFieldStatus.segmentLengthId) << 11;

    // Bits 37-47: First Header Pointer (11 bits)
    // For this implementation, assuming packet starts at beginning of data field
    // Therefore setting to 0 per 4.1.2.7.6.3
    // Set to 0x7FF (11111111111) if no packet starts in frame per 4.1.2.7.6.4
    // Set to 0x7FE (11111111110) if only idle data per 4.1.2.7.6.5
    dataFieldStatus |= m_value.dataFieldStatus.firstHeaderPointer;
    status = buffer.serialize(dataFieldStatus);
    FW_ASSERT(status == Fw::FW_SERIALIZE_OK, status);

    FwSizeType serSize = buffer.getBuffAddrSer() - startingSerLoc;
    FW_ASSERT(serSize == SERIALIZED_SIZE, serSize, SERIALIZED_SIZE);

    return Fw::SerializeStatus::FW_SERIALIZE_OK;
}

// This is the Primary Header Deserializer.
Fw::SerializeStatus ProtocolDataUnit<PRIMARY_HEADER_SERIALIZED_SIZE, PrimaryHeaderControlInfo_t>::deserializeValue(
    Fw::SerializeBufferBase& buffer) {
    Fw::SerializeStatus status;

    // Deserialize first two octets
    U16 firstTwoOctets;
    status = buffer.deserialize(firstTwoOctets);
    FW_ASSERT(status == Fw::FW_SERIALIZE_OK, status);

    // Extract fields in reverse order of serialization
    m_value.operationalControlFlag = firstTwoOctets & 0x01;
    m_value.virtualChannelId = (firstTwoOctets >> 1) & 0x07;
    m_value.spacecraftId = (firstTwoOctets >> 4) & 0x3FF;
    m_value.transferFrameVersion = (firstTwoOctets >> 14) & 0x03;

    U16 frameCounts = 0;
    status = buffer.deserialize(frameCounts);
    FW_ASSERT(status == Fw::FW_SERIALIZE_OK, status);

    frameCounts |= (m_value.masterChannelFrameCount & 0xFF00) << 8;
    frameCounts |= (m_value.virtualChannelFrameCount & 0x00FF);

    m_value.virtualChannelFrameCount = frameCounts & 0x00FF;
    m_value.masterChannelFrameCount = (frameCounts >> 8) & 0x00FF;

    // Deserialize data field status
    U16 dataFieldStatus;
    status = buffer.deserialize(dataFieldStatus);
    FW_ASSERT(status == Fw::FW_SERIALIZE_OK, status);

    // Extract data field status flags
    m_value.dataFieldStatus.firstHeaderPointer = dataFieldStatus & 0x7FF;  // Bits 37-47
    // Bits 35-36 (Segment Length Identifier)
    U8 segLength = 0;
    segLength = (dataFieldStatus >> 11) & 0x3;

    m_value.dataFieldStatus.isPacketOrdered = (dataFieldStatus >> 13) & 0x01;     // Bit 34
    m_value.dataFieldStatus.isSyncFlagEnabled = (dataFieldStatus >> 14) & 0x01;   // Bit 33
    m_value.dataFieldStatus.hasSecondaryHeader = (dataFieldStatus >> 15) & 0x01;  // Bit 32

    // TODO should probably add an assert here although I'm not certain this would never be possible
    if (!m_value.dataFieldStatus.isSyncFlagEnabled && segLength != 0x00) {
        Fw::Logger::log("[WARNING] when sync flag is enabled segLength should be 0x00 not %d\n", segLength);
    }

    return Fw::SerializeStatus::FW_SERIALIZE_OK;
}

// NOTE sourceBufferPtr should be const but can't be at the moment due to requirements by the circBuff
template <U16 StartWord, FwSizeType TransferFrameLength>
bool FrameErrorControlField<StartWord, TransferFrameLength>::calcBufferCRC(U8* sourceBufferPtr,
                                                                           FwSizeType const sourceBufferSize,
                                                                           U16& crcValue) const {
    Types::CircularBuffer circBuff(sourceBufferPtr, sourceBufferSize);
    Fw::SerializeStatus stat = circBuff.serialize(sourceBufferPtr, sourceBufferSize);

    circBuff.print();

    // Calculate the CRC based off of the buffer serialized into the circBuff
    FwSizeType sizeOut;
    // Add frame error control (CRC-16)
    CheckSum crc;
    crc.calculate(circBuff, 0, sizeOut);
    // Ensure we've checked the whole thing (minus the error control field itself)
    FW_ASSERT(sizeOut == sourceBufferSize - SERIALIZED_SIZE, sizeOut, sourceBufferSize);

    crcValue = crc.getExpected();
    Fw::Logger::log("framed CRC val %d\n", crcValue);
    // Crc value should always be non-zero
    FW_ASSERT(crcValue != 0);

    return true;
}

template <U16 StartWord, FwSizeType TransferFrameLength>
bool FrameErrorControlField<StartWord, TransferFrameLength>::insert(U8* errorCheckStart,
                                                                    Fw::SerializeBufferBase& buffer) const {
    FW_ASSERT(errorCheckStart);
    // Assumes the startPtr indicates where the serialization started in the buffer
    // currentFrameSerializedSize represents the size of the fields serialized into the buffer so far
    FwSizeType const currentFrameSerializedSize = buffer.getBuffAddrSer() - errorCheckStart;
    // Check the buffer has enough capacity left to hold this field
    FwSizeType const remainingCapacity = buffer.getBuffCapacity() - (buffer.getBuffAddrSer() - buffer.getBuffAddr());
    FW_ASSERT(remainingCapacity >= SERIALIZED_SIZE, remainingCapacity, currentFrameSerializedSize);
    // NOTE We may actually want to just pass the currentFrameSerializedSize
    FwSizeType const postFieldInsertionSize = currentFrameSerializedSize + SERIALIZED_SIZE;

    U16 crcValue;
    bool selfStatus = calcBufferCRC(errorCheckStart, postFieldInsertionSize, crcValue);
    FW_ASSERT(selfStatus);

    Fw::SerializeStatus serStatus;
    serStatus = buffer.serialize(crcValue);
    FW_ASSERT(serStatus == Fw::SerializeStatus::FW_SERIALIZE_OK, serStatus);

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
    U8* const startPtr = buffer.getBuffAddrSer();
    bool status = true;

    // Serialize primary header
    status = primaryHeader.insert(buffer);
    FW_ASSERT(status);

    // Serialize secondary header
    status = secondaryHeader.insert(buffer);
    FW_ASSERT(status);

    // Serialize data field
    status = dataField.insert(buffer);
    FW_ASSERT(status);

    // Serialize operational control field if present
    // Note this should be handled by the templating but could also do this check
    // (and for the secondary header as well)
    // if (m_primaryHeader.hasOperationalControl()) {
    status = operationalControlField.insert(buffer);
    FW_ASSERT(status);

    // Calculate and serialize error control field
    status = errorControlField.insert(startPtr, buffer);
    FW_ASSERT(status);

    // Make sure we've serialized the correct size
    // TODO do this on each ser/deser with pdu base
    FwSizeType serSize = buffer.getBuffAddrSer() - startPtr;
    FW_ASSERT(serSize == SERIALIZED_SIZE, serSize, SERIALIZED_SIZE);

    return true;
}

template <typename SecondaryHeaderType,
          typename DataFieldType,
          typename OperationalControlFieldType,
          typename ErrorControlFieldType>
bool TransferFrameBase<SecondaryHeaderType, DataFieldType, OperationalControlFieldType, ErrorControlFieldType>::extract(
    Fw::SerializeBufferBase& buffer) {
    U8 const* startPtr = buffer.getBuffAddrLeft();
    bool status;
    // Extract primary header first
    status = primaryHeader.extract(buffer);
    FW_ASSERT(status);

    // Extract operational control field if present
    // if (m_primaryHeader.hasOperationalControl()) {
    status = secondaryHeader.extract(buffer);
    FW_ASSERT(status);

    // Extract data field
    status = dataField.extract(buffer);
    FW_ASSERT(status);

    // Extract operational control field if present
    // if (m_primaryHeader.hasOperationalControl()) {
    status = operationalControlField.extract(buffer);
    FW_ASSERT(status);

    // Extract and verify error control field
    U16 calculatedCrc;

    // NOTE we need this for now to ensure const correctness but adds another copy overhead
    // TODO remove during performance stripping
    std::array<U8, SERIALIZED_SIZE> crcBuff;
    (void)std::memcpy(crcBuff.data(), startPtr, crcBuff.size());
    status = errorControlField.calcBufferCRC(crcBuff.data(), crcBuff.size(), calculatedCrc);

    status = errorControlField.extract(buffer);
    FW_ASSERT(status);

    U16 retrievedCrc;
    errorControlField.get(retrievedCrc);
    FW_ASSERT(retrievedCrc == calculatedCrc, retrievedCrc, calculatedCrc);

    return true;
}

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
    Fw::SerializeBufferBase& serBuffer = srcBuff.getSerializeRepr();
    serBuffer.setBuffLen(FieldSize);
    // NOTE this has a linting error but it just isn't picking up
    // that the base class provides this
    status = this->extract(serBuffer);
    FW_ASSERT(status);
}

// Instantiate DataField
template class FrameErrorControlField<CCSDS_SCID, FPrimeTransferFrame::SERIALIZED_SIZE>;
template class DataField<247>;

// template class DataField<FPRIME_VCA_DATA_FIELD_SIZE>;
// Explicit instantiation of the specific TransferFrameBase being used
template class TransferFrameBase<FPrimeSecondaryHeaderField,
                                 FPrimeDataField,
                                 FPrimeOperationalControlField,
                                 FPrimeErrorControlField>;

}  // namespace TMSpaceDataLink

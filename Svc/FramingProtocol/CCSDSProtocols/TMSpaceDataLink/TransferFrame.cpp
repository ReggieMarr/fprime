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
#include "Utils/Hash/Hash.hpp"
#include "Utils/Types/CircularBuffer.hpp"

namespace TMSpaceDataLink {

PrimaryHeader::PrimaryHeader(MissionPhaseParameters_t const& params, TransferData_t &transferData) {
    setControlInfo(transferData);
    setControlInfo(params);
}

PrimaryHeader::PrimaryHeader(MissionPhaseParameters_t const& params) {
    setControlInfo(params);
}

Fw::SerializeStatus PrimaryHeader::serialize(Fw::SerializeBufferBase& buffer) const {
    Fw::SerializeStatus status;
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

Fw::SerializeStatus DataField::serialize(Fw::SerializeBufferBase& buffer, const U8* const data, const U32 size) const {
    Fw::SerializeStatus status;

    // TODO add some extra check here, adding size maybe something else
    status = buffer.serialize(data, size);
    FW_ASSERT(status == Fw::FW_SERIALIZE_OK, status);

    return Fw::SerializeStatus::FW_SERIALIZE_OK;
}

Fw::SerializeStatus TransferFrame::serialize(Fw::SerializeBufferBase& buffer,
                                             TransferData_t& transferData,
                                             const U8* const data,
                                             const U32 size) {
    Fw::SerializeStatus status;
    // accounts for if any serialization has been performed yet
    U8* startPtr = buffer.getBuffAddrSer();

    status = m_primaryHeader.serialize(buffer, transferData);
    FW_ASSERT(status == Fw::FW_SERIALIZE_OK, status);

    if (m_primaryHeader.getControlInfo().dataFieldStatus.hasSecondaryHeader) {
        status = m_secondaryHeader.serialize(buffer);
        FW_ASSERT(status == Fw::FW_SERIALIZE_OK, status);
    }

    status = m_dataField.serialize(buffer, data, size);
    FW_ASSERT(status == Fw::FW_SERIALIZE_OK, status);

    if (m_primaryHeader.getControlInfo().operationalControlFlag) {
        // Not currently supported
        FW_ASSERT(0);
    }

    // Add frame error control (CRC-16)
    CheckSum crc;
    // NATIVE_UINT_TYPE serializedSize = static_cast<NATIVE_UINT_TYPE>(buffer.getBuffAddrSer() - startPtr);
    NATIVE_UINT_TYPE serializedSize = buffer.getBuffCapacity() - sizeof(U16);
    Types::CircularBuffer circBuff(startPtr, serializedSize);
    Fw::SerializeStatus stat = circBuff.serialize(startPtr, serializedSize);

    Fw::Logger::log(
        "TM Frame: Ver %d, SC_ID %d, VC_ID %d, OCF %d, MC_CNT %d, VC_CNT %d, Len %d TF_SIZE %d\n",
        m_primaryHeader.getControlInfo().transferFrameVersion, m_primaryHeader.getControlInfo().spacecraftId,
        m_primaryHeader.getControlInfo().virtualChannelId, m_primaryHeader.getControlInfo().operationalControlFlag,
        m_primaryHeader.getControlInfo().masterChannelFrameCount,
        m_primaryHeader.getControlInfo().virtualChannelFrameCount, serializedSize, buffer.getBuffCapacity());

    circBuff.print();

    // Calculate the CRC based off of the buffer serialized into the circBuff
    FwSizeType sizeOut;
    crc.calculate(circBuff, 0, sizeOut);
    // Ensure we've checked the whole thing
    FW_ASSERT(sizeOut == serializedSize, sizeOut);

    // since the CRC has to go on the end we place it there assuming that the buffer is sized correctly
    FwSizeType skipByteNum = FW_MAX((buffer.getBuffCapacity() - 2) - buffer.getBuffLength(), 0);
    status = buffer.serializeSkip(skipByteNum);
    FW_ASSERT(status == Fw::FW_SERIALIZE_OK, status);

    U16 crcValue = crc.getExpected();
    Fw::Logger::log("framed CRC val %d\n", crcValue);
    FW_ASSERT(crcValue);

    status = buffer.serialize(crcValue);
    FW_ASSERT(status == Fw::FW_SERIALIZE_OK, status);

    return Fw::SerializeStatus::FW_SERIALIZE_OK;
}

}  // namespace TMSpaceDataLink

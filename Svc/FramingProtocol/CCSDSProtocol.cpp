// ======================================================================
// \title  CCSDSProtocol.cpp
// \author Reginald Marr
// \brief  cpp file for CCSDSProtocol class
//
// \copyright
// Copyright 2009-2022, by the California Institute of Technology.
// ALL RIGHTS RESERVED.  United States Government Sponsorship
// acknowledged.
//
// ======================================================================
#include "CCSDSProtocol.hpp"
#include "FpConfig.h"
#include "FpConfig.hpp"
#include "Fw/Logger/Logger.hpp"
#include "Fw/Types/Assert.hpp"
#include "Fw/Types/Serializable.hpp"
#include "Svc/FrameAccumulator/FrameDetector.hpp"
#include "Svc/FrameAccumulator/FrameDetector/CCSDSFrameDetector.hpp"
#include "Utils/Hash/Hash.hpp"
#include "Utils/Types/CircularBuffer.hpp"
#include "Svc/FramingProtocol/CCSDSProtocolDefs.hpp"

namespace Svc {

void SpacePacketFraming::frame(const U8* const data, const U32 size, Fw::ComPacket::ComPacketType packet_type) {
        FW_ASSERT(data != nullptr);
        FW_ASSERT(m_interface != nullptr);
        // Review this before using
        FW_ASSERT(0);

        // Calculate total size including Space Packet header
        const U32 totalSize = SPACE_PACKET_HEADER_SIZE + size;
        Fw::Buffer buffer = m_interface->allocate(totalSize);

        // Get serialize representation
        Fw::SerializeBufferBase& serializer = buffer.getSerializeRepr();

        // Build Space Packet Header
        U8 firstByte = 0;
        firstByte |= (0 & 0x07) << 5;       // Version Number (3 bits)
        firstByte |= (0 & 0x01) << 4;       // Packet Type (1 bit)
        firstByte |= (0 & 0x01) << 3;       // Secondary Header Flag (1 bit)
        firstByte |= (m_apid >> 8) & 0x07;  // APID MSB (3 bits)

        // Serialize header
        Fw::SerializeStatus status;
        status = serializer.serialize(firstByte);
        FW_ASSERT(status == Fw::FW_SERIALIZE_OK, status);

        // APID LSB
        status = serializer.serialize(static_cast<U8>(m_apid & 0xFF));
        FW_ASSERT(status == Fw::FW_SERIALIZE_OK, status);

        // Sequence Control (2 bytes)
        // U16 seqControl = (static_cast<U16>(SequenceFlags::UNSEGMENTED) << 14) | (m_sequenceCount & 0x3FFF);
        // status = serializer.serialize(seqControl);
        // FW_ASSERT(status == Fw::FW_SERIALIZE_OK, status);

        // Packet Length (size - 1)
        status = serializer.serialize(static_cast<U16>(size - 1));
        FW_ASSERT(status == Fw::FW_SERIALIZE_OK, status);

        // Add packet data
        status = serializer.serialize(data, size, true);
        FW_ASSERT(status == Fw::FW_SERIALIZE_OK, status);

        buffer.setSize(totalSize);
        m_interface->send(buffer);

        m_sequenceCount = (m_sequenceCount + 1) & 0x3FFF;
}


void TCSpaceDataLinkFraming::frame(const U8* const data, const U32 dataSize, Fw::ComPacket::ComPacketType packet_type) {
        FW_ASSERT(data != nullptr);
        FW_ASSERT(m_interface != nullptr);

        // Calculate total size including TC frame header and trailer
        const U32 transferFrameSize = TC_SPACE_DATA_LINK_HEADER_SIZE + dataSize + TC_SPACE_DATA_LINK_TRAILER_SIZE;
        FW_ASSERT(transferFrameSize < CCSDS_SDLTP_TC_MAX_FRAME_LENGTH && transferFrameSize > 0, transferFrameSize);

        Fw::Buffer buffer = m_interface->allocate(transferFrameSize);

        Fw::SerializeBufferBase& serializer = buffer.getSerializeRepr();

        // Build TC Primary Frame Header
        U16 firstTwoOctets = 0;

        // CCSDS 232.0-B-4 reserves this combo at the current version
        FW_ASSERT(!(!(m_bypassFlag & 0x01) && m_controlCommandFlag & 0x01),
                  (m_bypassFlag & 0x01 && m_controlCommandFlag & 0x01));

        // First octet (MSB)
        firstTwoOctets |= (m_version & 0x03) << 14;            // Version Number (2 bits)
        firstTwoOctets |= (m_bypassFlag & 0x01) << 13;         // Bypass Flag (1 bit)
        firstTwoOctets |= (m_controlCommandFlag & 0x01) << 12; // Control Command Flag (1 bit)
        firstTwoOctets |= (0 & 0x03) << 10;                    // Reserved Spare (2 bits)
        firstTwoOctets |= (m_spacecraftId & 0x3FF);            // Spacecraft ID (10 bits)

        // Serialize the first two octets
        Fw::SerializeStatus status;
        status = serializer.serialize(firstTwoOctets);
        FW_ASSERT(status == Fw::FW_SERIALIZE_OK, status);

        // Next two octets: Virtual Channel ID (6 bits) and Frame Length (10 bits)
        U16 nextTwoOctets = 0;
        // CCSDS 232.0-B-4 4.1.2.6
        nextTwoOctets |= (m_virtualChannelId & 0x3F) << 10;  // Virtual Channel ID (6 bits)

        // CCSDS 232.0-B-4 4.1.2.7
        U16 frame_length = transferFrameSize - 1;

        nextTwoOctets |= (frame_length & 0x3FF);                     // Frame Length (10 bits)

        status = serializer.serialize(nextTwoOctets);
        FW_ASSERT(status == Fw::FW_SERIALIZE_OK, status);

        // Frame Sequence Number (8 bits)
        status = serializer.serialize(m_frameSequence);
        FW_ASSERT(status == Fw::FW_SERIALIZE_OK, status);

        // CCSDS 232.0-B-4 4.1.2.3.2
        bool isTransferFrameFrameDataUnit = (m_controlCommandFlag & 0x01) ? false : true;

        // As per CCSDS 232.0-B-4 4.1.3.2.2
        // If the transfer frame is of Type-C (Frame Data Unit)
        // then we need to have a segment header.
        // Segment headers are not currently supported
        FW_ASSERT(!isTransferFrameFrameDataUnit, m_controlCommandFlag);

        // Add frame data
        status = serializer.serialize(data, dataSize, true);
        FW_ASSERT(status == Fw::FW_SERIALIZE_OK, status);

        // Add frame error control (CRC-16)
        TC_CheckSum crc;
        Types::CircularBuffer circBuff(buffer.getData(), frame_length);
        Fw::SerializeStatus ss = circBuff.serialize(buffer.getData(), frame_length);
        FwSizeType size_out;
        // As per CCSDS 232.0-B-4 4.1.4.2.1 the CRC is to be calculated from the first bit of the
        // transfer frame until the Frame Error Control Field (i.e. the transfer frame's length - 2 octets)
        // since the frame_length is the transfer frame's length - 1 we need to subtract one more
        FrameDetector::Status fstats = crc.calculate(circBuff, frame_length - 1, size_out);
        status = serializer.serialize(crc.getExpected());
        FW_ASSERT(status == Fw::FW_SERIALIZE_OK, status);

        buffer.setSize(transferFrameSize);

        m_interface->send(buffer);
        m_frameSequence = (m_frameSequence + 1) & 0xFF;
}

void TMSpaceDataLinkFraming::frame(const U8* const data, const U32 size, Fw::ComPacket::ComPacketType packet_type) {
        FW_ASSERT(data != nullptr);
        FW_ASSERT(m_interface != nullptr);

        // Calculate total size including TM frame header
        const U32 totalSize = TM_FRAME_PRIMARY_HEADER_SIZE + size + FRAME_ERROR_CONTROL_SIZE;
        Fw::Buffer buffer = m_interface->allocate(totalSize);
        Fw::SerializeBufferBase& serializer = buffer.getSerializeRepr();
        Fw::SerializeStatus status;

        // First two octets: Master Channel ID (12 bits) + Virtual Channel ID (3 bits) + OCF Flag (1 bit)
        U16 firstTwoOctets = 0;
        firstTwoOctets |= (m_tfVersionNumber & 0x03) << 14;
        firstTwoOctets |= (m_spacecraftId & 0x3FF) << 4;
        firstTwoOctets |= (m_virtualChannelId & 0x07) << 1;     // Virtual Channel ID (3 bits)
        firstTwoOctets |= m_operationalControlFlag & 0x01;      // Operational Control Field Flag (1 bit)
        status = serializer.serialize(firstTwoOctets);
        FW_ASSERT(status == Fw::FW_SERIALIZE_OK, status);

        // Master Channel Frame Count (8 bits)
        status = serializer.serialize(m_masterChannelFrameCount);
        FW_ASSERT(status == Fw::FW_SERIALIZE_OK, status);

        // Virtual Channel Frame Count (8 bits)
        status = serializer.serialize(m_virtualChannelFrameCount);
        FW_ASSERT(status == Fw::FW_SERIALIZE_OK, status);

        // Transfer Frame Data Field Status (16 bits) - Per spec 4.1.2.7
        U16 dataFieldStatus = 0;

        // Bit 32: Transfer Frame Secondary Header Flag (0 if no secondary header)
        // Must be static throughout Mission Phase per 4.1.2.7.2.3
        dataFieldStatus |= (m_hasSecondaryHeader ? 1 : 0) << 15;

        // Bit 33: Synchronization Flag
        // 0 for octet-synchronized and forward-ordered Packets or Idle Data
        // 1 for VCA_SDU
        // Must be static within Virtual Channel per 4.1.2.7.3.3
        dataFieldStatus |= (m_isVcaSdu ? 1 : 0) << 14;

        // Bit 34: Packet Order Flag
        // Set to 0 when Synchronization Flag is 0 per 4.1.2.7.4
        dataFieldStatus |= 0 << 13;

        // Bits 35-36: Segment Length Identifier
        // Must be set to '11' (3) when Synchronization Flag is 0 per 4.1.2.7.5.2
        dataFieldStatus |= 0x3 << 11;

        // Bits 37-47: First Header Pointer (11 bits)
        // For this implementation, assuming packet starts at beginning of data field
        // Therefore setting to 0 per 4.1.2.7.6.3
        // Set to 0x7FF (11111111111) if no packet starts in frame per 4.1.2.7.6.4
        // Set to 0x7FE (11111111110) if only idle data per 4.1.2.7.6.5
        dataFieldStatus |= 0; // Assuming packet starts at beginning

        status = serializer.serialize(dataFieldStatus);
        FW_ASSERT(status == Fw::FW_SERIALIZE_OK, status);

        // Add frame data
        status = serializer.serialize(data, size, true);
        FW_ASSERT(status == Fw::FW_SERIALIZE_OK, status);

        // Add frame error control (CRC-16)
        CheckSum crc;
        Types::CircularBuffer circBuff(buffer.getData(), buffer.getSize());
        FwSizeType size_out;
        crc.calculate(circBuff, totalSize - 2, size_out);
        status = serializer.serialize(crc.getValue());
        FW_ASSERT(status == Fw::FW_SERIALIZE_OK, status);

        buffer.setSize(totalSize);
        m_interface->send(buffer);

        // Update frame counts
        m_masterChannelFrameCount = (m_masterChannelFrameCount + 1) & 0xFF;
        m_virtualChannelFrameCount = (m_virtualChannelFrameCount + 1) & 0xFF;
}
}

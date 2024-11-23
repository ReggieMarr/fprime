// ======================================================================
// \title  TCSpaceDataLink.cpp
// \author Reginald Marr
// \brief  cpp file for TCSpaceDataLinkProtocol class
//
// \copyright
// Copyright 2009-2022, by the California Institute of Technology.
// ALL RIGHTS RESERVED.  United States Government Sponsorship
// acknowledged.
//
// ======================================================================
#include "TCSpaceDataLink.hpp"
#include "FpConfig.h"
#include "FpConfig.hpp"
#include "Fw/Logger/Logger.hpp"
#include "Fw/Types/Assert.hpp"
#include "Fw/Types/Serializable.hpp"
#include "Svc/FrameAccumulator/FrameDetector.hpp"
#include "Svc/FramingProtocol/CCSDSProtocols/CCSDSProtocolDefs.hpp"
#include "Utils/Hash/Hash.hpp"
#include "Utils/Types/CircularBuffer.hpp"

namespace Svc {

void TCSpaceDataLink::frame(const U8* const data, const U32 dataSize, Fw::ComPacket::ComPacketType packet_type) {
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
    FW_ASSERT(!(!(m_config.bypassFlag & 0x01) && m_config.controlCommandFlag & 0x01),
              (m_config.bypassFlag & 0x01 && m_config.controlCommandFlag & 0x01));

    // First octet (MSB)
    firstTwoOctets |= (m_config.version & 0x03) << 14;             // Version Number (2 bits)
    firstTwoOctets |= (m_config.bypassFlag & 0x01) << 13;          // Bypass Flag (1 bit)
    firstTwoOctets |= (m_config.controlCommandFlag & 0x01) << 12;  // Control Command Flag (1 bit)
    firstTwoOctets |= (0 & 0x03) << 10;                            // Reserved Spare (2 bits)
    firstTwoOctets |= (m_config.spacecraftId & 0x3FF);             // Spacecraft ID (10 bits)

    // Serialize the first two octets
    Fw::SerializeStatus status;
    status = serializer.serialize(firstTwoOctets);
    FW_ASSERT(status == Fw::FW_SERIALIZE_OK, status);

    // Next two octets: Virtual Channel ID (6 bits) and Frame Length (10 bits)
    U16 nextTwoOctets = 0;
    // CCSDS 232.0-B-4 4.1.2.6
    nextTwoOctets |= (m_config.virtualChannelId & 0x3F) << 10;  // Virtual Channel ID (6 bits)

    // CCSDS 232.0-B-4 4.1.2.7
    U16 frame_length = transferFrameSize - 1;

    nextTwoOctets |= (frame_length & 0x3FF);  // Frame Length (10 bits)

    status = serializer.serialize(nextTwoOctets);
    FW_ASSERT(status == Fw::FW_SERIALIZE_OK, status);

    // Frame Sequence Number (8 bits)
    status = serializer.serialize(m_config.frameSequence);
    FW_ASSERT(status == Fw::FW_SERIALIZE_OK, status);

    // CCSDS 232.0-B-4 4.1.2.3.2
    bool isTransferFrameFrameDataUnit = (m_config.controlCommandFlag & 0x01) ? false : true;

    // As per CCSDS 232.0-B-4 4.1.3.2.2
    // If the transfer frame is of Type-C (Frame Data Unit)
    // then we need to have a segment header.
    // Segment headers are not currently supported
    FW_ASSERT(!isTransferFrameFrameDataUnit, m_config.controlCommandFlag);

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
    m_config.frameSequence = (m_config.frameSequence + 1) & 0xFF;
}

}  // namespace Svc

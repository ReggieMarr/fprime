// ======================================================================
// \title  SpacePacketProtocol.cpp
// \author Reginald Marr
// \brief  cpp file for SpacePacketProtocol class
//
// \copyright
// Copyright 2009-2022, by the California Institute of Technology.
// ALL RIGHTS RESERVED.  United States Government Sponsorship
// acknowledged.
//
// ======================================================================
#include "SpacePacketProtocol.hpp"
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

void SpacePacketProtocol::frame(const U8* const data, const U32 size, Fw::ComPacket::ComPacketType packet_type) {
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
    firstByte |= (0 & 0x07) << 5;              // Version Number (3 bits)
    firstByte |= (0 & 0x01) << 4;              // Packet Type (1 bit)
    firstByte |= (0 & 0x01) << 3;              // Secondary Header Flag (1 bit)
    firstByte |= (m_config.apid >> 8) & 0x07;  // APID MSB (3 bits)

    // Serialize header
    Fw::SerializeStatus status;
    status = serializer.serialize(firstByte);
    FW_ASSERT(status == Fw::FW_SERIALIZE_OK, status);

    // APID LSB
    status = serializer.serialize(static_cast<U8>(m_config.apid & 0xFF));
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

    m_config.sequenceCount = (m_config.sequenceCount + 1) & 0x3FFF;
}

}  // namespace Svc

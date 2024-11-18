// ======================================================================
// \title  TCSpaceDataLink.hpp
// \author Reginald Marr
// \brief  hpp file for TCSpaceDataLink class
//
// \copyright
// Copyright 2009-2021, by the California Institute of Technology.
// ALL RIGHTS RESERVED.  United States Government Sponsorship
// acknowledged.
//
// ======================================================================

#ifndef SVC_TC_SPACE_DATA_LINK_HPP
#define SVC_TC_SPACE_DATA_LINK_HPP

#include <Svc/FramingProtocol/FramingProtocol.hpp>
#include "Svc/FrameAccumulator/FrameDetector/StartLengthCrcDetector.hpp"
#include "Svc/FramingProtocol/CCSDSProtocols/CCSDSProtocolDefs.hpp"
#include "config/FpConfig.h"

namespace Svc {
namespace FrameDetectors {
constexpr U16 TC_SCID_MASK = 0x03FF;
    static_assert(CCSDS_SCID <= (std::numeric_limits<U16>::max() & TC_SCID_MASK), "SCID must fit in 10bits");

    //! CCSDS framing start word is:
    //! - 2 bits of version number "00"
    //! - 1 bit of bypass flag "0"
    //! - 1 bit of control command flag "0"
    //! - 2 bits of reserved "00"
    //! - 10 bits of configurable SCID
    using TCSpaceDataLinkStartWord = StartToken<U16, static_cast<U16>(0 | (CCSDS_SCID & TC_SCID_MASK)), TC_SCID_MASK>;
    //! CCSDS length is the last 10 bits of the 3rd and 4th octet
    using TCSpaceDataLinkLength = LengthToken<U16, sizeof(U16), CCSDS_SDLTP_TC_MAX_FRAME_LENGTH, TC_SCID_MASK>;
    //! CCSDS checksum is a 16bit CRC which is calculated from the first bit in the transfer frame up to (but not including)
    //! the Frame Error Control field (aka the crc U16 itself) as per CCSDS 232.0-B-4 4.1.4.2
    //! When we perform the calculation during detection we do so by passing the length (found using the Length token)
    //! The Frame Length is set as one fewer than the total octets in the Transfer Frame as per CCSDS 232.0-B-4 4.1.4.2
    //! Therefore if we're calculating a CRC check on a frame which itself includes a CRC check we need to offset by
    //! another 1 fewer than what the frame length describes.
    using TCSpaceDataLinkChecksum = CRC<U16, 0, -1, CRC16_CCITT>;

    //! CCSDS frame detector is a start/length/crc detector using the configured fprime tokens
    using TCSpaceDataLinkDetector = StartLengthCrcDetector<TCSpaceDataLinkStartWord, TCSpaceDataLinkLength, TCSpaceDataLinkChecksum>;
};

class TCSpaceDataLink: public FramingProtocol {
    typedef enum {
        DISABLED = 0,
        ENABLED = 1,
    } BypassFlag;

    typedef enum {
        DATA = 0,
        COMMAND = 1,
    } ControlCommandFlag;

  public:
    typedef struct TCSpaceDataLinkConfig_s {
        U8 version{0};
        U8 bypassFlag{BypassFlag::ENABLED};
        U8 controlCommandFlag{ControlCommandFlag::COMMAND};
        U16 spacecraftId{CCSDS_SCID};
        U8 virtualChannelId{0};
        U8 frameSequence{0};
    } TCSpaceDataLinkConfig_t;

    TCSpaceDataLink() = default;
    explicit TCSpaceDataLink(const TCSpaceDataLinkConfig_t& config) : m_config(config) {}

    void setup(const TCSpaceDataLinkConfig_t& config) { m_config = config; }

    void frame(const U8* const data, const U32 size, Fw::ComPacket::ComPacketType packet_type) override;

  private:
    using TC_CheckSum = FrameDetectors::TCSpaceDataLinkChecksum;
    TCSpaceDataLinkConfig_t m_config;
};

}  // namespace Svc
#endif  // SVC_CCSDS_PROTOCOL_HPP

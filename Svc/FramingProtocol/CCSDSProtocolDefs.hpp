// ======================================================================
// \title  CCSDSProtocolDefs.hpp
// \author Reginald Marr
// \brief  hpp file for CCSDS frame detector definitions
// ======================================================================
#ifndef SVC_CCSDS_PROTOCOL_DEFS
#define SVC_CCSDS_PROTOCOL_DEFS
#include "FpConfig.h"
namespace Svc {
// TC Transfer Frame Structural Components
// +-------------------------+-----------------------+-------------------------+
// | TRANSFER FRAME          | TRANSFER FRAME        | FRAME ERROR CONTROL     |
// | PRIMARY HEADER          | DATA FIELD            | FIELD (Optional)        |
// |                         |                       |                         |
// +-------------------------+-----------------------+-------------------------+
// | 5 octets                | Varies (Up to 1019)   | 2 octets                |
// +-------------------------+-----------------------+-------------------------+

// Telecommand (TC) Transfer Frame Definitions based on CCSDS 232.0-B-4 TC Space Data Link Protocol
constexpr FwSizeType CCSDS_SDLTP_TC_MAX_FRAME_LENGTH = 1017;      // Maximum TC transfer frame data field (CCSDS 232.0-B-4, Section 4.1.3)
constexpr FwSizeType TC_SPACE_DATA_LINK_HEADER_SIZE = 5;          // TC Primary Header: 5 octets (CCSDS 232.0-B-4, Section 4.1.2)
constexpr FwSizeType TC_SPACE_DATA_LINK_TRAILER_SIZE = 2;         // Frame Error Control Field (optional): 2 octets for error detection (CCSDS 232.0-B-4, Section 4.1.4)
constexpr FwSizeType TC_SPACE_DATA_LINK_MAX_TRANSFER_FRAME_SIZE = CCSDS_SDLTP_TC_MAX_FRAME_LENGTH + TC_SPACE_DATA_LINK_HEADER_SIZE
                                                                         + TC_SPACE_DATA_LINK_TRAILER_SIZE;  // Total TC transfer frame size (CCSDS 232.0-B-4)

// TM Transfer Frame Structural Components
// +-------------------------+------------------------+------------------------+-------------------------+-------------------------+
// | TRANSFER FRAME          | TRANSFER FRAME         | TRANSFER FRAME         | TRANSFER FRAME          | TRANSFER FRAME          |
// | PRIMARY HEADER          | SECONDARY HEADER       | DATA FIELD             | TRAILER                 | TRAILER                 |
// |                         | (Optional)             |                        | OPERATIONAL CONTROL     | FRAME ERROR CONTROL     |
// |                         |                        |                        | FIELD (Optional)        | FIELD (Optional)        |
// +-------------------------+------------------------+------------------------+-------------------------+-------------------------+
// | 6 octets                | Up to 64 octets        | Varies                 | 4 octets                | 2 octets                |
// +-------------------------+------------------------+------------------------+-------------------------+-------------------------+

// Telemetry (TM) Transfer Frame Definitions based on CCSDS 132.0-B-3 TM Space Data Link Protocol
constexpr FwSizeType TM_FRAME_PRIMARY_HEADER_SIZE = 6;           // TM Primary Header: 6 octets (CCSDS 132.0-B-3, Section 4.1.2)
constexpr FwSizeType TM_SECONDARY_HEADER_MAX_SIZE = 64;          // TM Secondary Header (optional): Up to 64 octets (CCSDS 132.0-B-3, Section 4.1.3)
constexpr FwSizeType TM_OPERATIONAL_CONTROL_SIZE = 4;            // Operational Control Field (optional): 4 octets (CCSDS 132.0-B-3, Section 4.1.5)
constexpr FwSizeType FRAME_ERROR_CONTROL_SIZE = 2;               // Frame Error Control Field (optional): 2 octets for error detection (CCSDS 132.0-B-3, Section 4.1.6)
constexpr FwSizeType TM_DATA_FIELD_DFLT_SIZE = 1039;             // TM Data Field size varies by application, typically constrained by maximum transfer frame size (CCSDS 132.0-B-2, Section 4.1.3.4)

#define TM_TRANSFER_FRAME_SIZE(DATA_FIELD_SIZE) \
 (DATA_FIELD_SIZE + TM_FRAME_PRIMARY_HEADER_SIZE + TM_SECONDARY_HEADER_MAX_SIZE + TM_OPERATIONAL_CONTROL_SIZE + FRAME_ERROR_CONTROL_SIZE)

// Common Constants
static constexpr U8 SPACE_PACKET_HEADER_SIZE = 6;                // Space Packet Header size (used in both TM and TC)

// Definitions for the CCSDS frame header
namespace CCSDSFrameHeader {

//! Token type for CCSDS frame header
typedef U32 TokenType;

enum {
    //! Header size for CCSDS frame header
    SIZE = sizeof(TokenType) * 2
};

//! The start word for CCSDS framing
const TokenType START_WORD = static_cast<TokenType>(0xdeadbeef);

}  // namespace CCSDSFrameHeader

}  // namespace Svc
#endif  // SVC_CCSDS_PROTOCOL_DEFS

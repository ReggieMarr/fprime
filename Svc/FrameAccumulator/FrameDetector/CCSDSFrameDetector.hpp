// ======================================================================
// \title  CCSDSFrameDetector.hpp
// \author mstarch
// \brief  hpp file for CCSDS frame detector definitions
// ======================================================================
#ifndef SVC_FRAME_ACCUMULATOR_FRAME_DETECTOR_CCSDS_FRAME_DETECTOR
#define SVC_FRAME_ACCUMULATOR_FRAME_DETECTOR_CCSDS_FRAME_DETECTOR
#include "FpConfig.h"
#include "Svc/FrameAccumulator/FrameDetector/StartLengthCrcDetector.hpp"
#include "Svc/FramingProtocol/CCSDSProtocolDefs.hpp"
namespace Svc {
namespace FrameDetectors {

//! \brief CRC16 CCITT implementation
//!
//! CCSDS uses a CRC16 (CCITT) implementation with polynomial 0x11021, initial value of 0xFFFF, and XOR of 0x0000.
//!
class CRC16_CCITT : public CRCWrapper<U16> {
  public:
    // Initial value is 0xFFFF
    CRC16_CCITT() : CRCWrapper<U16>(std::numeric_limits<U16>::max()) {}

    //! \brief update CRC with one new byte
    //!
    //! Update function for CRC taking previous value from member variable and updating it.
    //!
    //! \param new_byte: new byte to add to calculation
    void update(U8 new_byte) override {
        this->m_crc =  static_cast<U16>(update_crc_ccitt(m_crc, new_byte));
    };

    //! \brief finalize and return CRC value
    U16 finalize() override {
        // Specified XOR value is 0x0000
        return this->m_crc ^ static_cast<U16>(0);
    };
};
constexpr U16 TC_SCID_MASK = 0x03FF;
static_assert(CCSDS_SCID <= (std::numeric_limits<U16>::max() & TC_SCID_MASK), "SCID must fit in 10bits");

//! CCSDS framing start word is:
//! - 2 bits of version number "00"
//! - 1 bit of bypass flag "0"
//! - 1 bit of control command flag "0"
//! - 2 bits of reserved "00"
//! - 10 bits of configurable SCID
using CCSDSStartWord = StartToken<U16, static_cast<U16>(0 | (CCSDS_SCID & TC_SCID_MASK)), TC_SCID_MASK>;
//! CCSDS length is the last 10 bits of the 3rd and 4th octet
using CCSDSLength = LengthToken<U16, sizeof(U16), CCSDS_SDLTP_TC_MAX_FRAME_LENGTH, TC_SCID_MASK>;
//! CCSDS checksum is a 16bit CRC which is calculated from the first bit in the transfer frame up to (but not including)
//! the Frame Error Control field (aka the crc U16 itself) as per CCSDS 232.0-B-4 4.1.4.2
//! When we perform the calculation during detection we do so by passing the length (found using the Length token)
//! The Frame Length is set as one fewer than the total octets in the Transfer Frame as per CCSDS 232.0-B-4 4.1.4.2
//! Therefore if we're calculating a CRC check on a frame which itself includes a CRC check we need to offset by
//! another 1 fewer than what the frame length describes.
using CCSDSChecksum = CRC<U16, 0, -1, CRC16_CCITT>;

//! CCSDS frame detector is a start/length/crc detector using the configured fprime tokens
using TCSpaceDataLinkDetector = StartLengthCrcDetector<CCSDSStartWord, CCSDSLength, CCSDSChecksum>;

using TMSpaceDataLinkStartWord = StartToken<U16, static_cast<U16>(0 | (TM_SCID & TM_SCID_MASK)), TM_SCID_MASK>;
using TMSpaceDataLinkLength = LengthToken<FwSizeType, sizeof(FwSizeType), TM_TRANSFER_FRAME_SIZE(TM_DATA_FIELD_DFLT_SIZE), TM_LENGTH_MASK>;
using TMSpaceDataLinkChecksum = CRC<U16, TM_TRANSFER_FRAME_SIZE(TM_DATA_FIELD_DFLT_SIZE), -2, CRC16_CCITT>;
using TMSpaceDataLinkDetector = StartLengthCrcDetector<TMSpaceDataLinkStartWord, TMSpaceDataLinkLength, TMSpaceDataLinkChecksum>;
}  // namespace FrameDetectors
}  // namespace Svc
#endif  // SVC_FRAME_ACCUMULATOR_FRAME_DETECTOR_CCSDS_FRAME_DETECTOR

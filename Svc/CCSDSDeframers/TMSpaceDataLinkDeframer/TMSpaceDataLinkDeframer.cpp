// ======================================================================
// \title  TMSpaceDataLinkDeframer.cpp
// \author chammard
// \brief  cpp file for TMSpaceDataLinkDeframer component implementation class
// ======================================================================

#include "TMSpaceDataLinkDeframer.hpp"
#include "FpConfig.hpp"
#include "Fw/Logger/Logger.hpp"

namespace Svc {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

TMSpaceDataLinkDeframer ::TMSpaceDataLinkDeframer(const char* const compName)
    : TMSpaceDataLinkDeframerComponentBase(compName) {}

TMSpaceDataLinkDeframer ::~TMSpaceDataLinkDeframer() {}

// ----------------------------------------------------------------------
// Handler implementations for user-defined typed input ports
// ----------------------------------------------------------------------
void TMSpaceDataLinkDeframer::processDataFieldStatus(U16 status) {
    // Mission-specific processing of Data Field Status
    // Example implementation:
    bool secondaryHeaderPresent = (status >> 15) & 0x01;
    bool syncFlag = (status >> 14) & 0x01;
    bool packetOrderFlag = (status >> 13) & 0x01;
    U16 firstHeaderPointer = status & 0x7FF;

    // Log status information
    Fw::Logger::log(
        "Data Field Status: SecHdr %d Sync %d Order %d FHP %d",
        secondaryHeaderPresent,
        syncFlag,
        packetOrderFlag,
        firstHeaderPointer
    );
}

void TMSpaceDataLinkDeframer ::framedIn_handler(FwIndexType portNum, Fw::Buffer& data, Fw::Buffer& context) {
    // CCSDS TM Format:
    // 6 octets - TM Primary Header
    // Variable length - Data Field
    // 2 octets - Frame Error Control Field (optional)

    // CCSDS TM Primary Header (CCSDS 132.0-B-3):
    // 12b - XX - Master Channel Identifier
    // 3b  - XX - Virtual Channel Identifier
    // 1b  - XX - Operational Control Field Flag
    // 8b  - XX - Master Channel Frame Count
    // 8b  - XX - Virtual Channel Frame Count
    // 16b - XX - Transfer Frame Data Field Status

    FW_ASSERT(data.getSize() >= TM_SPACE_DATA_LINK_HEADER_SIZE + TM_SPACE_DATA_LINK_TRAILER_SIZE, data.getSize());

    // First two octets contain Master Channel ID, Virtual Channel ID, and OCF Flag
    U16 firstTwoOctets;
    U8 byte1 = data.getData()[0];
    U8 byte2 = data.getData()[1];
    firstTwoOctets = (byte1 << 8) | byte2;

    // Extract fields from first two octets
    U16 master_channel_id = (firstTwoOctets >> 4) & 0x0FFF;  // 12 bits
    U8 virtual_channel_id = (firstTwoOctets >> 1) & 0x07;    // 3 bits
    bool ocf_flag = firstTwoOctets & 0x01;                   // 1 bit

    // Frame counts
    U8 master_frame_count = data.getData()[2];    // 8 bits
    U8 virtual_frame_count = data.getData()[3];   // 8 bits

    // Data Field Status (2 octets)
    U16 data_field_status;
    byte1 = data.getData()[4];
    byte2 = data.getData()[5];
    data_field_status = (byte1 << 8) | byte2;

    // Calculate frame length
    // Note: In TM frames, length might be fixed or derived from Data Field Status
    // This is mission-specific and might need adjustment
    U16 frame_length = data.getSize() - (TM_SPACE_DATA_LINK_HEADER_SIZE + TM_SPACE_DATA_LINK_TRAILER_SIZE);

    // Verify frame size constraints
    FW_ASSERT(frame_length < TM_SPACE_DATA_LINK_MAX_TRANSFER_FRAME_SIZE);

    // Log frame details
    Fw::Logger::log(
        "TM Frame: MC_ID %d VC_ID %d OCF %d MC_CNT %d VC_CNT %d Length %d",
        master_channel_id,
        virtual_channel_id,
        ocf_flag,
        master_frame_count,
        virtual_frame_count,
        frame_length
    );

    // Optional: Process Data Field Status
    // This is mission-specific and depends on how the status field is used
    processDataFieldStatus(data_field_status);

    // Set data buffer to contain only the frame data
    data.setData(data.getData() + TM_SPACE_DATA_LINK_HEADER_SIZE);
    data.setSize(frame_length);

    // Output deframed data
    this->deframedOut_out(0, data, context);
}

}  // namespace Svc

// ======================================================================
// \title  TMSpaceDataLinkDeframer.cpp
// \author chammard
// \brief  cpp file for TMSpaceDataLinkDeframer component implementation class
// ======================================================================

#include "TMSpaceDataLinkDeframer.hpp"
#include "FpConfig.hpp"
#include "Fw/Logger/Logger.hpp"
#include "Svc/FramingProtocol/CCSDSProtocolDefs.hpp"

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

void TMSpaceDataLinkDeframer::framedIn_handler(FwIndexType portNum, Fw::Buffer& data, Fw::Buffer& context) {
    // Verify minimum frame size
    const U32 minFrameSize = TM_SPACE_DATA_LINK_HEADER_SIZE + TM_SPACE_DATA_LINK_TRAILER_SIZE;
    FW_ASSERT(data.getSize() >= minFrameSize, data.getSize());

    Fw::SerializeBufferBase& serializer = data.getSerializeRepr();
    // NOTE this probably shouldn't be required but seems to be at least for a loopback test
    serializer.setBuffLen(data.getSize());
    Fw::SerializeStatus status;

    // Deserialize header components
    U16 firstTwoOctets;
    status = serializer.deserialize(firstTwoOctets);
    FW_ASSERT(status == Fw::FW_SERIALIZE_OK, status);

    // Extract version number, spacecraft ID, virtual channel ID, and OCF flag
    U8 version_number = (firstTwoOctets >> 14) & 0x03;
    U16 spacecraft_id = (firstTwoOctets >> 4) & 0x3FF;
    U8 virtual_channel_id = (firstTwoOctets >> 1) & 0x07;
    bool ocf_flag = firstTwoOctets & 0x01;

    // Deserialize frame counts
    U8 master_frame_count;
    U8 virtual_frame_count;
    status = serializer.deserialize(master_frame_count);
    FW_ASSERT(status == Fw::FW_SERIALIZE_OK, status);
    status = serializer.deserialize(virtual_frame_count);
    FW_ASSERT(status == Fw::FW_SERIALIZE_OK, status);

    // Deserialize data field status
    U16 data_field_status;
    status = serializer.deserialize(data_field_status);
    FW_ASSERT(status == Fw::FW_SERIALIZE_OK, status);

    // Extract data field status components
    bool has_secondary_header = (data_field_status >> 15) & 0x01;
    bool is_vca_sdu = (data_field_status >> 14) & 0x01;
    U8 segment_length_id = (data_field_status >> 11) & 0x03;
    U16 first_header_pointer = data_field_status & 0x7FF;

    // Calculate frame data length
    U32 dataLength = data.getSize() - minFrameSize;

    // Create output buffer for deframed data
    Fw::Buffer outputBuffer = data;
    outputBuffer.setData(data.getData() + TM_SPACE_DATA_LINK_HEADER_SIZE);
    outputBuffer.setSize(dataLength);

    // Log frame details
    Fw::Logger::log(
        "TM Frame: Ver %d, SC_ID %d, VC_ID %d, OCF %d, MC_CNT %d, VC_CNT %d, Len %d\n",
        version_number,
        spacecraft_id,
        virtual_channel_id,
        ocf_flag,
        master_frame_count,
        virtual_frame_count,
        dataLength
    );

    // Output deframed data
    this->deframedOut_out(0, outputBuffer, context);
}

}  // namespace Svc

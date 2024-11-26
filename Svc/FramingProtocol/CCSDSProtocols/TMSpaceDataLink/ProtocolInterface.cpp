// ======================================================================
// \title  TMSpaceDataLink ProtocolInterface.cpp
// \author Reginald Marr
// \brief  Implementation of protocol interface to support TMSpaceDataLink
//
// ======================================================================
#include "ProtocolInterface.hpp"
#include <algorithm>
#include <array>
#include <stdexcept>
#include "FpConfig.h"
#include "FpConfig.hpp"
#include "Fw/Com/ComBuffer.hpp"
#include "Fw/Com/ComPacket.hpp"
#include "Fw/Logger/Logger.hpp"
#include "Fw/Types/Assert.hpp"
#include "Fw/Types/Serializable.hpp"
#include "Svc/FramingProtocol/CCSDSProtocols/CCSDSProtocolDefs.hpp"
#include "Svc/FramingProtocol/CCSDSProtocols/TMSpaceDataLink/Channels.hpp"
#include "Svc/FramingProtocol/CCSDSProtocols/TMSpaceDataLink/ManagedParameters.hpp"
#include "Svc/FramingProtocol/CCSDSProtocols/TMSpaceDataLink/Services.hpp"
#include "TransferFrame.hpp"
#include "Utils/Hash/Hash.hpp"
#include "Utils/Types/CircularBuffer.hpp"

namespace TMSpaceDataLink {

ProtocolEntity::ProtocolEntity(ManagedParameters_t const& params) : m_params(params) {}

bool ProtocolEntity::UserComIn_handler(Fw::Buffer data, U32 context) {
    // Forward data to the appropriate Virtual Channel
    U8 mcId = (context >> 8) & 0xFF;
    U8 vcId = context & 0xFF;
    // TODO should have some sort of routing function here that takes the context
    // and gets the MCID and GVCID
    bool channelStatus = true;
    VirtualChannelParams_t vcParams = m_params.physicalParams.subChannels.at(0).subChannels.at(0);
    MCID_t mcid = {m_params.physicalParams.transferFrameVersion, 0};
    GVCID_t gvcid = {mcid, 0};
    FwSizeType transferFrameSize;
    VirtualChannel virtualChannel(vcParams, transferFrameSize, gvcid);

    // This will transfer through the following services and functions
    // PacketProcessing() (If VCP Service is supported)
    // -> This packetizes the raw buffer according to some registered and well known scheme
    // VCA will packetize the raw buffer/process it in a private manner
    // VirtualChannelGeneration()
    // -> Takes the packets and secondary header and/or operational control field (if services support)
    // and then uses the Virtual Channel Framing Service to create a transfer frame
    // then sends it to the parent master channel via the queue for it to handle
    Fw::ComBuffer com(data.getData(), data.getSize());
    // NOTE this implies that the virutal channel is setup to be synchronous
    // we should support async and periodic with queues as well.
    virtualChannel.transfer(com);

    return channelStatus;
}

void ProtocolEntity::generateNextFrame() {
    // Generate frames for Virtual Channels
    // for (U8 mcId = 0; mcId < 8; mcId++) {
    //     std::array<Fw::Buffer, 8> vcFrames;
    //     for (U8 vcId = 0; vcId < 8; vcId++) {
    //         Fw::Buffer sdu;
    //         m_virtualChannels[mcId][vcId].propogate(sdu);
    //         vcFrames[vcId] = sdu;
    //     }
    //     // Multiplex VC frames into Master Channel
    //     Fw::Buffer mcFrame;
    //     m_masterChannels[mcId].VirtualChannelMultiplexing_handler(vcFrames);
    //     m_masterChannels[mcId].MasterChannelGeneration_handler(mcFrame);
    // }

    // Generate final Physical Channel frames
    Fw::Buffer finalFrame;
}

void ProtocolEntity::generateIdleData(Fw::Buffer& frame) {
    // Generate an idle frame with appropriate First Header Pointer
}

}  // namespace TMSpaceDataLink

namespace Svc {

void TMSpaceDataLinkProtocol::frame(const U8* const data, const U32 size, Fw::ComPacket::ComPacketType packet_type) {
    FW_ASSERT(data != nullptr);
    FW_ASSERT(m_interface != nullptr);

    // Calculate total size including TM frame header
    Fw::Buffer buffer = m_interface->allocate(TM_TRANSFER_FRAME_SIZE(m_dataFieldSize));
    FW_ASSERT(buffer.getSize() == TM_TRANSFER_FRAME_SIZE(m_dataFieldSize), TM_TRANSFER_FRAME_SIZE(m_dataFieldSize));

    Fw::SerializeBufferBase& serializer = buffer.getSerializeRepr();
    Fw::SerializeStatus status;
    // status = m_transferFrame.serialize(serializer, m_transferData, data, size);
    // FW_ASSERT(status == Fw::FW_SERIALIZE_OK, status);

    // buffer.setSize(TM_TRANSFER_FRAME_SIZE(m_dataFieldSize));

    m_interface->send(buffer);

    // Update frame counts
    m_transferData.masterChannelFrameCount = (m_transferData.masterChannelFrameCount + 1) & 0xFF;
    m_transferData.virtualChannelFrameCount = (m_transferData.virtualChannelFrameCount + 1) & 0xFF;
}

}  // namespace Svc
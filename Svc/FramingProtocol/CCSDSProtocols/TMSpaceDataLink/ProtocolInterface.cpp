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
#include "Fw/Buffer/Buffer.hpp"
#include "Fw/Com/ComBuffer.hpp"
#include "Fw/Com/ComPacket.hpp"
#include "Fw/Logger/Logger.hpp"
#include "Fw/Types/Assert.hpp"
#include "Fw/Types/Serializable.hpp"
#include "ManagedParameters.hpp"
#include "ProtocolDataUnits.hpp"
#include "Services.hpp"
#include "Svc/FramingProtocol/CCSDSProtocols/CCSDSProtocolDefs.hpp"
#include "Svc/FramingProtocol/CCSDSProtocols/TMSpaceDataLink/Channels.hpp"
#include "Svc/FramingProtocol/CCSDSProtocols/TMSpaceDataLink/ManagedParameters.hpp"
#include "Svc/FramingProtocol/CCSDSProtocols/TMSpaceDataLink/Services.hpp"
#include "TransferFrame.hpp"
#include "Utils/Hash/Hash.hpp"
#include "Utils/Types/CircularBuffer.hpp"

namespace TMSpaceDataLink {

bool ProtocolEntity::UserComIn_handler(Fw::Buffer& data, U32 context) {
    // Determine the channel mapping from context

    GVCID_t gvcid;
    GVCID_t::fromVal(gvcid, context);

    // This will transfer through the following services and functions
    // PacketProcessing() (If VCP Service is supported)
    // -> This packetizes the raw buffer according to some registered and well known scheme
    // VCA will packetize the raw buffer/process it in a private manner
    // VirtualChannelGeneration()
    // -> Takes the packets and secondary header and/or operational control field (if services support)
    // and then uses the Virtual Channel Framing Service to create a transfer frame
    // then sends it to the parent master channel via the queue for it to handle
    // NOTE this implies that the virutal channel is setup to be synchronous
    // we should support async and periodic with queues as well.
    // Specific implementation for VirtualChannel with no underlying services
    // VirtualChannel vc = m_physicalChannel.getChannel(gvcid);
    // Fw::ComBuffer com(data.getData(), data.getSize());
    // vc.transfer(com);
    VirtualChannel vc(gvcid);
    // std::array<VirtualChannel, 1> vcs {vc};
    std::array<std::reference_wrapper<VirtualChannel>, 1> vcs{std::ref(vc)};
    MasterChannel<1> mc(gvcid.MCID, vcs);

    return true;
}

void ProtocolEntity::generateNextFrame() {
    // Generate Physical Channel frames
    Fw::Buffer finalFrame;
    // m_physicalChannel.transfer();
}

}  // namespace TMSpaceDataLink

namespace Svc {
// TMSpaceDataLinkProtocol::TMSpaceDataLinkProtocol(const TMSpaceDataLink::MissionPhaseParameters_t& missionParams)
// :   {
//     m_transferFrame = TMSpaceDataLink::FPrimeTransferFrame(missionParams, Fw::Buffer(m_dataFieldBuffer.data(),
//     m_dataFieldBuffer.size()));
// }

void TMSpaceDataLinkProtocol::frame(const U8* const data, const U32 size, Fw::ComPacket::ComPacketType packet_type) {
    // FW_ASSERT(data != nullptr);
    // FW_ASSERT(m_interface != nullptr);

    // // Calculate total size including TM frame header
    // Fw::Buffer buffer = m_interface->allocate(TM_TRANSFER_FRAME_SIZE(m_dataFieldSize));
    // FW_ASSERT(buffer.getSize() == TM_TRANSFER_FRAME_SIZE(m_dataFieldSize), TM_TRANSFER_FRAME_SIZE(m_dataFieldSize));

    // Fw::SerializeBufferBase& serializer = buffer.getSerializeRepr();
    // Fw::SerializeStatus status;
    // // status = m_transferFrame.serialize(serializer, m_transferData, data, size);
    // // FW_ASSERT(status == Fw::FW_SERIALIZE_OK, status);

    // // buffer.setSize(TM_TRANSFER_FRAME_SIZE(m_dataFieldSize));

    // m_interface->send(buffer);

    // // Update frame counts
    // m_transferData.masterChannelFrameCount = (m_transferData.masterChannelFrameCount + 1) & 0xFF;
    // m_transferData.virtualChannelFrameCount = (m_transferData.virtualChannelFrameCount + 1) & 0xFF;
}

}  // namespace Svc

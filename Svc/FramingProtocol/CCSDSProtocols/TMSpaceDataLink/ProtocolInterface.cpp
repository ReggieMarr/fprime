// ======================================================================
// \title  TMSpaceDataLink ProtocolInterface.cpp
// \author Reginald Marr
// \brief  Implementation of protocol interface to support TMSpaceDataLink
//
// ======================================================================
#include "ProtocolInterface.hpp"
#include <algorithm>
#include <array>
#include <cstring>
#include <stdexcept>
#include "FpConfig.h"
#include "FpConfig.hpp"
#include "Fw/Buffer/Buffer.hpp"
#include "Fw/Com/ComBuffer.hpp"
#include "Fw/Com/ComPacket.hpp"
#include "Fw/Logger/Logger.hpp"
#include "Fw/Types/Assert.hpp"
#include "Fw/Types/Serializable.hpp"
#include "Fw/Types/String.hpp"
#include "ManagedParameters.hpp"
#include "ProtocolDataUnits.hpp"
#include "Services.hpp"
#include "Svc/FramingProtocol/CCSDSProtocols/CCSDSProtocolDefs.hpp"
#include "Svc/FramingProtocol/CCSDSProtocols/TMSpaceDataLink/Channels.hpp"
#include "Svc/FramingProtocol/CCSDSProtocols/TMSpaceDataLink/ManagedParameters.hpp"
#include "Svc/FramingProtocol/CCSDSProtocols/TMSpaceDataLink/Services.hpp"
#include "Svc/FramingProtocol/CCSDSProtocols/TMSpaceDataLink/TransferFrameDefs.hpp"
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

    // NOTE we should get this via a physical channel getter
    MasterChannel<NUM_VIRTUAL_CHANNELS>& mc = m_physicalChannel.m_subChannels.at(0);
    Fw::Logger::log("transfering for %d \n", gvcid.VCID);

    VirtualChannel& vc = mc.m_subChannels.at(gvcid.VCID);
    vc.transfer(data);

    return true;
}

void ProtocolEntity::generateNextFrame(Fw::Buffer& nextFrameBuffer) {
    // Generate Physical Channel frames
    std::nullptr_t null_arg = nullptr;

    m_physicalChannel.transfer(null_arg);
    Fw::SerializeBufferBase& serBuff = nextFrameBuffer.getSerializeRepr();

    m_physicalChannel.popFrameBuff(serBuff);
    Fw::Logger::log("\nGot ser buff \n");
    NATIVE_UINT_TYPE idx = 0;
    Fw::Logger::log("Exiting Frame Buff: \n");
    for (NATIVE_UINT_TYPE i = 0; i < serBuff.getBuffLength(); i++) {
        Fw::Logger::log("%02x ", serBuff.getBuffAddr()[i]);
    }
    Fw::Logger::log("\n");

    FPrimeTransferFrame frame;
    frame.extract(serBuff);
    PrimaryHeaderControlInfo_t ci;
    frame.primaryHeader.set(ci);
    U16 testVal = ((static_cast<U8>(ci.dataFieldStatus.isPacketOrdered) << 13) & 0x1) |
                  ((ci.dataFieldStatus.segmentLengthId << 11) & 0x7) | (ci.dataFieldStatus.firstHeaderPointer & 0x7FF);
    Fw::Logger::log("Got frame.\n SCID %d, VCID %d, VC Count %d MC Count %d testVal 0x%04X", ci.spacecraftId,
                    ci.virtualChannelId, ci.virtualChannelFrameCount, ci.masterChannelFrameCount, testVal);
    std::array<U8, frame.dataField.SERIALIZED_SIZE> dataField;
    frame.dataField.get(dataField);

}

}  // namespace TMSpaceDataLink

namespace Svc {
TMSpaceDataLinkProtocol::TMSpaceDataLinkProtocol(TMSpaceDataLink::ManagedParameters_t const& params)
    : m_params(params), m_tmSpaceLink(m_params) {}

void TMSpaceDataLinkProtocol::frame(const U8* const data, const U32 size, Fw::ComPacket::ComPacketType packet_type) {
    FW_ASSERT(data != nullptr);
    FW_ASSERT(m_interface != nullptr);
    constexpr FwSizeType dataFieldSize = TMSpaceDataLink::FPrimeTransferFrame::DataField_t::Base::SERIALIZED_SIZE;
    FW_ASSERT(size <= dataFieldSize, size, dataFieldSize);

    constexpr FwSizeType transferFrameSize = TMSpaceDataLink::FPrimeTransferFrame::SERIALIZED_SIZE;

    Fw::Buffer sendBuffer = m_interface->allocate(transferFrameSize);
    FW_ASSERT(sendBuffer.getSize() == transferFrameSize, transferFrameSize);

    // TODO remove this after we have good getters and param propogation
    TMSpaceDataLink::MCID_t mcid = {
        .SCID = m_params.physicalParams.subChannels.at(0).spaceCraftId,
        .TFVN = m_params.physicalParams.transferFrameVersion,
    };
    TMSpaceDataLink::GVCID_t gvcid = {
        .MCID = mcid,
        .VCID = 0,
    };
    U32 context;
    TMSpaceDataLink::GVCID_t::fromVal(gvcid, context);

    // NOTE this copy is just needed at the momemnt since the FramingProtocol interface requires data to be const
    // TODO remove after fixing that and better handling memory through the TM data link pipeline
    std::array<U8, dataFieldSize> tmpDataBuff;
    std::memcpy(tmpDataBuff.data(), data, size);
    Fw::Buffer dataBuff(tmpDataBuff.data(), tmpDataBuff.size());
    m_tmSpaceLink.UserComIn_handler(dataBuff, context);

    m_tmSpaceLink.generateNextFrame(sendBuffer);

    m_interface->send(sendBuffer);
}

}  // namespace Svc

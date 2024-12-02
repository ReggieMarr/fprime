// ======================================================================
// \title  TMSpaceDataLink ProtocolInterface.cpp
// \author Reginald Marr
// \brief  Implementation of protocol interface to support TMSpaceDataLink
//         This includes the implementation a protocol entity which based off
//         of CCSDS 132.0-B-3 TM Space Data Link Protocol
//
// This implementation follows the protocol entity specification from section 2.3
// of CCSDS 132.0-B-3, implementing the required functions while explicitly
// excluding those listed as not performed by the protocol entity.
//
// \copyright
// Copyright 2009-2021, by the California Institute of Technology.
// ALL RIGHTS RESERVED.  United States Government Sponsorship
// acknowledged.
//
// ======================================================================

#ifndef SVC_TM_SPACE_DATA_LINK_PROTOCOL_HPP
#define SVC_TM_SPACE_DATA_LINK_PROTOCOL_HPP

#include <Svc/FramingProtocol/FramingProtocol.hpp>
#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <queue>
#include <vector>
#include "Channels.hpp"
#include "Fw/Com/ComBuffer.hpp"
#include "Fw/Types/SerialStatusEnumAc.hpp"
#include "Fw/Types/Serializable.hpp"
#include "ManagedParameters.hpp"
#include "Os/Queue.hpp"
#include "ProtocolDataUnits.hpp"
#include "Svc/FrameAccumulator/FrameDetector/StartLengthCrcDetector.hpp"
#include "Svc/FramingProtocol/CCSDSProtocols/CCSDSProtocolDefs.hpp"
#include "TransferFrame.hpp"
#include "config/FpConfig.h"

namespace TMSpaceDataLink {

/**
 * @class ProtocolEntity
 * @brief Implementation of the TM Space Data Link Protocol Entity
 *
 * This class implements the protocol functions specified in CCSDS 132.0-B-3
 * Section 2.3.1. It handles the generation and processing of protocol data units
 * (TM Transfer Frames) at a constant rate.
 */
class ProtocolEntity {
  public:
    ProtocolEntity(ManagedParameters_t& params)
        : m_params(params), m_physicalChannel(createPhysicalChannel(params.physicalParams)) {}

    // Process incoming telemetry data
    bool UserComIn_handler(Fw::Buffer& data, U32 context);

    // Generate the next TM Transfer Frame for transmission
    // Implements constant rate transfer requirement from 2.3.1
    void generateNextFrame(Fw::Buffer& nextFrameBuffer);

    SinglePhysicalChannel m_physicalChannel;

  private:
    // NOTE could be made as a deserializer
    ManagedParameters_t m_params;

    static SinglePhysicalChannel createPhysicalChannel(PhysicalChannelParams_t& params) {
        MCID_t mcid = {
            .SCID = params.subChannels.at(0).spaceCraftId,
            .TFVN = params.transferFrameVersion,
        };
        GVCID_t gvcidPrimary = {
            .MCID = mcid,
            .VCID = 0,
        };
        GVCID_t gvcidSecondary = {
            .MCID = mcid,
            .VCID = 1,
        };
        VirtualChannel vcPrimary(gvcidPrimary);
        VirtualChannel vcSecondary(gvcidSecondary);
        std::array<VirtualChannel, 2> vcs{vcPrimary, vcSecondary};
        MasterChannel<NUM_VIRTUAL_CHANNELS> mc(gvcidPrimary.MCID, vcs);
        std::array<MasterChannel<NUM_VIRTUAL_CHANNELS>, 1> mcs{mc};
        Fw::String pcName("Physical Channel");
        PhysicalChannel<1> pc(pcName, mcs);
        return pc;
    }
};

}  // namespace TMSpaceDataLink

namespace Svc {
class TMSpaceDataLinkProtocol : public FramingProtocol {
  public:
    TMSpaceDataLinkProtocol(TMSpaceDataLink::ManagedParameters_t const& params);

    void setup(const TMSpaceDataLink::MissionPhaseParameters_t& missionParams, const FwSizeType dataFieldSize);

    void frame(const U8* const data, const U32 size, Fw::ComPacket::ComPacketType packet_type) override;

  private:
    std::array<U8, TMSpaceDataLink::FPrimeTransferFrame::SERIALIZED_SIZE> m_dataFieldBuffer;
    TMSpaceDataLink::ManagedParameters_t m_params;
    TMSpaceDataLink::ProtocolEntity m_tmSpaceLink;
    const FwSizeType m_dataFieldSize{Svc::TM_DATA_FIELD_DFLT_SIZE};
};
}  // namespace Svc
#endif  // SVC_TM_SPACE_DATA_LINK_PROTOCOL_HPP

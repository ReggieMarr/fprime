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
    static constexpr FwSizeType TransferFrameSize = 255;
  public:
    ProtocolEntity(ManagedParameters_t& params) : m_physicalChannel(params.physicalParams), m_params(params) {}

    // Process incoming telemetry data
    bool UserComIn_handler(Fw::Buffer data, U32 context);

    // Generate the next TM Transfer Frame for transmission
    // Implements constant rate transfer requirement from 2.3.1
    void generateNextFrame();

  private:
    // NOTE could be made as a deserializer
    ManagedParameters_t m_params;

    PhysicalChannel<TransferFrameSize> m_physicalChannel;
};

}  // namespace TMSpaceDataLink

namespace Svc {
class TMSpaceDataLinkProtocol : public FramingProtocol {
  public:
    TMSpaceDataLinkProtocol(const TMSpaceDataLink::MissionPhaseParameters_t& missionParams,
                            const FwSizeType dataFieldSize)
        : m_transferFrame(TMSpaceDataLink::TransferFrame<>(missionParams)), m_dataFieldSize(dataFieldSize) {}

    void setup(const TMSpaceDataLink::MissionPhaseParameters_t& missionParams, const FwSizeType dataFieldSize);

    void frame(const U8* const data, const U32 size, Fw::ComPacket::ComPacketType packet_type) override;

  private:
    TMSpaceDataLink::TransferData_t m_transferData = {0};
    TMSpaceDataLink::TransferFrame<> m_transferFrame;
    const FwSizeType m_dataFieldSize{Svc::TM_DATA_FIELD_DFLT_SIZE};
};
}  // namespace Svc
#endif  // SVC_TM_SPACE_DATA_LINK_PROTOCOL_HPP

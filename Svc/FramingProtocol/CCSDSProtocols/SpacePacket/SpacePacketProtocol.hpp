// ======================================================================
// \title  SpacePacketProtocol.hpp
// \author Reginald Marr
// \brief  hpp file for SpacePacketProtocol class
//
// \copyright
// Copyright 2009-2021, by the California Institute of Technology.
// ALL RIGHTS RESERVED.  United States Government Sponsorship
// acknowledged.
//
// ======================================================================

#ifndef SVC_SPACE_PACKET_PROTOCOL_HPP
#define SVC_SPACE_PACKET_PROTOCOL_HPP

#include <Svc/FramingProtocol/FramingProtocol.hpp>
#include "Svc/FramingProtocol/CCSDSProtocols/CCSDSProtocolDefs.hpp"
#include "config/FpConfig.h"

namespace Svc {

class SpacePacketProtocol : public FramingProtocol {
  public:
    typedef struct SpacePacketConfig_s {
        U16 apid{0};
        U16 sequenceCount{0};
    } SpacePacketConfig_t;

    SpacePacketProtocol() = default;
    explicit SpacePacketProtocol(const SpacePacketConfig_t& config) : m_config(config) {}

    void setup(const SpacePacketConfig_t& config) { m_config = config; }

    void frame(const U8* const data, const U32 size, Fw::ComPacket::ComPacketType packet_type) override;

  private:
    SpacePacketConfig_t m_config;
};


}  // namespace Svc
#endif  // SVC_SPACE_PACKET_PROTOCOL_HPP

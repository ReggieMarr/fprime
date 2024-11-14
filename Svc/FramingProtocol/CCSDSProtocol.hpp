// ======================================================================
// \title  CCSDSProtocol.hpp
// \author Reginald Marr
// \brief  hpp file for CCSDSProtocol class
//
// \copyright
// Copyright 2009-2021, by the California Institute of Technology.
// ALL RIGHTS RESERVED.  United States Government Sponsorship
// acknowledged.
//
// ======================================================================

#ifndef SVC_CCSDS_PROTOCOL_HPP
#define SVC_CCSDS_PROTOCOL_HPP

#include <Svc/FramingProtocol/FramingProtocol.hpp>
#include "Svc/FrameAccumulator/FrameDetector/CCSDSFrameDetector.hpp"
#include "Svc/FramingProtocol/CCSDSProtocolDefs.hpp"
#include "config/FpConfig.h"

namespace Svc {

class SpacePacketFraming : public FramingProtocol {
  public:
    typedef struct SpacePacketConfig_s {
        U16 apid{0};
        U16 sequenceCount{0};
    } SpacePacketConfig_t;

    SpacePacketFraming() = default;
    explicit SpacePacketFraming(const SpacePacketConfig_t& config) : m_config(config) {}

    void setup(const SpacePacketConfig_t& config) { m_config = config; }

    void frame(const U8* const data, const U32 size, Fw::ComPacket::ComPacketType packet_type) override;

  private:
    SpacePacketConfig_t m_config;
};

class TCSpaceDataLinkFraming : public FramingProtocol {
    typedef enum {
        DISABLED = 0,
        ENABLED = 1,
    } BypassFlag;

    typedef enum {
        DATA = 0,
        COMMAND = 1,
    } ControlCommandFlag;

  public:
    typedef struct TCSpaceDataLinkConfig_s {
        U8 version{0};
        U8 bypassFlag{BypassFlag::ENABLED};
        U8 controlCommandFlag{ControlCommandFlag::COMMAND};
        U16 spacecraftId{CCSDS_SCID};
        U8 virtualChannelId{0};
        U8 frameSequence{0};
    } TCSpaceDataLinkConfig_t;

    TCSpaceDataLinkFraming() = default;
    explicit TCSpaceDataLinkFraming(const TCSpaceDataLinkConfig_t& config) : m_config(config) {}

    void setup(const TCSpaceDataLinkConfig_t& config) { m_config = config; }

    void frame(const U8* const data, const U32 size, Fw::ComPacket::ComPacketType packet_type) override;

  private:
    using TC_CheckSum = FrameDetectors::CCSDSChecksum;
    TCSpaceDataLinkConfig_t m_config;
};

class TMSpaceDataLinkFraming : public FramingProtocol {
    typedef enum {
        DISABLED = 0,
        ENABLED = 1,
    } BypassFlag;

    typedef enum {
        COMMAND = 0,
        DATA = 1,
    } ControlCommandFlag;

  public:
    typedef struct TMSpaceDataLinkConfig_s {
        FwSizeType tmDataFieldSize{TM_DATA_FIELD_DFLT_SIZE};
        U8 tfVersionNumber{0};
        U16 spacecraftId{CCSDS_SCID};
        U8 virtualChannelId{0};
        bool operationalControlFlag{false};
        U8 masterChannelFrameCount{0};
        U8 virtualChannelFrameCount{0};
        bool hasSecondaryHeader{false};
        bool isVcaSdu{false};
    } TMSpaceDataLinkConfig_t;

    TMSpaceDataLinkFraming() = default;
    explicit TMSpaceDataLinkFraming(const TMSpaceDataLinkConfig_t& config) : m_config(config) {}

    void setup(const TMSpaceDataLinkConfig_t& config) { m_config = config; }

    void frame(const U8* const data, const U32 size, Fw::ComPacket::ComPacketType packet_type) override;

  private:
    using CheckSum = FrameDetectors::CCSDSChecksum;
    TMSpaceDataLinkConfig_t m_config;
};

}  // namespace Svc
#endif  // SVC_CCSDS_PROTOCOL_HPP

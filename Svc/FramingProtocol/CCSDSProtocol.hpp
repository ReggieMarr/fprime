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

// NOTE should this be namespaced to Fw ?
// class DataFieldStatus : public Fw::SerializeBufferBase {
//     public:

//         DataFieldStatus(const U8 *args, NATIVE_UINT_TYPE size);
//         virtual ~DataFieldStatus();
//         DataFieldStatus& operator=(const DataFieldStatus& other);

//         NATIVE_UINT_TYPE getBuffCapacity() const; // !< returns capacity, not current size, of buffer
//         U8* getBuffAddr();
//         const U8* getBuffAddr() const;

//     private:
//         Fw::SerializeStatus serializeBase(Fw::SerializeBufferBase& buffer) const ; // called by derived classes to serialize common fields
//         Fw::SerializeStatus deserializeBase(Fw::SerializeBufferBase& buffer); // called by derived classes to deserialize common fields
// };

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
    using CheckSum = FrameDetectors::TMSpaceDataLinkChecksum;
    TMSpaceDataLinkConfig_t m_config;
};

class TMDataFieldFraming : public FramingProtocol {
public:
    typedef struct TMDataFieldConfig_s {
        FwSizeType size{TM_DATA_FIELD_DFLT_SIZE};
        bool hasSecondaryHeader{false};
        U8 secondaryHeaderVersion{0};      // Fixed to '00' per 4.1.3.2.2.2
        U8 secondaryHeaderLength{0};       // Length minus 1, per 4.1.3.2.3.2
        U8* secondaryHeaderData{nullptr};  // Secondary header data if present
        bool isVcaSdu{false};             // Per 4.1.4.3 and 4.1.4.4
        bool allowIdleData{true};         // Controls if OID frames are allowed
    } TMDataFieldConfig_t;

    // TMDataFieldFraming() = default;
    explicit TMDataFieldFraming(const TMDataFieldConfig_t& config);
    void setup(const TMDataFieldConfig_t& config);
    void frame(const U8* const data, const U32 size,
               Fw::ComPacket::ComPacketType packet_type) override;

private:
    // Should we not just describe a frame as serializable or deserializable ?
    void frameSecondaryHeader(Fw::SerializeBufferBase& serializer);
    void frameDataField(const U8* const data, const U32 size, U8* buffer, U32& offset);
    void generateIdleData(U8* buffer, U32 size);

    TMDataFieldConfig_t m_config;
    U32 m_lfsr_state{0xFFFFFFFF};  // Initial state for Fibonacci LFSR
};

}  // namespace Svc
#endif  // SVC_CCSDS_PROTOCOL_HPP

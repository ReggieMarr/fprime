// ======================================================================
// \title  ManagedParameters.hpp
// \author Reginald Marr
// \brief  hpp file for TMSpaceDataLink class
//
// \copyright
// Copyright 2009-2021, by the California Institute of Technology.
// ALL RIGHTS RESERVED.  United States Government Sponsorship
// acknowledged.
//
// ======================================================================

#ifndef TM_SPACE_DATA_LINK_MANAGED_PARAMETERS_HPP
#define TM_SPACE_DATA_LINK_MANAGED_PARAMETERS_HPP

#include <array>
#include "FpConfig.h"
#include "Fw/Types/String.hpp"

// Based on the definition of MCID addressing we infer that we can only
// have one master channel per physical channel
constexpr FwSizeType MAX_MASTER_CHANNELS = 1;
// Per Master Channel
constexpr FwSizeType MAX_VIRTUAL_CHANNELS = 8;
// FIXME See the SANA Packet Version Number registry @ https://sanaregistry.org/r/packet_version_number/
constexpr FwSizeType NUM_SUPPORTED_PACKET_VERSIONS = 1;

typedef enum MC_MUX_TYPE {
    MC_MUX_TIME_DIVSION = 0,
} MC_MUX_TYPE;

typedef enum VC_MUX_TYPE {
    VC_MUX_TIME_DIVSION = 0,
} VC_MUX_TYPE;

namespace TMSpaceDataLink {

// CCSDS 132.0-B-3 5.4
typedef struct PacketTransferParams_s {
    std::array<NATIVE_UINT_TYPE, NUM_SUPPORTED_PACKET_VERSIONS> supportedPacketVersions;
    FwSizeType maxPacketLengthOctets;
    bool isIncompletePacketTransmissionRequired;
} PacketTransferParams_t;

// CCSDS 132.0-B-3 5.3
typedef struct VirtualChannelParams_s {
    // NOTE Id's must be between 0 and 7
    U8 virtualChannelId;
    // If used != 0, 0 indicates this channel uses the VCP Service.
    FwSizeType VCA_SDULength;
    // Valid lengths are from 2 to 64, 0 indicates its not present
    U8 VC_FSHLength;
    // Is the Virtual Channel Operational Control Field present
    bool isVC_OCFPresent;
} VirtualChannelParams_t;

// CCSDS 132.0-B-3 5.2
typedef struct MasterChannelParams_s {
    U16 spaceCraftId;
    // NOTE Id's must be between 0 and 7
    // REVIEW varies a bit from spec by implying the valid virtualchannels
    FwSizeType numSubChannels;
    std::array<VirtualChannelParams_t, MAX_VIRTUAL_CHANNELS> subChannels;
    // NOTE should this be a function pointer instead ?
    VC_MUX_TYPE vcMuxScheme;
    // Valid lengths are from 2 to 64, 0 indicates its not present
    U8 MC_FSHLength;
    // Is the Master Channel Operational Control Field present
    bool isMC_OCFPresent;
} MasterChannelParams_t;

// CCSDS 132.0-B-3 5.1
typedef struct PhysicalChannelParams_s {
    Fw::String channelName;
    FwSizeType transferFrameSize;
    U8 transferFrameVersion;
    // NOTE we could restructure things to imply this better
    // maybe bring in span ?
    FwSizeType numSubChannels;
    std::array<MasterChannelParams_t, MAX_VIRTUAL_CHANNELS> subChannels;
    // NOTE should this be a function pointer instead ?
    MC_MUX_TYPE mcMuxScheme;
    bool isFrameErrorControlEnabled;
} PhysicalChannelParams_t;

typedef struct ManagedParameters_s {
    PhysicalChannelParams_t physicalParams;
} ManagedParameters_t;

}  // namespace TMSpaceDataLink
namespace Fw {

class GlobalVirtualChannelId : public Serializable {
  public:
    GlobalVirtualChannelId();

    // Addresses derived from CCSDS 132.0-B-3 2.1.3
    // Master Channel Identifier (MCID)
    typedef struct MCID_s {
        // Spacecraft Identifier (10 bits allocated)
        U16 SCID : 10;
        // Transfer Version Number (2 bits)
        U8 TFVN : 2;
    } __attribute__((packed)) MCID_t;

    // Global Virtual Channel Identifier (GVCID)
    typedef struct GVCID_s {
        // Master Channel Identifier (MCID)
        MCID_t MCID;
        // Virtual Channel Identifier (VCID) (3 bits)
        U8 VCID : 3;
    } __attribute__((packed)) GVCID_t;

    enum { SERIALIZED_SIZE = sizeof(GVCID_t) };

    static constexpr U16 MAX_SCID = 0x3FF;  // 10 bits max (0b1111111111)
    static constexpr U8 MAX_TFVN = 0x3;     // 2 bits max  (0b11)
    static constexpr U8 MAX_VCID = 0x7;     // 3 bits max  (0b111)

    // Bit positions in 32-bit word
    static constexpr U8 VCID_OFFSET = 0;
    static constexpr U8 TFVN_OFFSET = 3;
    static constexpr U8 SCID_OFFSET = 5;

    // Bit masks for extraction
    static constexpr U32 VCID_MASK = 0x7;    // 0b111
    static constexpr U32 TFVN_MASK = 0x3;    // 0b11
    static constexpr U32 SCID_MASK = 0x3FF;  // 0b1111111111

    GVCID_t getId() { return m_id; };
            SerializeStatus serialize(SerializeBufferBase& buffer) const; // !< Serialize method
            SerializeStatus deserialize(SerializeBufferBase& buffer); // !< Deserialize method

  private:
    GVCID_t m_id;
};
}  // namespace Fw

#endif  // TM_SPACE_DATA_LINK_MANAGED_PARAMETERS_HPP

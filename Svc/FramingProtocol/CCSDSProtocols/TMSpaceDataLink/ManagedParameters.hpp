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
#include "Fw/Types/Assert.hpp"
#include "Fw/Types/SerialBuffer.hpp"
#include "Fw/Types/String.hpp"

// Based on the definition of MCID addressing we infer that we can only
// have one master channel per physical channel
constexpr FwSizeType MAX_MASTER_CHANNELS = 1;
// Per Master Channel
constexpr FwSizeType MAX_VIRTUAL_CHANNELS = 1;
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

// clang-format off
// The Global Virtual Channel Id can be found in the first 2 octets
// of a TM Space Data Link Transfer Frame as shown here
// CCSDS 132.0-B-3 4.1.2.7 Transfer Frame Primary Header (48 bits)
// +--------+------------+-----+---+
// | Global Virtual Channel ID |OCF|
// |                           |(1)|
// +--------+------------+-----+---+
// |  MASTER CHANNEL ID  | VCID|OCF|
// |                     | (3) |(1)|
// +--------+------------+-----+---+
// |Version |  Spacecraft| VCID|OCF|
// |  (2)   |    ID      | (3) |(1)|
// |        |   (10)     |     |   |
// |        |            |     |   |
// +--------+------------+-----+---+
// |        2 Octets               |
// clang-format on
// The OCF field is not conceptually related to the other fields
// which make up the GVCID however it needs to be accounted for in
// unpacking and packing buffers containing the GVCID
static constexpr FwSizeType OCF_BIT_FIELD_SIZE = 1;
static constexpr FwSizeType SCID_BIT_FIELD_SIZE = 10;
static constexpr FwSizeType TFVN_BIT_FIELD_SIZE = 2;
static constexpr FwSizeType MCID_BIT_FIELD_SIZE = SCID_BIT_FIELD_SIZE + TFVN_BIT_FIELD_SIZE;

static constexpr FwSizeType VCID_BIT_FIELD_SIZE = 3;

// NOTE the size of GVCID_t fields is 15 bits however
// in order to retain the position of fields such as they would be
// in a transfer frame we must account for the 1 bit OCF field
static constexpr FwSizeType GVCID_BIT_FIELD_SIZE = MCID_BIT_FIELD_SIZE + VCID_BIT_FIELD_SIZE;

// Addresses derived from CCSDS 132.0-B-3 2.1.3
// Master Channel Identifier (MCID)
typedef struct MCID_s {
    // Spacecraft Identifier (10 bits allocated)
    U16 SCID : SCID_BIT_FIELD_SIZE;
    // Transfer Version Number (2 bits)
    U8 TFVN : TFVN_BIT_FIELD_SIZE;

    bool operator==(const MCID_s& other) const { return SCID == other.SCID && TFVN == other.TFVN; }
} MCID_t;

typedef struct GVCID_s {
    // Master Channel Identifier (MCID)
    MCID_t MCID;
    // Virtual Channel Identifier (VCID) (3 bits)
    U8 VCID : VCID_BIT_FIELD_SIZE;

    bool operator==(const GVCID_s& other) const { return MCID == other.MCID && VCID == other.VCID; }

    // represents the size in bytes
    static constexpr FwSizeType SIZE = sizeof(U16);

    // Maximum values
    static constexpr U16 MAX_SCID = 0x3FF;  // 10 bits max
    static constexpr U8 MAX_TFVN = 0x3;     // 2 bits max
    static constexpr U8 MAX_VCID = 0x7;     // 3 bits max

    // Bit positions in word
    static constexpr U16 VCID_OFFSET = OCF_BIT_FIELD_SIZE;
    static constexpr U16 SCID_OFFSET = VCID_OFFSET + VCID_BIT_FIELD_SIZE;
    static constexpr U16 TFVN_OFFSET = SCID_OFFSET + SCID_BIT_FIELD_SIZE;
    static constexpr U16 GVCID_OFFSET = TFVN_OFFSET + TFVN_BIT_FIELD_SIZE;

    // Bit masks for extraction
    static constexpr U32 VCID_MASK = 0x7;    // 0b111
    static constexpr U32 TFVN_MASK = 0x3;    // 0b11
    static constexpr U32 SCID_MASK = 0x3FF;  // 0b1111111111

    // Conversion functions now as static members
    static void toVal(GVCID_s const& gvcid, U16& val) {
        val |= (static_cast<U16>(gvcid.MCID.SCID) << SCID_OFFSET);
        val |= (static_cast<U16>(gvcid.MCID.TFVN) << TFVN_OFFSET);
        val |= (static_cast<U16>(gvcid.VCID) << VCID_OFFSET);
    }

    static void fromVal(GVCID_s& gvcid, U16 const val) {
        U16 scid = (val >> SCID_OFFSET) & SCID_MASK;
        U8 tfvn = (val >> TFVN_OFFSET) & TFVN_MASK;
        U8 vcid = (val >> VCID_OFFSET) & VCID_MASK;

        FW_ASSERT(scid <= MAX_SCID, scid);
        FW_ASSERT(tfvn <= MAX_TFVN, tfvn);
        FW_ASSERT(vcid <= MAX_VCID, vcid);

        gvcid.MCID.SCID = scid;
        gvcid.MCID.TFVN = tfvn;
        gvcid.VCID = vcid;
    }

    static void toVal(GVCID_s const& gvcid, U32& val) {
        U16 valU16 = 0;
        toVal(gvcid, valU16);
        val = static_cast<U32>(valU16) << GVCID_OFFSET;
    }

    static void fromVal(GVCID_s& gvcid, U32 const val) {
        U16 valU16 = static_cast<U16>(val >> GVCID_OFFSET);
        fromVal(gvcid, valU16);
    }
} GVCID_t;

}  // namespace TMSpaceDataLink

#endif  // TM_SPACE_DATA_LINK_MANAGED_PARAMETERS_HPP

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

// Addresses derived from CCSDS 132.0-B-3 2.1.3
// Master Channel Identifier (MCID)
typedef struct MCID_s {
    // Spacecraft Identifier
    NATIVE_UINT_TYPE SCID;
    // Transfer Version Number
    NATIVE_UINT_TYPE TFVN;
} MCID_t;

// Global Virtual Channel Identifier (GVCID)
typedef struct GVCID_s {
    // Master Channel Identifier (MCID)
    MCID_t MCID;
    // Virtual Channel Identifier (VCID)
    NATIVE_UINT_TYPE VCID;
} GVCID_t;

#endif  // TM_SPACE_DATA_LINK_MANAGED_PARAMETERS_HPP

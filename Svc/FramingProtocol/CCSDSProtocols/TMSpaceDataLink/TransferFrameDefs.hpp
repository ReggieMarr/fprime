#ifndef TRANSFER_FRAME_DEFS_H_
#define TRANSFER_FRAME_DEFS_H_
#include <array>
#include "FpConfig.h"
#include "Fw/Types/Assert.hpp"
#include "Fw/Types/SerialBuffer.hpp"
#include "Fw/Types/String.hpp"
#include "ManagedParameters.hpp"

namespace TMSpaceDataLink {

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

    bool operator==(const MCID_s& other) const;
} MCID_t;

typedef struct GVCID_s {
    // Master Channel Identifier (MCID)
    MCID_t MCID;
    // Virtual Channel Identifier (VCID) (3 bits)
    U8 VCID : VCID_BIT_FIELD_SIZE;

    bool operator==(const GVCID_s& other) const;

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

    static void toVal(GVCID_s const& gvcid, U16& val);

    static void fromVal(GVCID_s& gvcid, U16 const val);

    static void toVal(GVCID_s const& gvcid, U32& val);

    static void fromVal(GVCID_s& gvcid, U32 const val);
} GVCID_t;

// These contain a set of non-contiguous parameters which are mutually agreed upon
// between sender and receiver and do not change over the course of the mission phase
// as defined by CCSDS 132.0-B-3 1.6.1.3
typedef struct MissionPhaseParameters_s {
    // CCSDS 132.0-B-3 4.1.2.2.2 recommends this be set to 00
    U8 transferFrameVersion;
    // Identifies the space craft associated with the transfer frame
    U16 spaceCraftId;
    // CCSDS 132.0-B-3 4.1.2.2.2 indicates whether
    // the data field with be followed by an operational control field
    bool hasOperationalControlFlag;
    // CCSDS 132.0-B-3 4.1.2.7.3
    // 1 bit field which indicates whether to include a secondary header or not
    bool hasSecondaryHeader;
    // CCSDS 132.0-B-3 4.1.2.7.3
    // This 1 bit field expresses the type of data to be found in the data field
    // '0' indicates that the data field contains packets or idle data
    // (which should be octet-synchronized and forward ordered)
    // '1' indicates that the data field contains virtual channel access service data units (VCA_SDU)
    bool isSyncFlagEnabled;
} MissionPhaseParameters_t;

typedef struct {
    bool isOnlyIdleData;
    bool isFieldDataExtendedPacket;
    U16 packetOffset;
} DataFieldDesc_t;

// The data which is specific to each transfer frame
typedef struct {
    U8 virtualChannelId;
    U8 masterChannelFrameCount;
    U8 virtualChannelFrameCount;
    DataFieldDesc_t dataFieldDesc;
} TransferData_t;


// clang-format off
// CCSDS Transfer Frame Data Field Status (16 bits)
// +-----+-----+-----+-------+-----------------------------------+
// | Sec | Syn | Pkt | Seg   |           First Header           |
// | Hdr | ch  | Ord | Len   |            Pointer               |
// | (1) | (1) | (1) | (2)   |              (11)                |
// +-----+-----+-----+-------+-----------------------------------+
// clang-format on
typedef struct DataFieldStatus_s {
    // See missionPhaseParameters_t.hasSecondaryHeader
    bool hasSecondaryHeader : 1;  // Constant for Mission Phase
    // See missionPhaseParameters_t.isSyncFlagEnabled
    bool isSyncFlagEnabled : 1;  // Constant for Mission Phase
    // CCSDS 132.0-B-3 4.1.2.7.4
    // This field is reserved for future use (and is recommended to be set to 0)
    // if set to 1 it's meaning is undefined
    bool isPacketOrdered : 1;
    // CCSDS 132.0-B-3 4.1.2.7.5
    // If not isSyncFlagEnabled denotes non-use of source packet segments in previous (legacy) versions
    // and should be set to 0b11
    // If isSyncFlagEnabled is true then it's meaning is undefined.
    U8 segmentLengthId : 3;
    // CCSDS 4.1.2.7.6
    // What this field expresses depends on the sync flag field (isSyncEnabled).
    // If the sync flag field is 1 then this field is undefined.
    // If the sync flag field is 0 then this field indicates the position of
    // the first octet of the first packet in the transfer frame data field.
    U16 firstHeaderPointer : 11;
} __attribute__((packed)) DataFieldStatus_t;

// clang-format off
// CCSDS 132.0-B-3 4.1.2.7 Transfer Frame Primary Header (48 bits)
// +--------+------------+-----+---+---------+---------+----------------+
// |Version |  Spacecraft| VCID|OCF| Master  | Virtual |  Data Field    |
// |  (2)   |    ID      | (3) |(1)| Channel | Channel |    Status      |
// |        |   (10)     |     |   | Frame   | Frame   |     (16)       |
// |        |            |     |   | Count(8)| Count(8)|                |
// +--------+------------+-----+---+---------+---------+----------------+
// |        Octet 1-2              | Octet 3-4          |    Octet 5-6   |
// clang-format on

// NOTE __attribute__((packed)) is used here with bit specifications to express field mapping
typedef struct PrimaryHeaderControlInfo_s {
    U8 transferFrameVersion : 2;  // Constant for Mission Phase
    U16 spacecraftId : 10;        // Constant for Mission Phase
    U8 virtualChannelId : 3;
    bool operationalControlFlag : 1;  // Constant for Mission Phase
    U8 masterChannelFrameCount;
    U8 virtualChannelFrameCount;
    DataFieldStatus_t dataFieldStatus;
} __attribute__((packed)) PrimaryHeaderControlInfo_t;
// TM Primary Header: 6 octets (CCSDS 132.0-B-3, Section 4.1.2)
static constexpr FwSizeType PRIMARY_HEADER_SERIALIZED_SIZE = 6;

}  // namespace TMSpaceDataLink

#endif // TRANSFERFRAMEDEFS_H_

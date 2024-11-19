// ======================================================================
// \title  TMSpaceDataLink.hpp
// \author Reginald Marr
// \brief  hpp file for TMSpaceDataLink class
//
// \copyright
// Copyright 2009-2021, by the California Institute of Technology.
// ALL RIGHTS RESERVED.  United States Government Sponsorship
// acknowledged.
//
// ======================================================================

#ifndef SVC_TM_SPACE_DATA_LINK_HPP
#define SVC_TM_SPACE_DATA_LINK_HPP

#include <Svc/FramingProtocol/FramingProtocol.hpp>
#include <cstddef>
#include "Svc/FrameAccumulator/FrameDetector/StartLengthCrcDetector.hpp"
#include "Fw/Types/SerialStatusEnumAc.hpp"
#include "Fw/Types/Serializable.hpp"
#include "Svc/FramingProtocol/CCSDSProtocols/CCSDSProtocolDefs.hpp"
#include "Svc/FrameAccumulator/FrameDetector/StartLengthCrcDetector.hpp"
#include "config/FpConfig.h"

namespace Svc {
namespace FrameDetectors {
    using TMSpaceDataLinkStartWord = StartToken<U16, static_cast<U16>(0 | TM_SCID_VAL_TO_FIELD(CCSDS_SCID)), TM_SCID_MASK>;
    using TMSpaceDataLinkLength = LengthToken<FwSizeType, sizeof(FwSizeType), TM_TRANSFER_FRAME_SIZE(TM_DATA_FIELD_DFLT_SIZE), TM_LENGTH_MASK>;
    using TMSpaceDataLinkChecksum = CRC<U16, TM_TRANSFER_FRAME_SIZE(TM_DATA_FIELD_DFLT_SIZE), -2, CRC16_CCITT>;
    using TMSpaceDataLinkDetector = StartLengthCrcDetector<TMSpaceDataLinkStartWord, TMSpaceDataLinkLength, TMSpaceDataLinkChecksum>;
}
}

namespace TMSpaceDataLink {
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

// CCSDS Transfer Frame Data Field Status (16 bits)
// +-----+-----+-----+-------+-----------------------------------+
// | Sec | Syn | Pkt | Seg   |           First Header           |
// | Hdr | ch  | Ord | Len   |            Pointer               |
// | (1) | (1) | (1) | (2)   |              (11)                |
// +-----+-----+-----+-------+-----------------------------------+
typedef struct DataFieldStatus_s {
    // See missionPhaseParameters_t.hasSecondaryHeader
    bool hasSecondaryHeader : 1; // Constant for Mission Phase
    // See missionPhaseParameters_t.isSyncFlagEnabled
    bool isSyncFlagEnabled : 1; // Constant for Mission Phase
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
    U16 firstHeaderPointer: 11;
} __attribute__((packed)) DataFieldStatus_t;

// CCSDS 132.0-B-3 4.1.2.7 Transfer Frame Primary Header (48 bits)
// +--------+------------+-----+---+---------+---------+----------------+
// |Version |  Spacecraft| VCID|OCF| Master  | Virtual |  Data Field    |
// |  (2)   |    ID      | (3) |(1)| Channel | Channel |    Status      |
// |        |   (10)     |     |   | Frame   | Frame   |     (16)       |
// |        |            |     |   | Count(8)| Count(8)|                |
// +--------+------------+-----+---+---------+---------+----------------+
// |        Octet 1-2          |    Octet 3-4          |    Octet 5-6   |
class PrimaryHeader : public Fw::Serializable {
    // TM Primary Header: 6 octets (CCSDS 132.0-B-3, Section 4.1.2)
    static constexpr FwSizeType SIZE = 6;
    // If data field is an extension of some previous packet and the sync flag is 1
    // this should be the firstHeaderPointerField
    static constexpr U16 EXTEND_PACKET_TYPE = 0b11111111111;
    // If data field contains only idle data and the sync flag is 1
    // this should be the firstHeaderPointerField
    static constexpr U16 IDLE_TYPE = 0b11111111110;
    public:
        // Describes the primary headers fields
        // NOTE __attribute__((packed)) is used here with bit specifications to express field mapping
        typedef struct ControlInformation_s {
            U8 transferFrameVersion: 2; // Constant for Mission Phase
            U16 spacecraftId: 10; // Constant for Mission Phase
            U8 virtualChannelId : 3;
            bool operationalControlFlag : 1; // Constant for Mission Phase
            U8 masterChannelFrameCount;
            U8 virtualChannelFrameCount;
            DataFieldStatus_t dataFieldStatus;
        } __attribute__((packed)) ControlInformation_t;

        PrimaryHeader(MissionPhaseParameters_t const &params);
        ~PrimaryHeader() = default;
        PrimaryHeader& operator=(const PrimaryHeader& other);

        const ControlInformation_t getControlInfo() {return m_ci; };
        Fw::SerializeStatus serialize(Fw::SerializeBufferBase& buffer, TransferData_t &transferData);
    private:
        Fw::SerializeStatus serialize(Fw::SerializeBufferBase& buffer) const override;

        Fw::SerializeStatus deserialize(Fw::SerializeBufferBase& buffer) override {
            return Fw::SerializeStatus::FW_SERIALIZE_OK;
        }

        ControlInformation_t m_ci;
};

// CCSDS Transfer Frame Secondary Header (1-64 octets)
// +----------------------+-------------------------------+
// |    Secondary Header  |      Secondary Header         |
// |    ID Field (8)      |      Data Field               |
// |  +---------+--------+|     (8-504 bits)              |
// |  |Version  | Length ||    [1-63 octets]              |
// |  |  (2)    |  (6)   ||                               |
// +--+---------+--------++-------------------------------+
// |     Octet 1         |       Octets 2-64              |
// NOTE should leverage std::optional here
class SecondaryHeader : public Fw::Serializable {
    // At least one byte of transfer frame data units must be associated with the header for it to be used
    static constexpr FwSizeType MIN_FSDU_LEN = 1;
    static constexpr FwSizeType MAX_SIZE = 64;          // TM Secondary Header is up to 64 octets (CCSDS 132.0-B-3, Section 4.1.3)
    public:

        typedef struct ControlInformation_s {
            U8 version : 2;
            bool length : 6;
            U8 dataField[MAX_SIZE - 1];
        } __attribute__((packed)) ControlInformation_t;

        SecondaryHeader() = default;
        ~SecondaryHeader() = default;
        SecondaryHeader& operator=(const SecondaryHeader& other);

        const ControlInformation_t getControlInfo() {return m_ci; };

        Fw::SerializeStatus serialize(Fw::SerializeBufferBase& buffer) const override {
            // currently unsupported
            return Fw::SerializeStatus::FW_SERIALIZE_FORMAT_ERROR;
        }

        Fw::SerializeStatus deserialize(Fw::SerializeBufferBase& buffer) override {
            // currently unsupported
            return Fw::SerializeStatus::FW_SERIALIZE_FORMAT_ERROR;
        }

    private:
        ControlInformation_t m_ci;
};

// NOTE need to come up with a way to express the underlying service type
class DataField : public Fw::Serializable {
    // At least one byte of transfer frame data units must be associated with the header for it to be used
    static constexpr FwSizeType MIN_FSDU_LEN = 1;
    // TM Secondary Header is up to 64 octets (CCSDS 132.0-B-3, Section 4.1.3)
    static constexpr FwSizeType MAX_SIZE = 64;
    public:
        // NOTE expand and leverage this to express service info
        typedef enum {
            VCAS, // !< Virtual Channel Access Service
            VCPS, // !< Virtual Channel Packet Service
        } ServiceType;

        DataField() = default;
        ~DataField() = default;
        DataField& operator=(const DataField& other);

        Fw::SerializeStatus serialize(Fw::SerializeBufferBase& buffer, const U8* const data, const U32 size) const;
        Fw::SerializeStatus serialize(Fw::SerializeBufferBase& buffer) const override {
            return Fw::SerializeStatus::FW_SERIALIZE_OK;
        }

        Fw::SerializeStatus deserialize(Fw::SerializeBufferBase& buffer) override {
            // currently unsupported
            return Fw::SerializeStatus::FW_SERIALIZE_FORMAT_ERROR;
        }
};

// TM Transfer Frame Structural Components
// +-------------------------+------------------------+------------------------+-------------------------+-------------------------+
// | TRANSFER FRAME          | TRANSFER FRAME         | TRANSFER FRAME         | TRANSFER FRAME          | TRANSFER FRAME          |
// | PRIMARY HEADER          | SECONDARY HEADER       | DATA FIELD             | TRAILER                 | TRAILER                 |
// |                         | (Optional)             |                        | OPERATIONAL CONTROL     | FRAME ERROR CONTROL     |
// |                         |                        |                        | FIELD (Optional)        | FIELD (Optional)        |
// +-------------------------+------------------------+------------------------+-------------------------+-------------------------+
// | 6 octets                | Up to 64 octets        | Varies                 | 4 octets                | 2 octets                |
// +-------------------------+------------------------+------------------------+-------------------------+-------------------------+
class TransferFrame : public Fw::Serializable {
public:

    TransferFrame(const MissionPhaseParameters_t &missionParams):
            m_primaryHeader(missionParams), m_secondaryHeader(), m_dataField() {}
    ~TransferFrame() = default;

    PrimaryHeader getPrimaryHeader() {return m_primaryHeader; };
    SecondaryHeader getSecondaryHeader() {return m_secondaryHeader; };
    DataField getDataField() {return m_dataField; };

    Fw::SerializeStatus serialize(Fw::SerializeBufferBase& buffer, TransferData_t &transferData, const U8* const data, const U32 size);

private:
    PrimaryHeader m_primaryHeader;
    SecondaryHeader m_secondaryHeader;
    DataField m_dataField;
    using CheckSum = Svc::FrameDetectors::TMSpaceDataLinkChecksum;

    Fw::SerializeStatus serialize(Fw::SerializeBufferBase& buffer) const override {
        return Fw::SerializeStatus::FW_SERIALIZE_OK;
    }

    Fw::SerializeStatus deserialize(Fw::SerializeBufferBase& buffer) override {
        return Fw::SerializeStatus::FW_SERIALIZE_OK;
    }
};

}

namespace Svc {
class TMSpaceDataLinkProtocol: public FramingProtocol {

  public:

    TMSpaceDataLinkProtocol(const TMSpaceDataLink::MissionPhaseParameters_t& missionParams, const FwSizeType dataFieldSize):
        m_transferFrame(TMSpaceDataLink::TransferFrame(missionParams)), m_dataFieldSize(dataFieldSize) {}

    void setup(const TMSpaceDataLink::MissionPhaseParameters_t& missionParams, const FwSizeType dataFieldSize);

    void frame(const U8* const data, const U32 size, Fw::ComPacket::ComPacketType packet_type) override;

  private:
    TMSpaceDataLink::TransferData_t m_transferData = {0};
    TMSpaceDataLink::TransferFrame m_transferFrame;
    const FwSizeType m_dataFieldSize{Svc::TM_DATA_FIELD_DFLT_SIZE};
};
}  // namespace Svc
#endif  // SVC_TM_SPACE_DATA_LINK_HPP

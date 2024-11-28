#ifndef PROTOCOL_DATA_UNITS_H_
#define PROTOCOL_DATA_UNITS_H_
#include <Svc/FramingProtocol/FramingProtocol.hpp>
#include <array>
#include <cstddef>
#include "Fw/Buffer/Buffer.hpp"
#include "Fw/Com/ComBuffer.hpp"
#include "Fw/Types/Assert.hpp"
#include "Fw/Types/SerialStatusEnumAc.hpp"
#include "Fw/Types/Serializable.hpp"
#include "Svc/FrameAccumulator/FrameDetector/StartLengthCrcDetector.hpp"
#include "Svc/FramingProtocol/CCSDSProtocols/CCSDSProtocolDefs.hpp"
#include "config/FpConfig.h"

namespace Svc {
namespace FrameDetectors {
using TMSpaceDataLinkStartWord = StartToken<U16, static_cast<U16>(0 | TM_SCID_VAL_TO_FIELD(CCSDS_SCID)), TM_SCID_MASK>;
using TMSpaceDataLinkLength =
    LengthToken<FwSizeType, sizeof(FwSizeType), TM_TRANSFER_FRAME_SIZE(TM_DATA_FIELD_DFLT_SIZE), TM_LENGTH_MASK>;
using TMSpaceDataLinkChecksum = CRC<U16, TM_TRANSFER_FRAME_SIZE(TM_DATA_FIELD_DFLT_SIZE), -2, CRC16_CCITT>;
using TMSpaceDataLinkDetector =
    StartLengthCrcDetector<TMSpaceDataLinkStartWord, TMSpaceDataLinkLength, TMSpaceDataLinkChecksum>;
}  // namespace FrameDetectors
}  // namespace Svc

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
class PrimaryHeader : public Fw::Buffer {
  public:
    // TM Primary Header: 6 octets (CCSDS 132.0-B-3, Section 4.1.2)
    enum {
        SERIALIZED_SIZE = 6,  //!< Size of DataField when serialized
    };
    // If data field is an extension of some previous packet and the sync flag is 1
    // this should be the firstHeaderPointerField
    static constexpr U16 EXTEND_PACKET_TYPE = 0b11111111111;
    // If data field contains only idle data and the sync flag is 1
    // this should be the firstHeaderPointerField
    static constexpr U16 IDLE_TYPE = 0b11111111110;

    // Describes the primary headers fields
    // NOTE __attribute__((packed)) is used here with bit specifications to express field mapping
    typedef struct ControlInformation_s {
        U8 transferFrameVersion : 2;  // Constant for Mission Phase
        U16 spacecraftId : 10;        // Constant for Mission Phase
        U8 virtualChannelId : 3;
        bool operationalControlFlag : 1;  // Constant for Mission Phase
        U8 masterChannelFrameCount;
        U8 virtualChannelFrameCount;
        DataFieldStatus_t dataFieldStatus;
    } __attribute__((packed)) ControlInformation_t;

    PrimaryHeader() = default;
    PrimaryHeader(MissionPhaseParameters_t const& params);
    PrimaryHeader(MissionPhaseParameters_t const& params, TransferData_t& transferData);
    ~PrimaryHeader() = default;
    // PrimaryHeader& operator=(const PrimaryHeader& other);

    const ControlInformation_t getControlInfo() const { return m_ci; };
    Fw::SerializeStatus serialize(Fw::SerializeBufferBase& buffer, TransferData_t& transferData);

    Fw::SerializeStatus serialize(Fw::SerializeBufferBase& buffer) const override;
    Fw::SerializeStatus deserialize(Fw::SerializeBufferBase& buffer) override {
        return Fw::SerializeStatus::FW_SERIALIZE_OK;
    }

    void setMasterChannelCount(U8 channelCount) { m_ci.masterChannelFrameCount = channelCount; };
    void setVirtualChannelCount(U8 channelCount) { m_ci.virtualChannelFrameCount = channelCount; };

  private:
    void setControlInfo(TransferData_t& transferData);
    void setControlInfo(MissionPhaseParameters_t const& params);

    ControlInformation_t m_ci;
};

// clang-format off
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
// clang-format on
// Currently we assume this isnt supported
template <FwSizeType FieldSize = 0>
class SecondaryHeader : public Fw::Buffer {
    // At least one byte of transfer frame data units must be associated with the header for it to be used
    static constexpr FwSizeType MIN_FSDU_LEN = 1;
    static constexpr FwSizeType MAX_SIZE =
        64;  // TM Secondary Header is up to 64 octets (CCSDS 132.0-B-3, Section 4.1.3)
  public:
    typedef struct ControlInformation_s {
        U8 version : 2;
        bool length : 6;
        U8 dataField[MAX_SIZE - 1];
    } __attribute__((packed)) ControlInformation_t;

    SecondaryHeader() = default;
    ~SecondaryHeader() = default;
    // SecondaryHeader& operator=(const SecondaryHeader& other) {
    // };

    static constexpr FwSizeType SIZE = FieldSize;
    const ControlInformation_t getControlInfo() { return m_ci; };

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

template <FwSizeType FieldSize = 64>
class DataField : public Fw::Buffer {
  public:
    // At least one byte of transfer frame data units must be associated with the header for it to be used
    static constexpr FwSizeType MIN_FSDU_LEN = 1;
    // TM Secondary Header is up to 64 octets (CCSDS 132.0-B-3, Section 4.1.3)
    static constexpr FwSizeType MAX_SIZE = 64;
    enum {
        SERIALIZED_SIZE = FieldSize,  //!< Size of DataField when serialized
    };

    DataField() : m_data() {}

    DataField(std::array<U8, FieldSize> srcBuff) : m_data(srcBuff) {}
    DataField(Fw::Buffer& srcBuff) {
        FW_ASSERT(srcBuff.getSize() == FieldSize, srcBuff.getSize());
        // srcBuff.deserialize(m_data.data(), SERIALIZED_SIZE);
    }

    ~DataField() = default;
    DataField& operator=(const DataField& other);

    // NOTE should we maybe be inheriting from buffer instead ?
    Fw::SerializeStatus serialize(Fw::SerializeBufferBase& buffer, const U8* const data, const U32 size) const;
    Fw::SerializeStatus serialize(Fw::SerializeBufferBase& buffer) const override;
    Fw::SerializeStatus deserialize(Fw::SerializeBufferBase& buffer) override;
    Fw::SerializeStatus serialize(const U8* buff, FwSizeType length);
    Fw::SerializeStatus deserialize(U8* buff, NATIVE_UINT_TYPE length);

  private:
    std::array<U8, SERIALIZED_SIZE> m_data;
};

// This could become an optional template class
class FrameErrorControlField {
    enum {
        SERIALIZED_SIZE = sizeof(U16),  //!< Size of Field when serialized
    };

  public:
    FrameErrorControlField() = default;
    ~FrameErrorControlField() = default;
    bool set(U8* const startPtr, Fw::SerializeBufferBase& buffer) const;

  private:
    U16 m_Value;
    using CheckSum = Svc::FrameDetectors::TMSpaceDataLinkChecksum;
};

}  // namespace TMSpaceDataLink

#endif  // PROTOCOL_DATA_UNITS_H_

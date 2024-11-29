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

template <FwSizeType FieldSize, typename FieldValueType>
class ProtocolDataUnitBase {
public:
    using FieldValue_t = FieldValueType;
    enum { SERIALIZED_SIZE = FieldSize };

    ProtocolDataUnitBase();
    ProtocolDataUnitBase(FieldValueType srcVal);
    virtual ~ProtocolDataUnitBase() = default;
    ProtocolDataUnitBase& operator=(const ProtocolDataUnitBase& other);

    void get(FieldValueType& val) const;
    void set(FieldValueType const& val);
    bool insert(Fw::SerializeBufferBase& buffer) const;
    bool insert(Fw::SerializeBufferBase& buffer, FieldValueType& val);
    bool extract(Fw::SerializeBufferBase& buffer);
    bool extract(Fw::SerializeBufferBase& buffer, FieldValueType& val);

protected:
    FieldValueType m_value;
    virtual Fw::SerializeStatus serializeValue(Fw::SerializeBufferBase& buffer) const = 0;
    virtual Fw::SerializeStatus deserializeValue(Fw::SerializeBufferBase& buffer) = 0;
};

template <FwSizeType FieldSize, typename FieldValueType>
class ProtocolDataUnit : public ProtocolDataUnitBase<FieldSize, FieldValueType> {
public:
    using Base = ProtocolDataUnitBase<FieldSize, FieldValueType>;
    using typename Base::FieldValue_t;  // Need typename here
    using Base::ProtocolDataUnitBase;   // Inheriting constructor
    using Base::operator=;              // Inheriting assignment

    void get(FieldValue_t& val) const;
    void set(FieldValue_t const& val);

protected:
    Fw::SerializeStatus serializeValue(Fw::SerializeBufferBase& buffer) const override;
    Fw::SerializeStatus deserializeValue(Fw::SerializeBufferBase& buffer) override;
};

template <FwSizeType FieldSize>
class ProtocolDataUnit<FieldSize, std::array<U8, FieldSize>>
    : public ProtocolDataUnitBase<FieldSize, std::array<U8, FieldSize>> {
public:
    using Base = ProtocolDataUnitBase<FieldSize, std::array<U8, FieldSize>>;
    using typename Base::FieldValue_t;
    using Base::ProtocolDataUnitBase;
    using Base::operator=;

    void get(FieldValue_t& val) const;
    void set(FieldValue_t const& val);

protected:
    Fw::SerializeStatus serializeValue(Fw::SerializeBufferBase& buffer) const override;
    Fw::SerializeStatus deserializeValue(Fw::SerializeBufferBase& buffer) override;
};

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
class PrimaryHeader {
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

    Fw::SerializeStatus serialize(Fw::SerializeBufferBase& buffer) const;
    Fw::SerializeStatus deserialize(Fw::SerializeBufferBase& buffer);

    void setControlInfo(TransferData_t& transferData);
    void setMasterChannelCount(U8 channelCount) { m_ci.masterChannelFrameCount = channelCount; };
    void setVirtualChannelCount(U8 channelCount) { m_ci.virtualChannelFrameCount = channelCount; };

  private:
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
class DataField : public ProtocolDataUnit<FieldSize, std::array<U8, FieldSize>> {
  public:
    // Inherit parent's type definitions
    using Base = ProtocolDataUnit<FieldSize, std::array<U8, FieldSize>>;

    // Inherit constructors
    using Base::ProtocolDataUnit;
    using typename Base::FieldValue_t;
    using Base::operator=;

    // At least one byte of transfer frame data units must be associated with the header for it to be used
    static constexpr FwSizeType MIN_SIZE = 1;
    static_assert(FieldSize >= MIN_SIZE, "FieldSize must be at least MIN_FSDU_LEN");

    DataField(Fw::Buffer& srcBuff) {
        FW_ASSERT(srcBuff.getSize() == FieldSize, srcBuff.getSize());
        FW_ASSERT(this->extract(srcBuff.getSerializeRepr()));
    }
};

// NOTE This could leverage some optional template class
class FrameErrorControlField : public ProtocolDataUnit<sizeof(U16), U16> {
  public:
    // Inherit parent's type definitions
    using Base = ProtocolDataUnit<sizeof(U16), U16>;

    // Inherit constructors
    using Base::ProtocolDataUnit;
    using typename Base::FieldValue_t;
    using Base::operator=;

    bool insert(U8* startPtr, Fw::SerializeBufferBase& buffer) const;
  private:
    // Determines the fields value from a provided buffer and startPoint
    // bool set(U8 const* const startPtr, Fw::SerializeBufferBase& buffer);
    bool calc_value(U8* startPtr, Fw::SerializeBufferBase& buffer) const;

    using CheckSum = Svc::FrameDetectors::TMSpaceDataLinkChecksum;
};

}  // namespace TMSpaceDataLink

#endif  // PROTOCOL_DATA_UNITS_H_

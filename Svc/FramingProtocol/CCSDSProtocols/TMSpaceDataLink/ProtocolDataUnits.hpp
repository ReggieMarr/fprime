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
#include "TransferFrameDefs.hpp"

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
    using Base::ProtocolDataUnitBase;   // Inheriting constructor
    using typename Base::FieldValue_t;  // Need typename here
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
    using Base::ProtocolDataUnitBase;
    using typename Base::FieldValue_t;
    using Base::operator=;

    void get(FieldValue_t& val) const;
    void set(FieldValue_t const& val);

  protected:
    Fw::SerializeStatus serializeValue(Fw::SerializeBufferBase& buffer) const override;
    Fw::SerializeStatus deserializeValue(Fw::SerializeBufferBase& buffer) override;
};

template <>
class ProtocolDataUnit<0, std::nullptr_t> : public ProtocolDataUnitBase<0, std::nullptr_t> {
  public:
    using Base = ProtocolDataUnitBase<0, nullptr_t>;
    using Base::ProtocolDataUnitBase;
    using typename Base::FieldValue_t;
    using Base::operator=;

    void get(FieldValue_t& val) const;
    void set(FieldValue_t const& val);

  protected:
    Fw::SerializeStatus serializeValue(Fw::SerializeBufferBase& buffer) const override;
    Fw::SerializeStatus deserializeValue(Fw::SerializeBufferBase& buffer) override;
};

template <>
class ProtocolDataUnit<PRIMARY_HEADER_SERIALIZED_SIZE, PrimaryHeaderControlInfo_t>
    : public ProtocolDataUnitBase<PRIMARY_HEADER_SERIALIZED_SIZE, PrimaryHeaderControlInfo_t> {
  public:
    using Base = ProtocolDataUnitBase<PRIMARY_HEADER_SERIALIZED_SIZE, PrimaryHeaderControlInfo_t>;
    using Base::ProtocolDataUnitBase;
    using typename Base::FieldValue_t;
    using Base::operator=;

    // void get(FieldValue_t& val) const;
    // void set(FieldValue_t const& val);

  protected:
    Fw::SerializeStatus serializeValue(Fw::SerializeBufferBase& buffer) const override;
    Fw::SerializeStatus deserializeValue(Fw::SerializeBufferBase& buffer) override;
};

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
class PrimaryHeader : public ProtocolDataUnit<PRIMARY_HEADER_SERIALIZED_SIZE, PrimaryHeaderControlInfo_t> {
  public:
    // Inherit parent's type definitions
    using Base = ProtocolDataUnit<PRIMARY_HEADER_SERIALIZED_SIZE, PrimaryHeaderControlInfo_t>;

    // Inherit constructors
    using Base::ProtocolDataUnit;
    using typename Base::FieldValue_t;
    using Base::operator=;
    // If data field is an extension of some previous packet and the sync flag is 1
    // this should be the firstHeaderPointerField
    static constexpr U16 EXTEND_PACKET_TYPE = 0b11111111111;
    // If data field contains only idle data and the sync flag is 1
    // this should be the firstHeaderPointerField
    static constexpr U16 IDLE_TYPE = 0b11111111110;

    PrimaryHeader(MissionPhaseParameters_t const& params);
    PrimaryHeader(MissionPhaseParameters_t const& params, TransferData_t& transferData);

    void setControlInfo(TransferData_t& transferData);
    void setMasterChannelCount(U8 channelCount) { this->m_value.masterChannelFrameCount = channelCount; };
    void setVirtualChannelCount(U8 channelCount) { this->m_value.virtualChannelFrameCount = channelCount; };

  private:
    void setControlInfo(MissionPhaseParameters_t const& params);
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
// Note currently implemented but we want to support it architecturally
class NullSecondaryHeader : public ProtocolDataUnit<0, std::nullptr_t> {
  public:
    // Inherit parent's type definitions
    using Base = ProtocolDataUnit<0, std::nullptr_t>;

    // Inherit constructors
    using Base::ProtocolDataUnit;
    using typename Base::FieldValue_t;
    using Base::operator=;
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

    // const ControlInformation_t getControlInfo() { return m_ci; };

  private:
    ControlInformation_t m_ci;
};

class NullOperationalControlField : public ProtocolDataUnit<0, std::nullptr_t> {
  public:
    // Inherit parent's type definitions
    using Base = ProtocolDataUnit<0, std::nullptr_t>;

    // Inherit constructors
    using Base::ProtocolDataUnit;
    using typename Base::FieldValue_t;
    using Base::operator=;
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

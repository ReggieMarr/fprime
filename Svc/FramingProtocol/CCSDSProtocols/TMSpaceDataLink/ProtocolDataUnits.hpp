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
#include "TransferFrameDefs.hpp"
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
    void set(U8 const* buffPtr, FwSizeType size);

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

}  // namespace TMSpaceDataLink

#endif  // PROTOCOL_DATA_UNITS_H_

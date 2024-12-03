// ======================================================================
// \title  TMSpaceDataLink TransferFrame.cpp
// \author Reginald Marr
// \brief  Implementation of TMSpaceDataLink TransferFrame
//
// \copyright
// Copyright 2009-2021, by the California Institute of Technology.
// ALL RIGHTS RESERVED.  United States Government Sponsorship
// acknowledged.
//
// ======================================================================

#ifndef TM_SPACE_DATA_LINK_TRANSFER_FRAME_HPP
#define TM_SPACE_DATA_LINK_TRANSFER_FRAME_HPP

#include <Svc/FramingProtocol/FramingProtocol.hpp>
#include <array>
#include <cstddef>
#include "Fw/Buffer/Buffer.hpp"
#include "Fw/Com/ComBuffer.hpp"
#include "Fw/Types/Assert.hpp"
#include "Fw/Types/SerialStatusEnumAc.hpp"
#include "Fw/Types/Serializable.hpp"
#include "ProtocolDataUnits.hpp"
#include "Svc/FrameAccumulator/FrameDetector/StartLengthCrcDetector.hpp"
#include "Svc/FramingProtocol/CCSDSProtocols/CCSDSProtocolDefs.hpp"
#include "Svc/FramingProtocol/CCSDSProtocols/TMSpaceDataLink/ProtocolDataUnits.hpp"
#include "config/FpConfig.h"

namespace TMSpaceDataLink {

// NOTE this is used for optional fields when we want the option to not use them
class NullField : public ProtocolDataUnit<0, std::nullptr_t> {
  public:
    // Inherit parent's type definitions
    using Base = ProtocolDataUnit<0, std::nullptr_t>;

    // Inherit constructors
    using Base::Base;
    using typename Base::FieldValue_t;
    using Base::operator=;
};

// NOTE These use a generic protocol data unit which leverages the protocoldataunit base
// to provide a useful abstraction around the setting of values/deserializer/serializer.
// Its a useful abstraction but seems to venture into too much indirection territory at times.
// TODO see if we can reduce some of the layers here
class PrimaryHeader : public ProtocolDataUnit<PRIMARY_HEADER_SERIALIZED_SIZE, PrimaryHeaderControlInfo_t> {
  public:
    // Inherit parent's type definitions
    using Base = ProtocolDataUnit<PRIMARY_HEADER_SERIALIZED_SIZE, PrimaryHeaderControlInfo_t>;

    enum { SERIALIZED_SIZE = 6 };

    // Inherit constructors
    using Base::ProtocolDataUnit;
    using typename Base::FieldValue_t;
    using Base::operator=;
    using Base::set;
    // If data field is an extension of some previous packet and the sync flag is 1
    // this should be the firstHeaderPointerField
    static constexpr U16 EXTEND_PACKET_TYPE = 0b11111111111;
    // If data field contains only idle data and the sync flag is 1
    // this should be the firstHeaderPointerField
    static constexpr U16 IDLE_TYPE = 0b11111111110;

    PrimaryHeader(PrimaryHeaderControlInfo_t const& params);

    void setMasterChannelCount(U8 channelCount) { this->m_value.masterChannelFrameCount = channelCount; };
    void setVirtualChannelCount(U8 channelCount) { this->m_value.virtualChannelFrameCount = channelCount; };
};

static constexpr FwSizeType DFLT_DATA_FIELD_SIZE = 247;

template <FwSizeType FieldSize = DFLT_DATA_FIELD_SIZE>
class DataField : public ProtocolDataUnit<FieldSize, std::array<U8, FieldSize>> {
  public:
    // Inherit parent's type definitions
    using Base = ProtocolDataUnit<FieldSize, std::array<U8, FieldSize>>;

    // Inherit constructors
    using Base::ProtocolDataUnit;
    using Base::SERIALIZED_SIZE;
    using typename Base::FieldValue_t;
    using Base::operator=;
    using Base::get;
    using Base::set;

    // At least one byte of transfer frame data units must be associated with the header for it to be used
    static constexpr FwSizeType MIN_SIZE = 1;
    static_assert(FieldSize >= MIN_SIZE, "FieldSize must be at least MIN_FSDU_LEN");

    DataField(Fw::Buffer& srcBuff);
};

template <U16 StartWord, FwSizeType TransferFrameLength>
class FrameErrorControlField : public ProtocolDataUnit<sizeof(U16), U16> {
  public:
    // Inherit parent's type definitions
    using Base = ProtocolDataUnit<sizeof(U16), U16>;

    // Inherit constructors
    using Base::ProtocolDataUnit;
    using Base::SERIALIZED_SIZE;
    using typename Base::FieldValue_t;
    using Base::operator=;

  private:
    using TMSpaceDataLinkStartWord =
        Svc::FrameDetectors::StartToken<U16, static_cast<U16>(0 | TM_SCID_VAL_TO_FIELD(StartWord)), Svc::TM_SCID_MASK>;
    using TMSpaceDataLinkLength =
        Svc::FrameDetectors::LengthToken<FwSizeType, sizeof(FwSizeType), TransferFrameLength, Svc::TM_LENGTH_MASK>;
    // Sets up the crc checker to check the transferFrameLength minus the last field (this one)
    using TMSpaceDataLinkChecksum =
        Svc::FrameDetectors::CRC<U16, TransferFrameLength - SERIALIZED_SIZE, 0, Svc::FrameDetectors::CRC16_CCITT>;
    using TMSpaceDataLinkDetector = Svc::FrameDetectors::
        StartLengthCrcDetector<TMSpaceDataLinkStartWord, TMSpaceDataLinkLength, TMSpaceDataLinkChecksum>;

    using CheckSum = TMSpaceDataLinkChecksum;

  public:
    // Determines the fields value from a provided buffer and startPoint
    // bool set(U8 const* const startPtr, Fw::SerializeBufferBase& buffer);
    bool calcBufferCRC(U8* sourceBufferPtr, FwSizeType const sourceBufferSize, U16& crcValue) const;

    bool insert(U8* startPtr, Fw::SerializeBufferBase& buffer) const;
    using Base::extract;

    using Base::get;
    using Base::set;
};

// clang-format off
// TM Transfer Frame Structural Components
// +-------------------------+------------------------+------------------------+-------------------------+-------------------------+
// | TRANSFER FRAME          | TRANSFER FRAME         | TRANSFER FRAME         | TRANSFER FRAME          | TRANSFER FRAME          |
// | PRIMARY HEADER          | SECONDARY HEADER       | DATA FIELD             | TRAILER                 | TRAILER                 |
// |                         | (Optional)             |                        | OPERATIONAL CONTROL     | FRAME ERROR CONTROL     |
// |                         |                        |                        | FIELD (Optional)        | FIELD (Optional)        |
// +-------------------------+------------------------+------------------------+-------------------------+-------------------------+
// | 6 octets                | Up to 64 octets        | Varies                 | 4 octets                | 2 octets                |
// +-------------------------+------------------------+------------------------+-------------------------+-------------------------+
// clang-format on
// NOTE if we wanted to get really meta we could actually define this as a derived kind of pdu
template <typename SecondaryHeaderType,
          typename DataFieldType,
          typename OperationalControlFieldType,
          typename ErrorControlFieldType>
class TransferFrameBase {
    // TODO do some Type checking via static assertions
  public:
    using PrimaryHeader_t = PrimaryHeader;
    using SecondaryHeader_t = SecondaryHeaderType;
    using DataField_t = DataFieldType;
    using OperationalControlFieldType_t = OperationalControlFieldType;
    using ErrorControlFieldType_t = ErrorControlFieldType;

    enum {
        SERIALIZED_SIZE = PrimaryHeader::SERIALIZED_SIZE + SecondaryHeaderType::SERIALIZED_SIZE +
                          DataFieldType::SERIALIZED_SIZE + +OperationalControlFieldType::SERIALIZED_SIZE +
                          ErrorControlFieldType::SERIALIZED_SIZE,  //!< Size of DataField when serialized
    };
    static_assert(SERIALIZED_SIZE <= FW_COM_BUFFER_MAX_SIZE, "TransferFrame must fit within a Coms buffer");

    TransferFrameBase();
    virtual ~TransferFrameBase() = default;

    virtual bool insert(Fw::SerializeBufferBase& buffer) const;
    virtual bool extract(Fw::SerializeBufferBase& buffer);

    // NOTE We used to have getters here but since each class protects itself pretty well
    // we changed that for simplicity. Revisit if implementations need it.
    PrimaryHeader primaryHeader;
    SecondaryHeaderType secondaryHeader;
    DataFieldType dataField;
    OperationalControlFieldType operationalControlField;
    ErrorControlFieldType errorControlField;
};

// TODO wrap this in a nicer data structure
using FPrimeSecondaryHeaderField = NullField;
using FPrimeDataField = DataField<DFLT_DATA_FIELD_SIZE>;
using FPrimeOperationalControlField = NullField;
// NOTE this is tied to a particular SCID
using FPrimeErrorControlField =
    FrameErrorControlField<CCSDS_SCID,
                           PrimaryHeader::SERIALIZED_SIZE + FPrimeSecondaryHeaderField::SERIALIZED_SIZE +
                               FPrimeDataField::SERIALIZED_SIZE + FPrimeOperationalControlField::SERIALIZED_SIZE +
                               // To account for the size of the error field itself
                               sizeof(U16)>;
using FPrimeTransferFrame = TransferFrameBase<FPrimeSecondaryHeaderField,
                                              FPrimeDataField,
                                              FPrimeOperationalControlField,
                                              FPrimeErrorControlField>;

}  // namespace TMSpaceDataLink

#endif  // TM_SPACE_DATA_LINK_TRANSFER_FRAME_HPP

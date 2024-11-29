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
#include "Svc/FrameAccumulator/FrameDetector/StartLengthCrcDetector.hpp"
#include "Svc/FramingProtocol/CCSDSProtocols/CCSDSProtocolDefs.hpp"
#include "Svc/FramingProtocol/CCSDSProtocols/TMSpaceDataLink/ProtocolDataUnits.hpp"
#include "config/FpConfig.h"

namespace TMSpaceDataLink {

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
    // // Type checking via static assertions
    // static_assert(std::is_base_of_v<ProtocolDataUnitBase<
    //     SecondaryHeaderType::SERIALIZED_SIZE,
    //     typename SecondaryHeaderType::FieldValue_t>,
    //     SecondaryHeaderType>,
    //     "SecondaryHeaderType must inherit from ProtocolDataUnitBase");

    // static_assert(std::is_base_of_v<ProtocolDataUnitBase<
    //     DataFieldType::SERIALIZED_SIZE,
    //     typename DataFieldType::FieldValue_t>,
    //     DataFieldType>,
    //     "DataFieldType must inherit from ProtocolDataUnitBase");
  public:
    enum {
        SERIALIZED_SIZE = PrimaryHeader::SERIALIZED_SIZE + SecondaryHeaderType::SERIALIZED_SIZE +
                          DataFieldType::SERIALIZED_SIZE + +OperationalControlFieldType::SERIALIZED_SIZE +
                          ErrorControlFieldType::SERIALIZED_SIZE,  //!< Size of DataField when serialized
    };

    TransferFrameBase();
    virtual ~TransferFrameBase() = default;

    virtual bool insert(Fw::SerializeBufferBase& buffer) const;
    virtual bool extract(Fw::SerializeBufferBase& buffer);

    // NOTE It would be better at this point to return members
    virtual PrimaryHeader& getPrimaryHeader();

    virtual SecondaryHeaderType& getSecondaryHeader();

    virtual DataFieldType& getDataField();

    virtual ErrorControlFieldType& getErrorControlField();

  protected:
    PrimaryHeader m_primaryHeader;
    // NOTE should probably actually be something like this:
    // ProtocolDataUnitBase<SecondaryHeaderType::SERIALIZED_SIZE, SecondaryHeaderType::FieldValue_t> m_secondaryHeader;
    // however that seems a bit recursive and could probably be done better
    // by default these will act like so:
    // ProtocolDataUnit<0, nullptr_t> m_secondaryHeader;

    SecondaryHeaderType m_secondaryHeader;
    DataFieldType m_dataField;
    OperationalControlFieldType m_operationalControlField;
    ErrorControlFieldType m_errorControlField;
};

}  // namespace TMSpaceDataLink

#endif  // TM_SPACE_DATA_LINK_TRANSFER_FRAME_HPP

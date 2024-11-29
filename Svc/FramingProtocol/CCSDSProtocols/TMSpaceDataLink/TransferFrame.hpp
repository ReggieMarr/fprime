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
template <typename SecondaryHeaderType, typename DataFieldType, typename OperationalControlFieldType, typename ErrorControlFieldType>
class TransferFrameBase {
    enum {
        SERIALIZED_SIZE = PrimaryHeader::SERIALIZED_SIZE + SecondaryHeaderType::SERIALIZED_SIZE + DataFieldType::SERIALIZED_SIZE + ErrorControlFieldType::SERIALIZED_SIZE,  //!< Size of DataField when serialized
    };

    TransferFrameBase() = default;
    virtual ~TransferFrameBase() = default;
};

}  // namespace TMSpaceDataLink

#endif  // TM_SPACE_DATA_LINK_TRANSFER_FRAME_HPP

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
template <FwSizeType FrameSize = 255, FwSizeType SecondaryHeaderSize = 0, FwSizeType TrailerSize = 2>
class TransferFrame : public Fw::Buffer {
    // NOTE should become static assert
    // if (m_primaryHeader.getControlInfo().dataFieldStatus.hasSecondaryHeader) {
    // if (m_primaryHeader.getControlInfo().operationalControlFlag) {
  public:
    static constexpr FwSizeType DATA_FIELD_SIZE =
        FrameSize - PrimaryHeader::SERIALIZED_SIZE - SecondaryHeaderSize - TrailerSize;

  private:
    FrameErrorControlField m_errorControlField;
    PrimaryHeader m_primaryHeader;
    SecondaryHeader<SecondaryHeaderSize> m_secondaryHeader;
    DataField<DATA_FIELD_SIZE> m_dataField;

  public:
    enum {
        SERIALIZED_SIZE = FrameSize,  //!< Size of DataField when serialized
    };

    TransferFrame(const MissionPhaseParameters_t& missionParams, Fw::Buffer& dataBuff)
        : m_primaryHeader(missionParams), m_secondaryHeader(), m_dataField(dataBuff) {}

    TransferFrame(Fw::Buffer& dataBuff) : m_primaryHeader(), m_secondaryHeader(), m_dataField(dataBuff) {}

    TransferFrame(PrimaryHeader& primaryHeader, DataField<DATA_FIELD_SIZE>& dataField)
        : m_primaryHeader(primaryHeader), m_secondaryHeader(), m_dataField(dataField) {}
    ~TransferFrame() = default;

    PrimaryHeader getPrimaryHeader() { return m_primaryHeader; };

    // What this constitutes could also become optional
    DataField<DATA_FIELD_SIZE> getDataField() { return m_dataField; };

    Fw::SerializeStatus serialize(Fw::SerializeBufferBase& buffer, TransferData_t transferData, const U8* const data, const U32 size);

    Fw::SerializeStatus serialize(Fw::SerializeBufferBase& buffer);

    Fw::SerializeStatus deserialize(Fw::SerializeBufferBase& buffer);

    bool setFrameErrorControlField() { return false; };
};

}  // namespace TMSpaceDataLink

#endif  // TM_SPACE_DATA_LINK_TRANSFER_FRAME_HPP

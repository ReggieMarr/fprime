// ======================================================================
// \title  TMSpaceDataLink TransferFrame.cpp
// \author Reginald Marr
// \brief  Implementation of TMSpaceDataLink TransferFrame
//
// ======================================================================
#include "TransferFrame.hpp"
#include <array>
#include "FpConfig.h"
#include "FpConfig.hpp"
#include "Fw/Logger/Logger.hpp"
#include "Fw/Types/Assert.hpp"
#include "Fw/Types/Serializable.hpp"
#include "ProtocolDataUnits.hpp"
#include "ProtocolInterface.hpp"
#include "Svc/FrameAccumulator/FrameDetector.hpp"
#include "Svc/FramingProtocol/CCSDSProtocols/CCSDSProtocolDefs.hpp"
#include "Svc/FramingProtocol/CCSDSProtocols/TMSpaceDataLink/ProtocolDataUnits.hpp"
#include "Utils/Hash/Hash.hpp"
#include "Utils/Types/CircularBuffer.hpp"

namespace TMSpaceDataLink {

template <typename SecondaryHeaderType,
          typename DataFieldType,
          typename OperationalControlFieldType,
          typename ErrorControlFieldType>
TransferFrameBase<SecondaryHeaderType, DataFieldType, OperationalControlFieldType, ErrorControlFieldType>::
    TransferFrameBase()
    : m_primaryHeader(), m_secondaryHeader(), m_dataField(), m_operationalControlField(), m_errorControlField() {}

template <typename SecondaryHeaderType,
          typename DataFieldType,
          typename OperationalControlFieldType,
          typename ErrorControlFieldType>
bool TransferFrameBase<SecondaryHeaderType, DataFieldType, OperationalControlFieldType, ErrorControlFieldType>::insert(
    Fw::SerializeBufferBase& buffer) const {
    // Store the starting position for error control calculation
    U8 const* const startPtr = buffer.getBuffAddrSer();

    // Serialize primary header
    if (!m_primaryHeader.insert(buffer)) {
        return false;
    }

    // Serialize secondary header
    if (!m_secondaryHeader.insert(buffer)) {
        return false;
    }

    // Serialize data field
    if (!m_dataField.insert(buffer)) {
        return false;
    }

    // Serialize operational control field if present
    // Note this should be handled by the templating but could also do this check
    // (and for the secondary header as well)
    // if (m_primaryHeader.hasOperationalControl()) {
    if (!m_operationalControlField.insert(buffer)) {
        return false;
    }

    // Calculate and serialize error control field
    return true; //m_errorControlField.insert(startPtr, buffer);
}

template <typename SecondaryHeaderType,
          typename DataFieldType,
          typename OperationalControlFieldType,
          typename ErrorControlFieldType>
bool TransferFrameBase<SecondaryHeaderType, DataFieldType, OperationalControlFieldType, ErrorControlFieldType>::extract(
    Fw::SerializeBufferBase& buffer) {
    // Extract primary header first
    if (!m_primaryHeader.extract(buffer)) {
        return false;
    }

    // Extract operational control field if present
    // if (m_primaryHeader.hasOperationalControl()) {
    if (!m_secondaryHeader.extract(buffer)) {
        return false;
    }
    // }

    // Extract data field
    if (!m_dataField.extract(buffer)) {
        return false;
    }

    // Extract operational control field if present
    // if (m_primaryHeader.hasOperationalControl()) {
    if (!m_operationalControlField.extract(buffer)) {
        return false;
    }
    // }

    // Extract and verify error control field
    return m_errorControlField.extract(buffer);
}

// Getter implementations
template <typename SecondaryHeaderType,
          typename DataFieldType,
          typename OperationalControlFieldType,
          typename ErrorControlFieldType>
PrimaryHeader&
TransferFrameBase<SecondaryHeaderType, DataFieldType, OperationalControlFieldType, ErrorControlFieldType>::
    getPrimaryHeader() {
    return m_primaryHeader;
}

template <typename SecondaryHeaderType,
          typename DataFieldType,
          typename OperationalControlFieldType,
          typename ErrorControlFieldType>
SecondaryHeaderType&
TransferFrameBase<SecondaryHeaderType, DataFieldType, OperationalControlFieldType, ErrorControlFieldType>::
    getSecondaryHeader() {
    return m_secondaryHeader;
}

template <typename SecondaryHeaderType,
          typename DataFieldType,
          typename OperationalControlFieldType,
          typename ErrorControlFieldType>
DataFieldType&
TransferFrameBase<SecondaryHeaderType, DataFieldType, OperationalControlFieldType, ErrorControlFieldType>::
    getDataField() {
    return m_dataField;
}

template <typename SecondaryHeaderType,
          typename DataFieldType,
          typename OperationalControlFieldType,
          typename ErrorControlFieldType>
ErrorControlFieldType&
TransferFrameBase<SecondaryHeaderType, DataFieldType, OperationalControlFieldType, ErrorControlFieldType>::
    getErrorControlField() {
    return m_errorControlField;
}

// template class DataField<FPRIME_VCA_DATA_FIELD_SIZE>;
// Explicit instantiation of the specific TransferFrameBase being used
template class TransferFrameBase<
    NullField,
    FPrimeVCA,
    NullField,
    FrameErrorControlField
>;

}  // namespace TMSpaceDataLink

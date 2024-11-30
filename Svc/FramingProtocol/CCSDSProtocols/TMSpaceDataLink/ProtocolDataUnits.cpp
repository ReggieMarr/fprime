#include "ProtocolDataUnits.hpp"
#include <cstddef>
#include <cstring>

namespace TMSpaceDataLink {

template <FwSizeType FieldSize, typename FieldValueType>
ProtocolDataUnitBase<FieldSize, FieldValueType>::ProtocolDataUnitBase() : m_value() {}

template <FwSizeType FieldSize, typename FieldValueType>
ProtocolDataUnitBase<FieldSize, FieldValueType>::ProtocolDataUnitBase(FieldValueType srcVal) : m_value(srcVal) {}

template <FwSizeType FieldSize, typename FieldValueType>
void ProtocolDataUnitBase<FieldSize, FieldValueType>::set(FieldValueType const& val) {
    m_value = val;
}

template <FwSizeType FieldSize, typename FieldValueType>
void ProtocolDataUnitBase<FieldSize, FieldValueType>::get(FieldValueType& val) const {
    val = m_value;
}

template <FwSizeType FieldSize, typename FieldValueType>
bool ProtocolDataUnitBase<FieldSize, FieldValueType>::insert(Fw::SerializeBufferBase& buffer) const {
    return serializeValue(buffer) == Fw::FW_SERIALIZE_OK;
}

template <FwSizeType FieldSize, typename FieldValueType>
bool ProtocolDataUnitBase<FieldSize, FieldValueType>::insert(Fw::SerializeBufferBase& buffer, FieldValueType& val) {
    FieldValueType lastVal = m_value;
    set(val);
    if (!insert(buffer)) {
        set(lastVal);
        return false;
    }
    return true;
}

template <FwSizeType FieldSize, typename FieldValueType>
bool ProtocolDataUnitBase<FieldSize, FieldValueType>::extract(Fw::SerializeBufferBase& buffer) {
    return deserializeValue(buffer) == Fw::FW_SERIALIZE_OK;
}

template <FwSizeType FieldSize, typename FieldValueType>
bool ProtocolDataUnitBase<FieldSize, FieldValueType>::extract(Fw::SerializeBufferBase& buffer, FieldValueType& val) {
    FieldValueType lastVal = m_value;
    // NOTE in practice this won't actually be handled given the way we currently setup FW_ASSERT
    // TODO configure FW_ASSERT to get something like exception handling here;
    if (!extract(buffer)) {
        m_value = lastVal;
        return false;
    }
    val = m_value;
    return true;
}

template <FwSizeType FieldSize, typename FieldValueType>
ProtocolDataUnitBase<FieldSize, FieldValueType>& ProtocolDataUnitBase<FieldSize, FieldValueType>::operator=(
    const ProtocolDataUnitBase<FieldSize, FieldValueType>& other) {
    if (this != &other) {
        set(other.m_value);
    }
    return *this;
}

// Primitive type template implementations
template <FwSizeType FieldSize, typename FieldValueType>
Fw::SerializeStatus ProtocolDataUnit<FieldSize, FieldValueType>::serializeValue(Fw::SerializeBufferBase& buffer) const {
    return buffer.serialize(this->m_value);
}

template <FwSizeType FieldSize, typename FieldValueType>
Fw::SerializeStatus ProtocolDataUnit<FieldSize, FieldValueType>::deserializeValue(Fw::SerializeBufferBase& buffer) {
    return buffer.deserialize(this->m_value);
}

// Array specialization implementations
template <FwSizeType FieldSize>
Fw::SerializeStatus ProtocolDataUnit<FieldSize, std::array<U8, FieldSize>>::serializeValue(
    Fw::SerializeBufferBase& buffer) const {
    U8 const* buffPtr = this->m_value.data();
    NATIVE_UINT_TYPE buffSize = this->m_value.size();
    return buffer.serialize(buffPtr, buffSize, true);
}

template <FwSizeType FieldSize>
Fw::SerializeStatus ProtocolDataUnit<FieldSize, std::array<U8, FieldSize>>::deserializeValue(
    Fw::SerializeBufferBase& buffer) {
    U8* buffPtr = this->m_value.data();
    NATIVE_UINT_TYPE buffSize = this->m_value.size();
    return buffer.deserialize(buffPtr, buffSize, true);
}

template <FwSizeType FieldSize>
void ProtocolDataUnit<FieldSize, std::array<U8, FieldSize>>::set(FieldValue_t const& val) {
    // To set the internal state we need to have a value that is the same size or smaller
    FW_ASSERT(val.size() <= this->m_value.size(), val.size(), this->m_value.size());
    (void)std::memcpy(this->m_value.data(), val.data(), val.size());
}

template <FwSizeType FieldSize>
void ProtocolDataUnit<FieldSize, std::array<U8, FieldSize>>::get(FieldValue_t& val) const {
    // To set the internal state we need to have a value that is the same size or smaller
    FW_ASSERT(val.size() <= this->m_value.size(), val.size(), this->m_value.size());
    (void)std::memcpy(val.data(), this->m_value.data(), val.size());
}

template <FwSizeType FieldSize>
void ProtocolDataUnit<FieldSize, std::array<U8, FieldSize>>::set(U8 const* buffPtr, FwSizeType size) {
    // To set the internal state we need to have a value that is the same size
    FW_ASSERT(buffPtr != nullptr, size);
    FW_ASSERT(size <= this->m_value.size(), size, this->m_value.size());
    (void)std::memcpy(this->m_value.data(), buffPtr, size);
}

// nullptr specialization implementations
Fw::SerializeStatus ProtocolDataUnit<0, std::nullptr_t>::serializeValue(Fw::SerializeBufferBase& buffer) const {
    return Fw::SerializeStatus::FW_SERIALIZE_OK;
    // FW_ASSERT(0);
    // return Fw::SerializeStatus::FW_SERIALIZE_FORMAT_ERROR;
}

Fw::SerializeStatus ProtocolDataUnit<0, std::nullptr_t>::deserializeValue(Fw::SerializeBufferBase& buffer) {
    // allow this no-op for now
    return Fw::SerializeStatus::FW_SERIALIZE_OK;
    // FW_ASSERT(0);
    // return Fw::SerializeStatus::FW_SERIALIZE_FORMAT_ERROR;
}

void ProtocolDataUnit<0, std::nullptr_t>::set(FieldValue_t const& buffer) {
    FW_ASSERT(0);
}

void ProtocolDataUnit<0, std::nullptr_t>::get(FieldValue_t& buffer) const {
    FW_ASSERT(0);
}

// NOTE this should probably be done on the instatiation side of things
// Instantiate the base class
template class ProtocolDataUnitBase<247, std::array<U8, 247>>;
template class ProtocolDataUnitBase<sizeof(U16), U16>;
template class ProtocolDataUnitBase<6, PrimaryHeaderControlInfo_t>;
template class ProtocolDataUnitBase<0, std::nullptr_t>;

// Instantiate the primary template (if needed)
// FIXME I get linter errors here but they don't seem to affect compilation:
// In template: dependent using declaration resolved to type without 'typename' (lsp)
template class ProtocolDataUnit<247, std::array<U8, 247>>;
template class ProtocolDataUnit<sizeof(U16), U16>;
template class ProtocolDataUnit<6, PrimaryHeaderControlInfo_t>;
template class ProtocolDataUnit<0, std::nullptr_t>;

// If you're using other sizes, add those too, for example:
template class ProtocolDataUnitBase<64, std::array<U8, 64>>;
// FIXME I get linter errors here but they don't seem to affect compilation:
// In template: dependent using declaration resolved to type without 'typename' (lsp)
template class ProtocolDataUnit<64, std::array<U8, 64>>;

}  // namespace TMSpaceDataLink

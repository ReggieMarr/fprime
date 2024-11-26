#include "ManagedParameters.hpp"

namespace Fw {
GlobalVirtualChannelId::GlobalVirtualChannelId()
: m_id({}) {}
    SerializeStatus GlobalVirtualChannelId::serialize(SerializeBufferBase& buffer) const {
    SerializeStatus status;

    // Serialize MCID members
    status = buffer.serialize(static_cast<U16>(m_id.MCID.SCID));
    if (status != SerializeStatus::FW_SERIALIZE_OK) {
        return status;
    }

    status = buffer.serialize(static_cast<U8>(m_id.MCID.TFVN));
    if (status != SerializeStatus::FW_SERIALIZE_OK) {
        return status;
    }

    // Serialize VCID
    status = buffer.serialize(static_cast<U8>(m_id.VCID));
    if (status != SerializeStatus::FW_SERIALIZE_OK) {
        return status;
    }

    return SerializeStatus::FW_SERIALIZE_OK;
    }

    SerializeStatus GlobalVirtualChannelId::deserialize(SerializeBufferBase& buffer) {
    SerializeStatus status;
    U16 scid;
    U8 tfvn;
    U8 vcid;

    // Deserialize and validate MCID members
    status = buffer.deserialize(scid);
    if (status != SerializeStatus::FW_SERIALIZE_OK) {
        return status;
    }
    if (scid > MAX_SCID) {
        return SerializeStatus::FW_SERIALIZE_FORMAT_ERROR;
    }

    status = buffer.deserialize(tfvn);
    if (status != SerializeStatus::FW_SERIALIZE_OK) {
        return status;
    }
    if (tfvn > MAX_TFVN) {
        return SerializeStatus::FW_SERIALIZE_FORMAT_ERROR;
    }

    // Deserialize and validate VCID
    status = buffer.deserialize(vcid);
    if (status != SerializeStatus::FW_SERIALIZE_OK) {
        return status;
    }
    if (vcid > MAX_VCID) {
        return SerializeStatus::FW_SERIALIZE_FORMAT_ERROR;
    }

    // All values valid, assign to member
    m_id.MCID.SCID = scid;
    m_id.MCID.TFVN = tfvn;
    m_id.VCID = vcid;

    return SerializeStatus::FW_SERIALIZE_OK;
    }

}  // namespace Fw

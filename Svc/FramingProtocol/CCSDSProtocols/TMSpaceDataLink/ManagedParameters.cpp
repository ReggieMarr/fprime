#include "ManagedParameters.hpp"
#include "Fw/Types/Assert.hpp"
#include "Fw/Types/Serializable.hpp"

namespace TMSpaceDataLink {
void GlobalVirtualChannelId::toVal(GVCID_t const gvcid, U16& val) {
    // Convert bit fields to a U16
    val |= (static_cast<U16>(m_id.MCID.SCID) << SCID_OFFSET);
    val |= (static_cast<U16>(m_id.MCID.TFVN) << TFVN_OFFSET);
    val |= (static_cast<U16>(m_id.VCID) << VCID_OFFSET);
}

void GlobalVirtualChannelId::fromVal(GVCID_t& gvcid, U16 const val) {
    // Extract fields
    U16 scid = (val >> SCID_OFFSET) & SCID_MASK;
    U8 tfvn = (val >> TFVN_OFFSET) & TFVN_MASK;
    U8 vcid = (val >> VCID_OFFSET) & VCID_MASK;

    // Validate fields
    FW_ASSERT(scid <= MAX_SCID, scid);
    FW_ASSERT(tfvn <= MAX_TFVN, tfvn);
    FW_ASSERT(vcid <= MAX_VCID, vcid);

    // All valid, assign to bit fields
    gvcid.MCID.SCID = scid;
    gvcid.MCID.TFVN = tfvn;
    gvcid.VCID = vcid;
}

void GlobalVirtualChannelId::toVal(GVCID_t const gvcid, U32& val) {
    // Convert bit fields to a U16
    U16 valU16;
    toVal(gvcid, valU16);
    val = valU16 << GVCID_OFFSET;
}

void GlobalVirtualChannelId::fromVal(GVCID_t& gvcid, U32 const val) {
    // Extract fields
    U16 scid = (val >> SCID_OFFSET) & SCID_MASK;
    U8 tfvn = (val >> TFVN_OFFSET) & TFVN_MASK;
    U8 vcid = (val >> VCID_OFFSET) & VCID_MASK;

    // Validate fields
    FW_ASSERT(scid <= MAX_SCID, scid);
    FW_ASSERT(tfvn <= MAX_TFVN, tfvn);
    FW_ASSERT(vcid <= MAX_VCID, vcid);

    // All valid, assign to bit fields
    gvcid.MCID.SCID = scid;
    gvcid.MCID.TFVN = tfvn;
    gvcid.VCID = vcid;
}

}  // namespace TMSpaceDataLink

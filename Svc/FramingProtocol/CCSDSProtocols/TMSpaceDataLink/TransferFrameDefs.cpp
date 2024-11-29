#include "TransferFrameDefs.hpp"

namespace TMSpaceDataLink {

bool MCID_s::operator==(const MCID_s& other) const {
    return SCID == other.SCID && TFVN == other.TFVN;
}

bool GVCID_s::operator==(const GVCID_s& other) const {
    return MCID == other.MCID && VCID == other.VCID;
}

void GVCID_s::toVal(GVCID_s const& gvcid, U16& val) {
    val |= (static_cast<U16>(gvcid.MCID.SCID) << SCID_OFFSET);
    val |= (static_cast<U16>(gvcid.MCID.TFVN) << TFVN_OFFSET);
    val |= (static_cast<U16>(gvcid.VCID) << VCID_OFFSET);
}

void GVCID_s::fromVal(GVCID_s& gvcid, U16 const val) {
    U16 scid = (val >> SCID_OFFSET) & SCID_MASK;
    U8 tfvn = (val >> TFVN_OFFSET) & TFVN_MASK;
    U8 vcid = (val >> VCID_OFFSET) & VCID_MASK;

    FW_ASSERT(scid <= MAX_SCID, scid);
    FW_ASSERT(tfvn <= MAX_TFVN, tfvn);
    FW_ASSERT(vcid <= MAX_VCID, vcid);

    gvcid.MCID.SCID = scid;
    gvcid.MCID.TFVN = tfvn;
    gvcid.VCID = vcid;
}

void GVCID_s::toVal(GVCID_s const& gvcid, U32& val) {
    U16 valU16 = 0;
    toVal(gvcid, valU16);
    val = static_cast<U32>(valU16) << GVCID_OFFSET;
}

void GVCID_s::fromVal(GVCID_s& gvcid, U32 const val) {
    U16 valU16 = static_cast<U16>(val >> GVCID_OFFSET);
    fromVal(gvcid, valU16);
}

}  // namespace TMSpaceDataLink

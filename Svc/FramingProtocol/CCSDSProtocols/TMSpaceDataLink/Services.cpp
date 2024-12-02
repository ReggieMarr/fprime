#include "Services.hpp"
#include "Fw/Com/ComBuffer.hpp"
#include "Fw/Types/String.hpp"
#include "Os/Queue.hpp"

namespace TMSpaceDataLink {
template <typename TemplateCfg>
TMServiceBase<TemplateCfg>::TMServiceBase(typename TemplateCfg::SAP_t const& id) : sap(id) {}

bool VCAService::generatePrimitive(Fw::Buffer& data, VCARequestPrimitive_t& prim) const {
    // NOTE should handle this in some way that respect const
    VCA_SDU_t sdu(data);
    prim.sdu = sdu;
    prim.sap = this->sap;
    // CCSDDS 132.0-B-3 3.4.2.3 provision of this field is mandatory, semantics are optional.
    // Used for a test value for now
    U16 testVal = 0xBEAD;
    prim.statusFields.isPacketOrdered = (testVal >> 15) & 0x1;
    prim.statusFields.segmentLengthId = (testVal >> 13) & 0x7;
    prim.statusFields.firstHeaderPointer = testVal & 0x7FF;
    return true;
}

bool VCFService::generatePrimitive(VCFUserData_t& request, VCFRequestPrimitive_t& prim) const {
    // Set whatever we can at this time provided the context
    // from request.statusData
    PrimaryHeaderControlInfo_t controlInfo;
    prim.frame.primaryHeader.set(controlInfo);
    prim.frame.dataField = request.sdu;
    prim.sap = request.sap;
    return true;
}

template class TMServiceBase<VCAServiceTemplateParams>;
template class TMServiceBase<VCFServiceTemplateParams>;

}  // namespace TMSpaceDataLink

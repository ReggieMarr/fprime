#include "Services.hpp"
#include "Fw/Com/ComBuffer.hpp"
#include "Fw/Types/String.hpp"
#include "Os/Queue.hpp"

namespace TMSpaceDataLink {
template <typename TemplateCfg>
TMServiceBase<TemplateCfg>::TMServiceBase(typename TemplateCfg::SAP_t const& id) : sap(id) {}

bool VCAService::generatePrimitive(Fw::Buffer & data, VCARequestPrimitive_t& prim) const {
    // NOTE should handle this in some way that respect const
    VCA_SDU_t sdu(data);
    prim.sdu = sdu;
    prim.sap = this->sap;
    // // TODO actually handle this
    // prim.statusFields = {};
    return true;
}

bool VCFService::generatePrimitive(VCFUserData_t & request, VCFRequestPrimitive_t& prim) const {
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

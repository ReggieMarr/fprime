#include "Services.hpp"
#include "Fw/Com/ComBuffer.hpp"
#include "Fw/Types/String.hpp"
#include "Os/Queue.hpp"

namespace TMSpaceDataLink {
// template <>
// TMServiceBase<TemplateCfg>::TMService(typename TemplateCfg::IdType const id)
//     : sap(serviceParams.sap), m_serviceParams(serviceParams) {
//     Os::Queue::Status status;
//     m_q.create(serviceName, qDepth, sizeof(ServiceTransferPrimitive_t));
//     FW_ASSERT(status == Os::Queue::Status::OP_OK, status);
// }

template <typename TemplateCfg>
TMServiceBase<TemplateCfg>::TMServiceBase(typename TemplateCfg::SAP_t const &id)
    : sap(id) {}

bool VCAService::generatePrimitive(Fw::Buffer const& data, VCARequestPrimitive_t& prim) const {
    // NOTE should handle this in some way that respect const
    // prim.sdu.set
    // VCA_SDU_t sdu(data);
    // prim.sdu = sdu;
    prim.sap = this->sap;
    // // TODO actually handle this
    // prim.statusFields = {};
    return true;
}

bool VCFService::generatePrimitive(VCFUserData_t const& request, VCFRequestPrimitive_t& prim) const {
    // Set whatever we can at this time provided the context
    PrimaryHeaderControlInfo_t controlInfo;
    FPrimeTransferFrame frame;
    frame.primaryHeader.set(controlInfo);
    // frame.dataField = prim.sdu;
    // frame.setDataField(data.dataField);
    // prim.frame(data)
    // send the frame on the queue here.
}

}  // namespace TMSpaceDataLink

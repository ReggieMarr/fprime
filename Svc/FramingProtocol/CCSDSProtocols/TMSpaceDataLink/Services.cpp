#include "Services.hpp"
#include "Fw/Com/ComBuffer.hpp"
#include "Fw/Types/String.hpp"
#include "Os/Queue.hpp"

namespace TMSpaceDataLink {
template <typename SDU_t,
          typename SAP_t,
          typename ServiceParams_t,
          typename ServiceTransferPrimitive_t,
          SERVICE_TRANSFER_TYPE_t SERVICE_TRANSFER_TYPE>
TMService<SDU_t, SAP_t, ServiceParams_t, ServiceTransferPrimitive_t, SERVICE_TRANSFER_TYPE>::TMService(
    ServiceParams_t const& serviceParams,
    FwSizeType const qDepth)
    : sap(serviceParams.sap), m_serviceParams(serviceParams) {
    Os::Queue::Status status;
    m_serviceParams.primQ.create(serviceName, qDepth, sizeof(ServiceTransferPrimitive_t));
    FW_ASSERT(status == Os::Queue::Status::OP_OK, status);
}

// NOTE this is fine for sync services.
// For async we would want something running its own thread to call this constantly
// and for periodic it would have strict timing relationships
template <typename SDU_t,
          typename SAP_t,
          typename ServiceParams_t,
          typename ServiceTransferPrimitive_t,
          SERVICE_TRANSFER_TYPE_t SERVICE_TRANSFER_TYPE>
bool TMService<SDU_t, SAP_t, ServiceParams_t, ServiceTransferPrimitive_t, SERVICE_TRANSFER_TYPE>::transfer(
    Fw::ComBuffer const& transferBuffer) {
    ServiceTransferPrimitive_t primitive;
    m_serviceParams.primQ.send(transferBuffer);
    return true;
}

}  // namespace TMSpaceDataLink

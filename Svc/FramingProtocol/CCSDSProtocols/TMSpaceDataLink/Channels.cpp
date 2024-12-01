#include "Channels.hpp"
#include <memory>
#include "FpConfig.h"
#include "Fw/Com/ComBuffer.hpp"
#include "Fw/Logger/Logger.hpp"
#include "Fw/Types/Assert.hpp"
#include "Fw/Types/String.hpp"
#include "Os/Models/QueueBlockingTypeEnumAc.hpp"
#include "Os/Models/QueueStatusEnumAc.hpp"
#include "Os/Queue.hpp"
#include "Services.hpp"
#include "Svc/FramingProtocol/CCSDSProtocols/TMSpaceDataLink/ManagedParameters.hpp"
#include "Svc/FramingProtocol/CCSDSProtocols/TMSpaceDataLink/TransferFrame.hpp"
#include "Svc/FramingProtocol/CCSDSProtocols/TMSpaceDataLink/TransferFrameDefs.hpp"

namespace TMSpaceDataLink {
// Default Base Class Transfer Function
template <typename ChannelTemplateConfig>
ChannelBase<ChannelTemplateConfig>::ChannelBase(Id_t id) : m_id(id) {
}

template <typename ChannelTemplateConfig>
ChannelBase<ChannelTemplateConfig>::~ChannelBase() {
}

template <typename ChannelTemplateConfig>
bool ChannelBase<ChannelTemplateConfig>::transfer(TransferIn_t & transferIn) {
    bool status = false;
    Generate_t generateIn;

    status = receive(transferIn, generateIn);
    FW_ASSERT(status);

    status = generate(generateIn);
    FW_ASSERT(status);

    return status;
}

// NOTE based on what we saw in the header I would have assumed this would give the lsp
// autocomplete enough info to pick up the underlying type of Queue_t but that seems not to be the case
static_assert(std::is_same<
    typename VirtualChannel::Queue_t,
    Os::Generic::TransformFrameQueue
                           >::value, "Queue_t type mismatch");

// Constructor for the Virtual Channel Template
VirtualChannel::VirtualChannel(GVCID_t id) : Base(id), m_receiveService(id), m_frameService(id) {
    Os::Queue::Status status;
    Fw::String name = "Channel";
    status = this->m_externalQueue.create(name, CHANNEL_Q_DEPTH);
    FW_ASSERT(status == Os::Queue::Status::OP_OK, status);
}

// Destructor for the Virtual Channel Template
VirtualChannel::~VirtualChannel() {
}

// Note we could probably just make this a pass through then
bool VirtualChannel::receive(Fw::Buffer & buffer, VCAService::Primitive_t& result) {
    return m_receiveService.generatePrimitive(buffer, result);
}

bool VirtualChannel::generate(VCFUserData_t & arg) {
    FW_ASSERT(arg.sap == this->m_id);
    VCFRequestPrimitive_t prim;
    bool status;
    status = m_frameService.generatePrimitive(arg, prim);
    FW_ASSERT(status);
    this->m_externalQueue.send(prim.frame, this->blockType, this->priority);
    return true;
}

// NOTE based on what we saw in the header I would have assumed this would give the lsp
// autocomplete enough info to pick up the underlying type of Queue_t but that seems not to be the case
// static_assert(std::is_same<
//     typename MasterChannel::Queue_t,
//     Os::Generic::TransformFrameQueue
//                            >::value, "Queue_t type mismatch");

// // Constructor for the Master Channel Template
// MasterChannel::MasterChannel(GVCID_t id) : Base(id) {
//     Os::Queue::Status status;
//     Fw::String name = "Channel";
//     status = this->m_externalQueue.create(name, CHANNEL_Q_DEPTH);
//     FW_ASSERT(status == Os::Queue::Status::OP_OK, status);
// }

// // Destructor for the Master Channel Template
// MasterChannel::~MasterChannel() {
// }

// // First, define the actual function implementations
// bool MasterChannel::receive(std::nullptr_t _, VCAService::Primitive_t& result) {
//     FPrimeTransferFrame frame;
// }

// bool MasterChannel::generate(VCFUserData_t & arg) {
//     FW_ASSERT(arg.sap == this->m_id);
//     VCFRequestPrimitive_t prim;
//     bool status;
//     FW_ASSERT(status);
//     this->m_externalQueue.send(prim.frame, this->blockType, this->priority);
//     return true;
// }


// Virtual Channel instantiations
// Instantiate the parameter config template
template class ChannelParameterConfig<
    Fw::Buffer,
    VCARequestPrimitive_t,                 // Generate_t
    VCFServiceTemplateParams::Primitive_t,
    Os::Generic::TransformFrameQueue,
    VCAServiceTemplateParams::SAP_t>;

// Instantiate the base channel template with your params
template class ChannelBase<VirtualChannelParams>;

// Additional explicit instantiation for Master Channel
// template class ChannelBase<MasterChannelConfig, MasterChannelFunctions>;

}  // namespace TMSpaceDataLink

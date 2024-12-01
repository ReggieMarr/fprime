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
bool ChannelBase<ChannelTemplateConfig>::transfer(TransferIn_t const& transferIn) {
    bool status = false;
    Receive_t receiveIn;
    Generate_t generateIn;

    status = receive(receiveIn, generateIn);
    FW_ASSERT(status);

    status = generate(generateIn);
    FW_ASSERT(status);

    return status;
}

// Base Class Instatiation for the Virtual Channel Template
VirtualChannel::VirtualChannel(GVCID_t id) : Base(id), m_receiveService(id), m_frameService(id) {
    Os::Queue::Status status;
    Fw::String name = "Channel";
    status = this->m_externalQueue.create(name, CHANNEL_Q_DEPTH);
    FW_ASSERT(status == Os::Queue::Status::OP_OK, status);
}

// First, define the actual function implementations
bool VirtualChannel::receive(Fw::Buffer const& buffer, VCAService::Primitive_t& result) {
    // Implementation
    // This is the same thing as Fw::ComBuffer
    // VCAService::UserData_t userData(com);
    return m_receiveService.generatePrimitive(buffer, result);
}

bool VirtualChannel::generate(const VCAService::Primitive_t& arg) {
    // Implementation
    return true;
}

// using MasterChannelConfig = ChannelConfig<FPrimeTransferFrame,                                    // TransferIn
//                                           std::array<FPrimeTransferFrame, MAX_VIRTUAL_CHANNELS>,  // TransferOut
//                                           Os::Generic::TransformFrameQueue,                       // Queue
//                                           MCID_t                                                  // Id
//                                           >;

// using MasterChannelFunctionArgs =
//     ChannelFunctionArgs<FPrimeTransferFrame,                                    // ReceiveArg
//                         std::array<FPrimeTransferFrame, MAX_VIRTUAL_CHANNELS>,  // GenerateArg
//                         FPrimeTransferFrame                                     // PropogateArg
//                         >;

// using MasterChannelFunctions = ChannelFunctionConfig<MasterChannelFunctionArgs>;

// // Master Channel function implementations
// namespace ChannelFunctions {
// bool masterChannelReceive(const FPrimeTransferFrame& arg,
//                           std::array<FPrimeTransferFrame, MAX_VIRTUAL_CHANNELS>& result) {
//     // Implementation
//     return true;
// }

// bool masterChannelGenerate(const std::array<FPrimeTransferFrame, MAX_VIRTUAL_CHANNELS>& arg,
//                            FPrimeTransferFrame& result) {
//     // Implementation
//     return true;
// }

// bool masterChannelPropogate(const FPrimeTransferFrame& arg) {
//     // Implementation
//     return true;
// }
// }  // namespace ChannelFunctions

// class MasterChannel : public ChannelBase<MasterChannelConfig, MasterChannelFunctions> {
//   public:
//     using Base = ChannelBase<MasterChannelConfig, MasterChannelFunctions>;

//     MasterChannel(Id_t id)
//         : Base(id,
//                &ChannelFunctions::masterChannelReceive,
//                &ChannelFunctions::masterChannelGenerate,
//                &ChannelFunctions::masterChannelPropogate) {}

//   protected:
//     void transferToReceive(FPrimeTransferFrame& transferIn, FPrimeTransferFrame& receiveIn) override {
//         // Implementation
//     }
// };

// Virtual Channel instantiations
template class ChannelBase<VirtualChannelParams>;
// Additional explicit instantiation for Master Channel
// template class ChannelBase<MasterChannelConfig, MasterChannelFunctions>;

}  // namespace TMSpaceDataLink

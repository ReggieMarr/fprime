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
template <>
ChannelBase<VirtualChannelParams>::ChannelBase(GVCID_t id)
: m_id(id) {
    Os::Queue::Status status;
    Fw::String name = "Channel";
    status = m_externalQueue.create(name, CHANNEL_Q_DEPTH);
    FW_ASSERT(status == Os::Queue::Status::OP_OK, status);
}

// First, define the actual function implementations
bool VirtualChannel::receive(const VCAService::UserData_t& arg, VCAService::Primitive_t& result) {
    // Implementation
    // receiveService
    return true;
}

bool VirtualChannel::generate(const VCAService::Primitive_t& arg, FPrimeTransferFrame& result) {
    // Implementation
    return true;
}

bool VirtualChannel::propogate(const FPrimeTransferFrame& arg) {
    // Implementation
    return true;
}

// Implement VirtualChannel methods
void VirtualChannel::transferToReceive(Fw::ComBuffer const& transferIn, VCAService::UserData_t& receiveIn) {
    // Implementation to convert ComBuffer to UserData_t
}

// Constructor implementation
// VirtualChannel::VirtualChannel(Id_t id)
//     : Base(id,
//            &ChannelFunctions::virtualChannelReceive,
//            &ChannelFunctions::virtualChannelGenerate,
//            &ChannelFunctions::virtualChannelPropogate) {}

// Base class template implementations
// template <typename ChannelConfig, typename FunctionConfig>
// bool ChannelBase<ChannelConfig, FunctionConfig>::receive(typename FunctionConfig::FnArgs::ReceiveArg const& arg,
//                                                          typename FunctionConfig::FnArgs::GenerateArg& result) {
//     return m_receiveFn(arg, result);
// }

// template <typename ChannelConfig, typename FunctionConfig>
// bool ChannelBase<ChannelConfig, FunctionConfig>::generate(typename FunctionConfig::FnArgs::GenerateArg const& arg,
//                                                           typename FunctionConfig::FnArgs::PropogateArg& result) {
//     return m_generateFn(arg, result);
// }

// template <typename ChannelConfig, typename FunctionConfig>
// bool ChannelBase<ChannelConfig, FunctionConfig>::propogate(typename FunctionConfig::FnArgs::PropogateArg const& arg)
// {
//     return m_propogateFn(arg);
// }

// If you have Master Channel configurations:
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

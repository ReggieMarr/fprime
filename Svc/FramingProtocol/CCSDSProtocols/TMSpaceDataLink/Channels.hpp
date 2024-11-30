#ifndef SVC_TM_SPACE_DATA_LINK_CHANNELS_HPP
#define SVC_TM_SPACE_DATA_LINK_CHANNELS_HPP

#include <Svc/FramingProtocol/FramingProtocol.hpp>
#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <queue>
#include <vector>
#include "FpConfig.h"
#include "Fw/Com/ComBuffer.hpp"
#include "Fw/Logger/Logger.hpp"
#include "Fw/Types/Assert.hpp"
#include "Fw/Types/SerialStatusEnumAc.hpp"
#include "Fw/Types/Serializable.hpp"
#include "Fw/Types/String.hpp"
#include "ManagedParameters.hpp"
#include "Os/Queue.hpp"
#include "Svc/FrameAccumulator/FrameDetector/StartLengthCrcDetector.hpp"
#include "Svc/FramingProtocol/CCSDSProtocols/CCSDSProtocolDefs.hpp"
#include "Svc/FramingProtocol/CCSDSProtocols/TMSpaceDataLink/ManagedParameters.hpp"
#include "Svc/FramingProtocol/CCSDSProtocols/TMSpaceDataLink/Services.hpp"
#include "Svc/FramingProtocol/CCSDSProtocols/TMSpaceDataLink/TransferFrame.hpp"
#include "Svc/FramingProtocol/CCSDSProtocols/TMSpaceDataLink/TransferFrameDefs.hpp"
#include "Svc/FramingProtocol/CCSDSProtocols/TMSpaceDataLink/TransferFrameQueue.hpp"
#include "TransferFrame.hpp"

namespace TMSpaceDataLink {
constexpr FwSizeType CHANNEL_Q_DEPTH = 10;
static constexpr FwSizeType TRANSFER_FRAME_SIZE = 255;

template <typename ChannelType, FwSizeType ChannelSize>
using ChannelList = std::array<ChannelType, ChannelSize>;

// template <typename ReceiverType, typename ProcessorType, typename TransmitType>
// struct ChannelObjectConfig<ReceiverType, ProcessorType, TransmitType> {
//     using Receiver = ReceiverType;
//     using Processor = ProcessorType;
//     using Transmiter = TransmitType;
// };

// // This is a shim layer that works both for channels which take in user data and those which
// // are downstream of other channels
// template <typename TransferInType, typename TransferOutType, typename IdType, typename QueueType>
// struct ChannelObjectConfig<TransferInType, TransferOutType, IdType, QueueType> {
//     using TransferIn_t = TransferInType;
//     using TransferOut_t = TransferOutType;
//     using Id_t = IdType;
//     using Queue_t = QueueType;
// };

// template <typename... Types>
// struct ChannelFnConfig;

// template <typename ArgIn, typename ReceiverType, typename ProcessorType>
// struct ChannelFnConfig<ArgIn, ReceiverType, ProcessorType> {
//     using Arg = ArgIn;
//     using Receiver = ReceiverType;
//     using Processor = ProcessorType;
// };

// template <typename ArgIn, typename ArgOut, typename ReceiverType, typename ProcessorType>
// struct ChannelFnConfig<ArgIn, ArgOut, ReceiverType, ProcessorType> {
//     using Arg = ArgIn;
//     using Result = ArgOut;
//     using Receiver = ReceiverType;
//     using Processor = ProcessorType;
// };

// // Function templates
// template <typename Config>
// bool ChannelTransferFn(typename Config::Arg);

// template <typename Config>
// bool ChannelGenerationFn(typename Config::Arg, typename Config::Result&);

// template <typename Config>
// bool ChannelMultiplexingFn(typename Config::Arg, typename Config::Result&);

// // This is used for channels which accept user data (i.e. raw data). This assumes
// // the channel will have at least one service to receive, process, and/or transmit data.
// // this is meant to be the case where we're passing Receiver, Processor, Transmitter
// using VCA_VCF_Procedure = ChannelObjectConfig<VCAService, VCFService, Os::Generic::TransformFrameQueue>;

// using VCA_VFC_ChannelConfig = ChannelObjectConfig<VCA_VCF_Procedure::Receiver::UserData_t,
//                                                   VCA_VCF_Procedure::Processor::Primitive_t,
//                                                   VCA_VCF_Procedure::Processor::SAP_t,
//                                                   VCA_VCF_Procedure::Transmiter>;

// using VCA_VCF_TransferFnCfg =
//     ChannelFnConfig<VCA_VCF_Procedure::Receiver, VCA_VCF_Procedure::Processor, Os::Generic::TransformFrameQueue>;

// using VCA_VCF_ChannelGenerationFnCfg = ChannelFnConfig<VCA_VCF_Procedure::Receiver::Primitive_t,
//                                                        VCA_VCF_Procedure::Processor::Primitive_t,
//                                                        Os::Generic::TransformFrameQueue>;

// using VCA_VCF_MultiplexingFnCfg = ChannelFnConfig<std::nullptr_t, std::nullptr_t, std::nullptr_t>;

// Function type definitions
// Takes some input, processes it and returns the result via the Result reference.
// Success is indicated by the boolean return value.
template <typename Arg, typename Result>
using ChannelProcessingFn = bool (*)(const Arg&, Result&);

// Takes some input, consumes it and does not return a result.
// Success is indicated by the boolean return value.
template <typename Arg>
using ChannelConsumingFn = bool (*)(const Arg&);

template <typename ReceiveArgType, typename GenerateArgType, typename PropogateArgType>
struct ChannelFunctionArgs {
    using ReceiveArg = ReceiveArgType;
    using GenerateArg = GenerateArgType;
    using PropogateArg = PropogateArgType;
};

// Channel function configurations bundle
template <typename Args>
struct ChannelFunctionConfig {
    using FnArgs = Args;
    using ReceiveFn = ChannelProcessingFn<typename Args::ReceiveArg, typename Args::GenerateArg>;
    using GenerateFn = ChannelProcessingFn<typename Args::GenerateArg, typename Args::PropogateArg>;
    using PropogateFn = ChannelConsumingFn<typename Args::PropogateArg>;
};

template <typename FnArgTypes,
          typename TransferInType,
          typename TransferOutType,
          typename QueueType,
          typename IdType>
struct ChannelParameterConfig {
    using FnArgs = FnArgTypes;
    using TransferIn_t = TransferInType;
    using TransferOut_t = TransferOutType;
    using Queue_t = QueueType;
    using Id_t = IdType;
};

template <typename ChannelTemplateConfig>
class ChannelBase {
  private:
    using FnArgs = typename ChannelTemplateConfig::FnArgs;
  public:
    // Channel type definitions
    using TransferOut_t = typename ChannelTemplateConfig::TransferOut_t;
    using TransferIn_t = typename ChannelTemplateConfig::TransferIn_t;
    using Queue_t = typename ChannelTemplateConfig::Queue_t;
    using Id_t = typename ChannelTemplateConfig::Id_t;

    // Function Arg type definitions.
    // Note that the receive function receives something that
    // can come from the TransformIn_t. Its result becomes the argument
    // for the generate function. The generates result likewise becomes
    // the argument for the propogate function which consumes the argument
    // and sends it on its way.
    using Receive_t = typename FnArgs::ReceiveArg;
    using Generate_t = typename FnArgs::GenerateArg;
    using Propogate_t = typename FnArgs::PropogateArg;

  protected:
    // Member variables
    Id_t m_id;
    Queue_t m_externalQueue;
    U8 m_channelTransferCount{0};

    // NOTE we should probably get rid of this at some point, for now its used
    // to convert the TransferIn_t to FnArgs::ReceiveArg. These cant be the same at the moment
    // since different transfer functions/receive functions dont always line up
    virtual void transferToReceive(TransferIn_t const& arg, Receive_t& result) = 0;

    virtual bool receive(Receive_t const& arg, Generate_t& result) = 0;

    virtual bool generate(Generate_t const& arg, Propogate_t& result) = 0;

    virtual bool propogate(Propogate_t const& arg) = 0;
    // ReceiveFn m_receiveFn;
    // GenerateFn m_generateFn;
    // PropogateFn m_propogateFn;

  public:
    ChannelBase(Id_t id);
    virtual ~ChannelBase();

    ChannelBase(const ChannelBase& other)
        : m_id(other.m_id),
        m_channelTransferCount(other.m_channelTransferCount) {
          // m_receiveFn(other.m_receiveFn),
          // m_generateFn(other.m_generateFn),
          // m_propogateFn(other.m_propogateFn) {
        FW_ASSERT(!other.m_externalQueue.getMessagesAvailable());
        Os::Queue::Status status;
        Fw::String name = "Channel";
        status = m_externalQueue.create(name, CHANNEL_Q_DEPTH);
        FW_ASSERT(status == Os::Queue::Status::OP_OK, status);
    }

    virtual bool transfer(TransferIn_t const& transferIn) {
        bool status = false;
        Receive_t receiveIn;
        Generate_t generateIn;
        Propogate_t propogateIn;

        transferToReceive(transferIn, receiveIn);

        status = receive(receiveIn, generateIn);
        FW_ASSERT(status);

        status = generate(generateIn, propogateIn);
        FW_ASSERT(status);

        status = propogate(propogateIn);
        FW_ASSERT(status);

        return status;
    }

};

using VirtualChannelFunctionArgs = ChannelFunctionArgs<
    // ReceiveArg
    VCAService::UserData_t,
    // GenerateArg
    VCAService::Primitive_t,
    // PropogateArg
    FPrimeTransferFrame>;

using VirtualChannelParams = ChannelParameterConfig<VirtualChannelFunctionArgs, // Template Struct containing the type info for the
                                                                                // transfer, receive, generate, and propogate functions.
                                           Fw::ComBuffer,                       // TransferIn_t
                                           FPrimeTransferFrame,                 // TransferOut_t
                                           Os::Generic::TransformFrameQueue,    // Queue_t
                                           GVCID_t                              // Id_t
                                           >;



using VirtualChannelFunctions = ChannelFunctionConfig<VirtualChannelFunctionArgs>;

typedef struct ChannelCfg_s {
  VirtualChannelParams params;
  VirtualChannelFunctionArgs fnArgs;
} ChannelCfg_t;

class VirtualChannel : public ChannelBase<VirtualChannelParams> {
  protected:
    VCAService receiveService;
    VCFService frameService;
    virtual void transferToReceive(Fw::ComBuffer const& transferIn, VCAService::UserData_t& receiveIn) override;
  public:
    using Base = ChannelBase<VirtualChannelParams>;
    using typename Base::Generate_t;
    using typename Base::Id_t;
    using typename Base::Propogate_t;
    using typename Base::Queue_t;
    using typename Base::Receive_t;
    using typename Base::TransferIn_t;
    using typename Base::TransferOut_t;

    // NOTE why can't I mark these as override
    virtual bool receive(VCAService::UserData_t const &data, VCAService::Primitive_t &packet);
    virtual bool generate(VCAService::Primitive_t const &packet, FPrimeTransferFrame &frame);
    virtual bool propogate(FPrimeTransferFrame const &frame);
};

// // Virtual Channel configurations
// struct VirtualChannelConfig {
//     using TransferIn_t = VCAService::UserData_t;
//     using TransferOut_t = VCFService::Primitive_t;
//     using Queue_t = Os::Generic::TransformFrameQueue;
//     using Id_t = GVCID_t;
// };

// using VirtualChannelFunctionConfig = ChannelFunctionConfig<VCAService::UserData_t, VCFService::UserData_t,
// VCFService::Primitive_t>;

// using VirtualReceiveConfig = FunctionConfig<VCAService::UserData_t, VCFService::Primitive_t>;

// using VirtualGenerateConfig = FunctionConfig<VCFService::Primitive_t, FPrimeTransferFrame>;

// using VirtualPropogateConfig = FunctionConfig<FPrimeTransferFrame>;

// // This is used for channels which accept user data (i.e. raw data). This assumes
// // the channel will have at least one service to receive, process, and/or transmit data.
// // Channel implementations
// class VirtualChannel : public ChannelBase<VirtualChannelConfig, VirtualChannelFunctionConfig> {
//   public:
//     using Base = ChannelBase<VirtualChannelConfig, VirtualChannelFunctionConfig>;
//     using Base::ChannelBase;
// };

// class MasterChannel : public ChannelBase<MasterChannelConfig, MasterChannelFunctionConfig> {
//   public:
//     using Base = ChannelBase<MasterChannelConfig, MasterChannelFunctionConfig>;
//     using Base::ChannelBase;
// };

// // Physical Channel Implementation
// class PhysicalChannel : public BaseMasterChannel<MasterChannel,
//                                                  typename MasterChannel::TransferOut_t,
//                                                  std::array<typename MasterChannel::TransferOut_t, 3>,
//                                                  Fw::String,
//                                                  MAX_MASTER_CHANNELS> {
//   public:
//     // Inherit parent's type definitions
//     using Base = BaseMasterChannel<MasterChannel,
//                                    typename MasterChannel::TransferOut_t,
//                                    std::array<typename MasterChannel::TransferOut_t, 3>,
//                                    Fw::String,
//                                    MAX_MASTER_CHANNELS>;
//     //
//     // Inherit constructors
//     using Base::BaseMasterChannel;

//     // not sure if this is needed
//     using TransferInType = typename Base::TransferIn_t;
//     using TransferOutType = typename Base::TransferOut_t;

//     using Base::operator=;

//     bool getChannel(MCID_t const mcid, NATIVE_UINT_TYPE& channelIdx);

//     VirtualChannel& getChannel(GVCID_t const gvcid);

//     bool transfer();

//   private:
//     bool MasterChannelMultiplexing_handler(TransferIn_t& masterChannelOut,
//                                            NATIVE_UINT_TYPE mcid,
//                                            TransferOut_t& physicalChannelOut);

//     bool AllFramesChannelGeneration_handler(TransferOut_t& masterChannelTransfer);
// };

}  // namespace TMSpaceDataLink

#endif  // SVC_TM_SPACE_DATA_LINK_CHANNELS_HPP

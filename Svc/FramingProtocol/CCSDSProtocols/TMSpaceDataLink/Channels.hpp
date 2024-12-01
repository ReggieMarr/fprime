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
#include "TransferFrameDefs.hpp"

namespace TMSpaceDataLink {
constexpr FwSizeType CHANNEL_Q_DEPTH = 10;
static constexpr FwSizeType TRANSFER_FRAME_SIZE = 255;

template <typename ChannelType, FwSizeType ChannelSize>
using ChannelList = std::array<ChannelType, ChannelSize>;

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

template <typename FnArgTypes, typename TransferInType, typename TransferOutType, typename QueueType, typename IdType>
struct ChannelParameterConfig {
    using FnArgs = FnArgTypes;
    using TransferIn_t = TransferInType;
    using TransferOut_t = TransferOutType;
    using Queue_t = QueueType;
    using Id_t = IdType;
};

// NOTE it might make sense to make each Channel it's own Queued Component
// TODO look into Feasability of the following:
// VirtualChannel (Active Componenet) ->
// -> MasterChannel[(Guarded)Port] (Queued Component) ->
// -> PhysicalChannel[(Guarded)Port] (Queued Component) ->
// -> ComDriver
// NOTE the general desire here is to generate channel template classes that are specialized
// based on the following
// a) The services it uses.
// b) The thing it propogates/its properties (e.g. TransferFrames and their length, or Channel queues/quantity)
// As it stands effectively what we ask the following:
// - What is the nature of the transfer ?
//    - i.e. passed user data (sync or async) vs triggered (periodic, async)
// - What does the receive function "Receive" and
// How does it process it into a request to be consumed by the generate functon.
//    - e.g. a Buffer of user data processed into a TransferFrame Data Field using a private method
// - What does the generate function "Generate" and how is that propogated ?
//    - e.g. a Data Field and other context used to create the start of a transfer frame, propogated via a queue.
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

    // NOTE consider collect/emit instead of receive/generate

    // This receives data from either the user that has access to it or the
    // subChannels it has access to.
    // In the former case, if using VCA we packetize according to some user provided class.
    // otherwise we convert the data into either Space Packets (CCSDS 133.0-B-2) or as
    // Encapsulation Packets (CCSDS 133.1-B-3).
    // The result, retrieved by reference, will be passed onto the generate function.
    virtual bool receive(TransferIn_t const& arg, Generate_t& result) = 0;

    // This receives packetized data and generates it.
    // If there are frame field services associated with this channel (such as the OSF or FSH services)
    // then we leverage them to set fields. If there is a Framing service (Such as VCF) then we create the frame.
    // At the end generate frame function propogates its result to the consumer of this channel.
    virtual bool generate(Generate_t const& arg) = 0;

  public:
    ChannelBase(Id_t id);
    virtual ~ChannelBase();

    virtual bool transfer(TransferIn_t const& transferIn);
};

// FIXME could probably just move the
// generate arg into channel params and then just get rid of this
// and the associated complexity.
// Re-evaluate this after implementing the master and physical channel
using VirtualChannelFunctionArgs = ChannelFunctionArgs<
    // ReceiveArg
    // NOTE we could probably eliminate this since its just the
    // VCAService::UserData_t,
    Fw::Buffer,
    // GenerateArg
    VCAService::Primitive_t,
    // PropogateArg
    FPrimeTransferFrame>;

using VirtualChannelParams =
    ChannelParameterConfig<VirtualChannelFunctionArgs,        // Template Struct containing the type info for the
                                                              // transfer, receive, generate, and propogate functions.
                           Fw::Buffer,                        // TransferIn_t
                           FPrimeTransferFrame,               // TransferOut_t
                           Os::Generic::TransformFrameQueue,  // Queue_t
                           GVCID_t                            // Id_t
                           >;

using VirtualChannelFunctions = ChannelFunctionConfig<VirtualChannelFunctionArgs>;

typedef struct ChannelCfg_s {
    VirtualChannelParams params;
    VirtualChannelFunctionArgs fnArgs;
} ChannelCfg_t;

// NOTE could be really called VirtualChannelQueueFrameAccess
class VirtualChannel : public ChannelBase<VirtualChannelParams> {
  protected:
    VCAService m_receiveService;
    VCFService m_frameService;

  public:
    using Base = ChannelBase<VirtualChannelParams>;
    using Base::ChannelBase;  // Inherit constructor
    using typename Base::Generate_t;
    using typename Base::Id_t;
    using typename Base::Propogate_t;
    using typename Base::Queue_t;
    using typename Base::Receive_t;
    using typename Base::TransferIn_t;
    using typename Base::TransferOut_t;

    VirtualChannel(GVCID_t id);

    // TODO figure out why can't I mark these as override
    virtual bool receive(VCAService::UserData_t const& data, VCAService::Primitive_t& packet);
    virtual bool generate(VCAService::Primitive_t const& packet);
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

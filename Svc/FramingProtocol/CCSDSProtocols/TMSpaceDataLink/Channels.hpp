#ifndef SVC_TM_SPACE_DATA_LINK_CHANNELS_HPP
#define SVC_TM_SPACE_DATA_LINK_CHANNELS_HPP

#include <Os/Generic/PriorityQueue.hpp>
#include <Svc/FramingProtocol/FramingProtocol.hpp>
#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <queue>
#include <type_traits>
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
#include "TransferFrame.hpp"
#include "TransferFrameDefs.hpp"

namespace TMSpaceDataLink {
constexpr FwSizeType CHANNEL_Q_DEPTH = 10;

template <typename TransferInType, typename TransferOutType, typename QueueType, typename IdType>
struct ChannelParameterConfig {
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
  public:
    // Channel type definitions
    using TransferIn_t = typename ChannelTemplateConfig::TransferIn_t;
    using Queue_t = typename ChannelTemplateConfig::Queue_t;
    using Id_t = typename ChannelTemplateConfig::Id_t;
    using TransferOut_t = typename ChannelTemplateConfig::TransferOut_t;

  protected:
    // Member variables
    U8 m_channelTransferCount{0};
    // TODO set m_priority based on something user driven
    FwQueuePriorityType m_priority = 0;
    Os::QueueInterface::BlockingType m_blockType = Os::QueueInterface::BlockingType::BLOCKING;

    // NOTE consider collect/emit instead of receive/generate

    // This receives data from either the user that has access to it or the
    // subChannels it has access to.
    // In the former case, if using VCA we packetize according to some user provided class.
    // otherwise we convert the data into either Space Packets (CCSDS 133.0-B-2) or as
    // Encapsulation Packets (CCSDS 133.1-B-3).
    // The result, retrieved by reference, will be passed onto the generate function.
    virtual bool receive(TransferIn_t& arg, TransferOut_t& result) = 0;

    // This receives packetized data and generates it.
    // If there are frame field services associated with this channel (such as the OSF or FSH services)
    // then we leverage them to set fields. If there is a Framing service (Such as VCF) then we create the frame.
    // At the end generate frame function propogates its result to the consumer of this channel.
    virtual bool generate(TransferOut_t& arg) = 0;

    Fw::ComBuffer serialBuffer;
    bool pullFrame(Queue_t& queue, FPrimeTransferFrame& frame);
    bool pushFrame(Queue_t& queue, FPrimeTransferFrame& frame);

  public:
    Id_t id;
    Queue_t m_externalQueue;

    ChannelBase(const ChannelBase& other);
    ChannelBase(Id_t const& id);

    virtual ChannelBase& operator=(const ChannelBase& other);
    virtual ~ChannelBase();

    virtual bool transfer(TransferIn_t& transferIn);
};

using VirtualChannelParams = ChannelParameterConfig<Fw::Buffer,  // TransferIn_t
                                                    VCAServiceTemplateParams::Primitive_t,
                                                    Os::Generic::PriorityQueue,  // Queue_t
                                                    // NOTE this comes from either the VCA or VCF SAP_t
                                                    // VCA was chosen arbitrarily
                                                    VCAServiceTemplateParams::SAP_t  // Id_t
                                                    >;

// NOTE could be really called VirtualChannelQueueFrameAccess
using VirtualChannelConfig = VirtualChannelParams;
// This is used for channels which accept user data (i.e. raw data). This assumes
// the channel will have at least one service to receive, process, and/or transmit data.
using VirtualChannelBase = ChannelBase<VirtualChannelConfig>;

// NOTE this is here for extra safety and also for linting
// lsp doesn't recognize VirtualChannel as a ChannelBase derived class otherwise
static_assert(std::is_same<VirtualChannelBase::TransferIn_t, Fw::Buffer>::value, "TransferIn_t type mismatch");

static_assert(std::is_same<VirtualChannelBase::TransferOut_t, VCARequestPrimitive_t>::value,
              "TransferOut_t type mismatch");

class VirtualChannel : public VirtualChannelBase {
  protected:
    VCAService m_receiveService;
    VCFService m_frameService;

  public:
    using Base = VirtualChannelBase;
    using Base::Base;  // Inherit all public members
    using typename Base::Id_t;
    using typename Base::Queue_t;
    using typename Base::TransferIn_t;
    using typename Base::TransferOut_t;

    VirtualChannel(Id_t const& id);
    ~VirtualChannel();

  protected:
    virtual bool receive(TransferIn_t& arg, TransferOut_t& result) override;
    virtual bool generate(TransferOut_t& arg) override;
};

template <typename ChannelType, FwSizeType ChannelSize>

// Used because the queues cant delete
// TODO I had a workaround earlier but I cant remember what that was right now so just use this
// using ChannelList = std::array<std::reference_wrapper<ChannelType>, ChannelSize>;
using ChannelList = std::array<ChannelType, ChannelSize>;

template <FwSizeType ChannelNumber, typename ChannelType>
struct SubChannelParams {
    static constexpr FwSizeType ChannelNum = ChannelNumber;
    using Channel_t = ChannelType;
};

using MasterChannelSubChannelParams = SubChannelParams<NUM_VIRTUAL_CHANNELS, VirtualChannel>;

using MasterChannelGenerateType = std::array<FPrimeTransferFrame, MasterChannelSubChannelParams::ChannelNum>;

using MasterChannelParams =
    // NOTE this is potentially unsafe
    // TODO look into a better mechanism
    ChannelParameterConfig<std::nullptr_t,              // TransferIn_t
                           MasterChannelGenerateType,   // TransferOut_t
                           Os::Generic::PriorityQueue,  // Queue_t
                           // NOTE this comes from either the VCA or VCF SAP_t
                           // VCA was chosen arbitrarily
                           // Id_t
                           MCID_t>;

// NOTE could be really called MasterChannelQueueFrameAccess
using MasterChannelConfig = MasterChannelParams;
// This is used for channels which accept user data (i.e. raw data). This assumes
// the channel will have at least one service to receive, process, and/or transmit data.
using MasterChannelBase = ChannelBase<MasterChannelConfig>;

// NOTE this is here for extra safety and also for linting
// lsp doesn't recognize MasterChannel as a ChannelBase derived class otherwise
static_assert(std::is_same<MasterChannelBase::TransferIn_t, std::nullptr_t>::value, "TransferIn_t type mismatch");

static_assert(std::is_same<MasterChannelBase::TransferOut_t, MasterChannelGenerateType>::value,
              "TransferOut_t type mismatch");

template <FwSizeType NumSubChannels>
class MasterChannel : public MasterChannelBase {
  public:
    using Base = MasterChannelBase;
    using Base::Base;
    using typename Base::Id_t;
    using typename Base::Queue_t;
    using typename Base::TransferIn_t;
    using typename Base::TransferOut_t;

    using VirtualChannelList = ChannelList<VirtualChannel, NumSubChannels>;
    using Channel_t = VirtualChannel;

    VirtualChannel& getChannel(GVCID_t const gvcid);

    MasterChannel(Id_t const& id, VirtualChannelList& subChannels);
    ~MasterChannel();

    // NOTE should use getter for this
    VirtualChannelList m_subChannels;

  protected:
    virtual bool receive(TransferIn_t& arg, TransferOut_t& result) override;
    virtual bool generate(TransferOut_t& frame) override;
};

// NOTE this is a temporary value that represents the most common implementation
using SingleMasterChannel = MasterChannel<NUM_VIRTUAL_CHANNELS>;

using PhysicalChannelSubChannelParams = SubChannelParams<1, SingleMasterChannel>;

using PhysicalChannelGenerateType = std::array<FPrimeTransferFrame, PhysicalChannelSubChannelParams::ChannelNum>;

using PhysicalChannelParams =
    // NOTE this is potentially unsafe
    // TODO look into a better mechanism
    ChannelParameterConfig<std::nullptr_t,  // TransferIn_t
                           PhysicalChannelGenerateType,
                           Os::Generic::PriorityQueue,  // Queue_t
                           // NOTE this comes from either the VCA or VCF SAP_t
                           // VCA was chosen arbitrarily
                           // Id_t
                           Fw::String>;

// NOTE could be really called PhysicalChannelQueueFrameAccess
using PhysicalChannelConfig = PhysicalChannelParams;
// This is used for channels which accept user data (i.e. raw data). This assumes
// the channel will have at least one service to receive, process, and/or transmit data.
using PhysicalChannelBase = ChannelBase<PhysicalChannelConfig>;

// NOTE this is here for extra safety and also for linting
// lsp doesn't recognize PhysicalChannel as a ChannelBase derived class otherwise
static_assert(std::is_same<PhysicalChannelBase::TransferIn_t, std::nullptr_t>::value, "TransferIn_t type mismatch");

static_assert(std::is_same<PhysicalChannelBase::TransferOut_t, PhysicalChannelGenerateType>::value,
              "TransferOut_t type mismatch");

template <FwSizeType NumSubChannels>
class PhysicalChannel : public PhysicalChannelBase {
  public:
    using Base = PhysicalChannelBase;
    using Base::Base;
    using typename Base::Id_t;
    using typename Base::Queue_t;
    using typename Base::TransferIn_t;
    using typename Base::TransferOut_t;

    using MasterChannelList = ChannelList<SingleMasterChannel, NumSubChannels>;
    using Channel_t = SingleMasterChannel;

    VirtualChannel& getChannel(GVCID_t const gvcid);
    PhysicalChannel(Id_t const& id, MasterChannelList& subChannels);
    ~PhysicalChannel();

    void popFrameBuff(Fw::SerializeBufferBase& frameBuff);

    // NOTE should use getter for this
    MasterChannelList m_subChannels;

  protected:
    virtual bool receive(TransferIn_t& arg, TransferOut_t& result) override;
    virtual bool generate(TransferOut_t& frame) override;
};

using SinglePhysicalChannel = PhysicalChannel<1>;

}  // namespace TMSpaceDataLink

#endif  // SVC_TM_SPACE_DATA_LINK_CHANNELS_HPP

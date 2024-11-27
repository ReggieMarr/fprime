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
#include "Os/Models/QueueBlockingTypeEnumAc.hpp"
#include "Os/Queue.hpp"
#include "Svc/FrameAccumulator/FrameDetector/StartLengthCrcDetector.hpp"
#include "Svc/FramingProtocol/CCSDSProtocols/CCSDSProtocolDefs.hpp"
#include "Svc/FramingProtocol/CCSDSProtocols/TMSpaceDataLink/ManagedParameters.hpp"
#include "Svc/FramingProtocol/CCSDSProtocols/TMSpaceDataLink/Services.hpp"
#include "Svc/FramingProtocol/CCSDSProtocols/TMSpaceDataLink/TransferFrameQueue.hpp"
#include "TransferFrame.hpp"

namespace TMSpaceDataLink {
constexpr FwSizeType CHANNEL_Q_DEPTH = 10;

template <typename ChannelType, FwSizeType ChannelSize>
using ChannelList = std::array<ChannelType, ChannelSize>;

// NOTE use trait struct here
template <typename ChannelParams_t,
          typename UserDataService,
          typename UserDataServiceRequest,
          typename FramingService,
          typename TransferInType,
          typename TransferOutType,
          typename IdType>
class BaseVirtualChannel : public UserDataService, public FramingService {
  public:
    using TransferIn_t = TransferInType;
    using TransferOut_t = TransferOutType;
    // require parameterless construction to make an array of virtual channels
    BaseVirtualChannel() : UserDataService({}), FrameService({}), id({}), m_externalQueue() {}

    BaseVirtualChannel(ChannelParams_t const& params, GVCID_t id);
    BaseVirtualChannel& operator=(const BaseVirtualChannel& other);

    // NOTE should be const
    IdType id;

    virtual bool transfer(TransferInType& transferBuffer) = 0;

    Os::Generic::TransformFrameQueue<255> m_externalQueue;  // Queue for inter-task communication
  protected:
    virtual bool ChannelGeneration_handler(UserDataServiceRequest const &request, TransferOut_t& channelOut) = 0;
    U8 m_channelTransferCount = 0;
};

template <typename ChannelParams_t,
          typename SubChannelType,
          typename TransferInType,
          typename TransferOutType,
          typename IdType>
class BaseMasterChannel {
  public:
    using TransferIn_t = TransferInType;
    using TransferOut_t = TransferOutType;
    // require parameterless construction to make an array of virtual channels
    BaseMasterChannel() : id({}), m_externalQueue() {}

    BaseMasterChannel(ChannelParams_t& params);
    BaseMasterChannel(ChannelParams_t& params, IdType id);

    BaseMasterChannel(const BaseMasterChannel& other);

    BaseMasterChannel& operator=(const BaseMasterChannel& other);

    // NOTE should be const
    IdType id;

    virtual bool transfer() = 0;

    Os::Generic::TransformFrameQueue<255> m_externalQueue;  // Queue for inter-task communication
  protected:

    // NOTE should be const
    ChannelList<SubChannelType, MAX_MASTER_CHANNELS> m_subChannels{};
    ChannelParams_t m_params;
    FwQueuePriorityType priority = 0;
    U8 m_channelTransferCount = 0;
};

class VirtualChannel : public BaseVirtualChannel<VirtualChannelParams_t,
                                                 VCAService,
                                                 VCAService::RequestPrimitive_t,
                                                 FrameService,
                                                 Fw::ComBuffer,
                                                 TransferFrame<255>,
                                                 GVCID_t> {
  public:
    // Inherit parent's type definitions
    using Base = BaseVirtualChannel<VirtualChannelParams_t,
                                    VCAService,
                                    VCAService::RequestPrimitive_t,
                                    FrameService,
                                    Fw::ComBuffer,
                                    TransferFrame<255>,
                                    GVCID_t>;

    // Inherit constructors
    using Base::BaseVirtualChannel;

    // not sure if this is needed
    using TransferInType = typename Base::TransferIn_t;
    using TransferOutType = typename Base::TransferOut_t;

    using Base::operator=;
    // NOTE this should use the sizeof operator or something else similar
    // static constexpr FwSizeType TransferOutType::SIZE;

    // NOTE we should probably be able to pass a const buffer here
    bool transfer(typename Base::TransferIn_t& transferBuffer) override;

  protected:
    bool ChannelGeneration_handler(VCAService::RequestPrimitive_t const &request, TransferOut_t& channelOut) override;
};

// Master Channel Implementation
class MasterChannel : public BaseMasterChannel<MasterChannelParams_t,
                                               VirtualChannel,
                                               typename VirtualChannel::TransferOutType,
                                               typename VirtualChannel::TransferOutType,
                                               MCID_t> {
    MasterChannelParams_t dfltParams{};

  public:
    // Inherit parent's type definitions
    using Base = BaseMasterChannel<MasterChannelParams_t,
                                   VirtualChannel,
                                   typename VirtualChannel::TransferOutType,
                                   typename VirtualChannel::TransferOutType,
                                   MCID_t>;
    // Inherit constructors
    using Base::BaseMasterChannel;

    // not sure if this is needed
    using TransferInType = typename Base::TransferIn_t;
    using TransferOutType = typename Base::TransferOut_t;
    // it's actually a q but each item contains all virtual channels that we are connected to
    // NOTE replace MAX_VIRTUAL_CHANNELS with some init param
    // using TransferOutItemType = std::array<TransferInType, MAX_VIRTUAL_CHANNELS>;
    // how many transfers we'll hold internally before we have to dump
    // note this is basically just an internal queue
    static constexpr FwSizeType InternalTransferOutLength = MAX_VIRTUAL_CHANNELS;
    using InternalTransferOutType = std::array<TransferInType, InternalTransferOutLength>;

    using Base::operator=;

    bool getChannel(GVCID_t const gvcid, VirtualChannel& vc);

    bool transfer() override;

  private:
    bool VirtualChannelMultiplexing_handler(TransferInType& virtualChannelOut,
                                            NATIVE_UINT_TYPE vcid,
                                            InternalTransferOutType& masterChannelTransfer);

    bool MasterChannelGeneration_handler(InternalTransferOutType& masterChannelTransfer);
};

// Physical Channel Implementation
class PhysicalChannel : public BaseMasterChannel<PhysicalChannelParams_t,
                                                 MasterChannel,
                                                 typename MasterChannel::TransferOutType,
                                                 std::array<typename MasterChannel::TransferOutType, 10>,
                                                 Fw::String> {
  public:
    // Inherit parent's type definitions
    using Base = BaseMasterChannel<PhysicalChannelParams_t,
                                   MasterChannel,
                                   typename MasterChannel::TransferOutType,
                                   std::array<typename VirtualChannel::TransferOutType, 10>,
                                   Fw::String>;
    // Inherit constructors
    using Base::BaseMasterChannel;

    // not sure if this is needed
    using TransferInType = typename Base::TransferIn_t;
    using TransferOutType = typename Base::TransferOut_t;

    using Base::operator=;

    bool getChannel(MCID_t const mcid, VirtualChannel& vc);

    bool getChannel(GVCID_t const gvcid, VirtualChannel& vc);

    bool transfer();

  private:
    bool MasterChannelMultiplexing_handler(TransferInType& masterChannelOut,
                                           NATIVE_UINT_TYPE mcid,
                                           TransferOutType& physicalChannelOut);

    bool AllFramesChannelGeneration_handler(TransferOutType& masterChannelTransfer);
};

}  // namespace TMSpaceDataLink

#endif  // SVC_TM_SPACE_DATA_LINK_CHANNELS_HPP

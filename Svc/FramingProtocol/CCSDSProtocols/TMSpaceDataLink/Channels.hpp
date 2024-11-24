#ifndef SVC_TM_SPACE_DATA_LINK_CHANNELS_HPP
#define SVC_TM_SPACE_DATA_LINK_CHANNELS_HPP

#include <Svc/FramingProtocol/FramingProtocol.hpp>
#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <queue>
#include <vector>
#include "Fw/Com/ComBuffer.hpp"
#include "Fw/Types/SerialStatusEnumAc.hpp"
#include "Fw/Types/Serializable.hpp"
#include "ManagedParameters.hpp"
#include "Os/Queue.hpp"
#include "Svc/FrameAccumulator/FrameDetector/StartLengthCrcDetector.hpp"
#include "Svc/FramingProtocol/CCSDSProtocols/CCSDSProtocolDefs.hpp"
#include "Svc/FramingProtocol/CCSDSProtocols/TMSpaceDataLink/ManagedParameters.hpp"
#include "Svc/FramingProtocol/CCSDSProtocols/TMSpaceDataLink/Services.hpp"
#include "TransferFrame.hpp"
#include "config/FpConfig.h"

namespace TMSpaceDataLink {

// template <class Service>
// class BaseChannel : public Service {
// protected:
//     FwSizeType m_transferFrameSize;
//     Os::Queue m_queue;  // Queue for inter-task communication
// public:
//     BaseChannel(FwSizeType transferFrameSize) : m_transferFrameSize(transferFrameSize) {}
//     BaseChannel(BaseChannel const &other) :
//         m_transferFrameSize(other.m_transferFrameSize) {}

//     BaseChannel& operator=(const BaseChannel& other) {
//         // Can't do this
//         // TODO fix queue swapping
//         // this->m_queue = other.m_queue;
//         if (this != &other) {
//             this->m_transferFrameSize = other.m_transferFrameSize;
//         }
//         return *this;
//     }

//     virtual ~BaseChannel() {}

//     virtual bool propogate(const Fw::Buffer& sdu) = 0;  // Abstract process method
// };

// Primary template for transfer implementation
template <typename UserDataProcessor,
          typename OCFService,
          typename FSHService,
          typename FrameService,
          bool HasOCF = is_service_active<OCFService>::value,
          bool HasFSH = is_service_active<FSHService>::value,
          bool HasFrame = is_service_active<FrameService>::value>
struct TransferImpl;

// Virtual Channel Implementation
// The Segmenting and blocking functionality described in 2.3.1(b)
// is implemented by the PacketProccessing_handler (if the VCP service is registered)
// And the VirtualChannelGeneration_handler.
template <typename UserDataProcessor,
          typename OCFFieldGenerator = OCF_None,
          typename FSHFieldGenerator = FSH_None,
          typename FrameGenerator = FrameNone>
class VirtualChannelSender
    : public ServiceProcedure<UserDataProcessor, OCFFieldGenerator, FSHFieldGenerator, FrameGenerator> {
    using BaseType = ServiceProcedure<UserDataProcessor, OCFFieldGenerator, FSHFieldGenerator, FrameGenerator>;

  public:
    VirtualChannelSender(VirtualChannelParams_t const& params, FwSizeType const transferFrameSize, GVCID_t id)
        : BaseType(VCA_ServiceParameters_t{id}, 1), id(id), m_transferFrameSize(transferFrameSize) {}

    const GVCID_t id;

    bool transfer(Fw::ComBuffer const& transferBuffer) {
        return TransferImpl<UserDataProcessor, OCFFieldGenerator, FSHFieldGenerator, FrameGenerator>::transfer(
            this, transferBuffer);
    }

  protected:
    FwSizeType m_transferFrameSize;

  private:
    // In general the ChannelGeneration creates the output that the channel transfers
    // if VCF service is available then this means it represents the channel as TransferFrame
    // if not then it will be a composite object that provides those fields that the virtual channel
    // can set in the TransferFrame at this time.
    // NOTE we could use composites to create these structures
    bool ChannelGeneration_handler(VCA_Request_t const& request, TransferFrame& channelOut);
    // place holders for now
    typedef NATIVE_UINT_TYPE VCA_OCF_FSH_ChannelOut_t;
    typedef NATIVE_INT_TYPE VCA_FSH_ChannelOut_t;
    typedef U8 VCA_OCF_ChannelOut_t;
    virtual bool ChannelGeneration_handler(VCA_Request_t const& request, VCA_OCF_FSH_ChannelOut_t& channelOut) = 0;
    virtual bool ChannelGeneration_handler(VCA_Request_t const& request, VCA_FSH_ChannelOut_t& channelOut) = 0;
    virtual bool ChannelGeneration_handler(VCA_Request_t const& request, VCA_OCF_ChannelOut_t& channelOut) = 0;

    typedef NATIVE_UINT_TYPE VCP_OCF_FSH_ChannelOut_t;
    typedef NATIVE_INT_TYPE VCP_FSH_ChannelOut_t;
    typedef U8 VCP_OCF_ChannelOut_t;
    virtual bool ChannelGeneration_handler(VCP_Request_t const& request, TransferFrame& channelOut) = 0;
    virtual bool ChannelGeneration_handler(VCP_Request_t const& request, VCP_OCF_FSH_ChannelOut_t& channelOut) = 0;
    virtual bool ChannelGeneration_handler(VCP_Request_t const& request, VCP_FSH_ChannelOut_t& channelOut) = 0;
    virtual bool ChannelGeneration_handler(VCP_Request_t const& request, VCP_OCF_ChannelOut_t& channelOut) = 0;
};

using VCAFramedChannel = VirtualChannelSender<VCAService, TransferFrame>;
template class VirtualChannelSender<VCAService, TransferFrame>;

// Helper to determine service type at compile time
template <typename T>
struct ServiceTraits {
    static constexpr bool is_vca = std::is_same<T, VCAService>::value;
    static constexpr bool is_frame = std::is_same<T, TransferFrame>::value;
};

// template <class VCService>
// class VirtualChannelSender : public BaseChannel<VCService> {
// public:
//     VirtualChannelSender(VirtualChannelParams_t const &params,
//                          FwSizeType const transferFrameSize, GVCID_t id) :
//                            BaseChannel<VCService>(transferFrameSize), id(id) {}

//     VirtualChannelSender(VirtualChannelSender const &other) :
//         BaseChannel<VCService>(other), id(other.id) {}

//     VirtualChannelSender& operator=(const VirtualChannelSender& other) {
//         if (this != &other) {
//             BaseChannel<VCService>::operator=(other);
//         }
//         return *this;
//     }

//     // NOTE should be const
//     const GVCID_t id;

//     // Handles incoming user data, first attempts to process packets (if supported)
//     // by calling PacketProcessing_handler then creates a frame representation of
//     // the virtual channel via VirtualChannelGeneration_handler
//     bool propogate(const Fw::Buffer& sdu) override;
// private:
//     bool PacketProcessing_handler(const Fw::Buffer& sdu);
//     bool VirtualChannelGeneration_handler(const Fw::Buffer& sdu);

// };

// // Master Channel Implementation
// template <class Service>
// class MasterChannelSender : public BaseChannel<Service> {
// public:
//     MasterChannelSender(MasterChannelParams_t const &params,
//                         FwSizeType const transferFrameLength, MCID_t id) :
//                     BaseChannel<Service>(transferFrameLength),
//                     m_subChannels(createSubChannels(params, transferFrameLength, id)),
//                     id(id) {}

//     MasterChannelSender(const MasterChannelSender& other) :
//         BaseChannel<Service>(other),
//         m_subChannels(other.m_subChannels), id(other.m_id) {}

//     MasterChannelSender& operator=(const MasterChannelSender& other) {
//         if (this != &other) {
//             BaseChannel<Service>::operator=(other);
//             // TODO set members
//             // m_registeredServices = other.m_registeredServices;
//             m_subChannels = other.m_subChannels;
//             id = other.id;
//         }
//         return *this;
//     }
//     const MCID_t id;

//     bool propogate(const Fw::Buffer& sdu) override;

// private:
//     bool VirtualChannelMultiplexing_handler(const std::array<Fw::Buffer, 8>& vcFrames);
//     bool MasterChannelGeneration_handler(Fw::Buffer& frame);

//     static std::array<VirtualChannelSender<Service>, MAX_VIRTUAL_CHANNELS>
//         createSubChannels(MasterChannelParams_t const &params,
//                           NATIVE_UINT_TYPE const transferFrameSize,
//                           MCID_t const mcid)
//     {
//         std::array<VirtualChannelSender<Service>, MAX_VIRTUAL_CHANNELS> channels;
//         for (NATIVE_UINT_TYPE i = 0; i < params.numSubChannels; i++) {
//             GVCID_t vcid = {mcid, i};
//             channels.at(i) = VirtualChannelSender<Service>(params.subChannels[i], transferFrameSize, vcid);
//         }
//         return channels;
//     }

//     std::array<VirtualChannelSender<Service>, MAX_VIRTUAL_CHANNELS> m_subChannels;
// };

// // Physical Channel Implementation
// template <class Service>
// class PhysicalChannelSender : public BaseChannel<Service> {
// public:
//     PhysicalChannelSender(PhysicalChannelParams_t const &params, Fw::String id) :
//         BaseChannel<Service>(params.transferFrameSize),
//         m_subChannels(createSubChannels(params)), id(id) {}

//     PhysicalChannelSender(const PhysicalChannelSender& other) :
//         BaseChannel<Service>(other),
//         m_subChannels(other.m_subChannels), id(other.id) {}

//     PhysicalChannelSender& operator=(const PhysicalChannelSender& other) {
//         if (this != &other) {
//             BaseChannel<Service>::operator=(other);
//             m_subChannels = other.m_subChannels;
//             id = other.id;
//         }
//         return *this;
//     }
//     const Fw::String id;

//     bool propogate(const Fw::Buffer& sdu) override;
// private:
//     bool MasterChannelMultiplexing_handler(const std::array<Fw::Buffer, 8>& mcFrames);
//     bool AllFramesGeneration_handler(Fw::Buffer& frame);

//     static std::array<MasterChannelSender<Service>, MAX_MASTER_CHANNELS> createSubChannels(
//         const PhysicalChannelParams_t& params)
//     {
//         std::array<MasterChannelSender<Service>, MAX_MASTER_CHANNELS> channels;
//         for (NATIVE_UINT_TYPE i = 0; i < params.numSubChannels; i++) {
//             MCID_t mcid = {params.subChannels.at(i).spaceCraftId, params.transferFrameVersion};
//             channels.at(i) = MasterChannelSender<Service>(params.subChannels.at(i), params.transferFrameSize, mcid);
//         }
//         return channels;
//     }

//     std::array<MasterChannelSender<Service>, MAX_MASTER_CHANNELS> m_subChannels;
// };

}  // namespace TMSpaceDataLink

#endif  // SVC_TM_SPACE_DATA_LINK_CHANNELS_HPP

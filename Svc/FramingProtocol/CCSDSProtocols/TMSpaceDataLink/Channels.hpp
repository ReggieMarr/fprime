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
#include "Fw/Types/String.hpp"
#include "ManagedParameters.hpp"
#include "Os/Queue.hpp"
#include "Svc/FrameAccumulator/FrameDetector/StartLengthCrcDetector.hpp"
#include "Svc/FramingProtocol/CCSDSProtocols/CCSDSProtocolDefs.hpp"
#include "Svc/FramingProtocol/CCSDSProtocols/TMSpaceDataLink/ManagedParameters.hpp"
#include "Svc/FramingProtocol/CCSDSProtocols/TMSpaceDataLink/Services.hpp"
#include "TransferFrame.hpp"
#include "config/FpConfig.h"

namespace TMSpaceDataLink {

// Virtual Channel Implementation
// The Segmenting and blocking functionality described in CCSDS 132.0-B-3 2.3.1(b)
// is implemented by the PacketProccessing_handler (if the VCP service is registered)
// And the VirtualChannelGeneration_handler.
template <typename UserDataProcessor,
          typename OCFFieldGenerator = OCF_None,
          typename FSHFieldGenerator = FSH_None,
          typename FrameGenerator = FrameNone>
class VirtualChannel
    : public ServiceProcedure<UserDataProcessor, OCFFieldGenerator, FSHFieldGenerator, FrameGenerator> {
    using BaseType = ServiceProcedure<UserDataProcessor, OCFFieldGenerator, FSHFieldGenerator, FrameGenerator>;

  public:
    VirtualChannel(VirtualChannelParams_t const& params, FwSizeType const transferFrameSize, GVCID_t id)
        : BaseType(VCA_ServiceParameters_t{id}, 1), id(id), m_transferFrameSize(transferFrameSize), m_q() {
        Os::Queue::Status status;
        Fw::String name = "Virtual Channel";
        FwSizeType qDepth = 1;
        // NOTE this is very wrong at the moment
        m_q.create(name, qDepth, sizeof(TransferFrame));
        FW_ASSERT(status == Os::Queue::Status::OP_OK, status);
    }

    const GVCID_t id;

    // NOTE we should probably be able to pass a const buffer here
    bool transfer(Fw::ComBuffer& transferBuffer);

    Os::Queue m_q;  // Queue for inter-task communication
  protected:
    FwSizeType m_transferFrameSize;
    // Os::Queue m_q;  // Queue for inter-task communication

    // In general the ChannelGeneration creates the output that the channel transfers
    // if VCF service is available then this means it represents the channel as TransferFrame
    // if not then it will be a composite object that provides those fields that the virtual channel
    // can set in the TransferFrame at this time.
    // template <typename RequestPrim, typename ChannelRepresentation>
    // bool ChannelGeneration_handler(RequestPrim const& request, ChannelRepresentation& channelOut);
    // NOTE we could use composites to create these structures
    bool ChannelGeneration_handler(VCA_Request_t const& request, TransferFrame& channelOut);
    // place holders for now
    // typedef NATIVE_UINT_TYPE VCA_OCF_FSH_ChannelOut_t;
    // typedef NATIVE_INT_TYPE VCA_FSH_ChannelOut_t;
    // typedef U8 VCA_OCF_ChannelOut_t;
    // virtual bool ChannelGeneration_handler(VCA_Request_t const& request, VCA_OCF_FSH_ChannelOut_t& channelOut) = 0;
    // virtual bool ChannelGeneration_handler(VCA_Request_t const& request, VCA_FSH_ChannelOut_t& channelOut) = 0;
    // virtual bool ChannelGeneration_handler(VCA_Request_t const& request, VCA_OCF_ChannelOut_t& channelOut) = 0;

    // typedef NATIVE_UINT_TYPE VCP_OCF_FSH_ChannelOut_t;
    // typedef NATIVE_INT_TYPE VCP_FSH_ChannelOut_t;
    // typedef U8 VCP_OCF_ChannelOut_t;
    // virtual bool ChannelGeneration_handler(VCP_Request_t const& request, TransferFrame& channelOut) = 0;
    // virtual bool ChannelGeneration_handler(VCP_Request_t const& request, VCP_OCF_FSH_ChannelOut_t& channelOut) = 0;
    // virtual bool ChannelGeneration_handler(VCP_Request_t const& request, VCP_FSH_ChannelOut_t& channelOut) = 0;
    // virtual bool ChannelGeneration_handler(VCP_Request_t const& request, VCP_OCF_ChannelOut_t& channelOut) = 0;
};

using VCAFramedChannel = VirtualChannel<VCAService, TransferFrame>;
// TODO we should be able to put this in a cpp file
template <>
class VirtualChannel<VCAService, TransferFrame> {
  public:
    bool transfer(Fw::ComBuffer& transferBuffer) {
        // Specific implementation for VCAService with TransferFrame
        VCA_SDU_t vcaSDU;
        VCA_Request_t vcaRequest;

        bool status = false;

        status = generatePrimitive(transferBuffer, vcaRequest);
        FW_ASSERT(status, status);

        TransferFrame frame;
        status = ChannelGeneration_handler(vcaRequest, frame);
        FW_ASSERT(status, status);

        FwQueuePriorityType priority = 0;
        Os::Queue::Status qStatus = Os::Queue::Status::OP_OK;
        // qStatus = m_q.send(static_cast<const U8 *>(transferBuffer.getBuffAddr()),
        //          static_cast<FwSizeType>(transferBuffer.getBuffLength()), priority, Os::QueueBlockingType::BLOCKING);
        return qStatus == Os::Queue::Status::OP_OK;
    }

  protected:
    bool ChannelGeneration_handler(VCA_Request_t const& request, TransferFrame& channelOut) {
        bool status = true;
        // Was something like this originally
        // FrameSDU_t framePrim;
        // status = sender->generateFramePrimitive(request, framePrim);
        DataField dataField(request.sdu);
        // TODO the ManagedParameters should propogate to this
        MissionPhaseParameters_t missionParams;
        TransferData_t transferData;
        // TODO do this
        // transferData.dataFieldDesc = request.statusFields
        // NOTE could just as easily get this from the id but this matches the spec better
        transferData.virtualChannelId = request.sap.VCID;
        // Explicitly do not set this
        // transferData.masterChannelFrameCount
        // TODO this should some how be propogated from the transfer call or the masterChannel parent
        static U8 vcFrameCount = 0;
        transferData.virtualChannelFrameCount = vcFrameCount++;
        PrimaryHeader primaryHeader(missionParams, transferData);
        TransferFrame frame(primaryHeader, dataField);
        // FIXME this I think is bad memory management
        channelOut = frame;

        FW_ASSERT(status, status);
        return true;
    };
};

// Helper to determine service type at compile time
template <typename T>
struct ServiceTraits {
    static constexpr bool is_vca = std::is_same<T, VCAService>::value;
    static constexpr bool is_frame = std::is_same<T, TransferFrame>::value;
};

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

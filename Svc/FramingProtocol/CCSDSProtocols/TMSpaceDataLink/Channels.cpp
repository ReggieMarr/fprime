#include "Channels.hpp"
#include "Os/Queue.hpp"
#include "Services.hpp"
#include "Svc/FramingProtocol/CCSDSProtocols/TMSpaceDataLink/TransferFrame.hpp"

namespace TMSpaceDataLink {

// Specialization for VCA + Frame only
// NOTE if we did template meta programming it might make sense to consider
// the setting of the VCA or VCP as the service to handle user data as the packet processing function
template <typename UserDataProcessor, typename OCFService, typename FSHService, typename FrameService>
struct TransferImpl<UserDataProcessor, OCFService, FSHService, FrameService, false, false, true> {
    static bool transfer(VirtualChannelSender<UserDataProcessor, OCFService, FSHService, FrameService>* sender,
                         Fw::ComBuffer const& buffer) {
        VCA_SDU_t vcaPrim;
        bool status = sender->generateVCAPrimitive(buffer, vcaPrim);
        FW_ASSERT(status, status);

        TransferFrame frame();
        status = sender->ChannelGeneration_handler(vcaPrim, frame);
        FW_ASSERT(status, status);

        return sender->m_q.send(frame) == Os::Queue::Status::OP_OK;

        return status;
    }
};

// NOTE could apply templating here to support VCA and VCP both in request and return types
template <>
bool VCAFramedChannel::ChannelGeneration_handler(VCA_Request_t const& request, TransferFrame& channelOut) {
    // Generate Transfer Frame for the Virtual Channel using the VCA prim as the data field
    // and the Frame service to create the transfer frame itself
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
}

// bool MasterChannelSender::VirtualChannelMultiplexing_handler(const std::array<Fw::Buffer, 8>& vcFrames) {
//     // Multiplex Virtual Channel frames into Master Channel
//     return true;
// }

// bool MasterChannelSender::MasterChannelGeneration_handler(Fw::Buffer& frame) {
//     // Add Master Channel-specific data
//     return true;
// }

// bool MasterChannelSender::propogate(const Fw::Buffer& sdu) {
//     // Process incoming frames for Master Channel
//     return true;
// }

// bool PhysicalChannelSender::MasterChannelMultiplexing_handler(const std::array<Fw::Buffer, 8>& mcFrames) {
//     // Multiplex Master Channel frames for physical channel
//     return true;
// }

// bool PhysicalChannelSender::AllFramesGeneration_handler(Fw::Buffer& frame) {
//     // Add Frame Error Control Field (FECF) and finalize frame
//     return true;
// }

// bool PhysicalChannelSender::propogate(const Fw::Buffer& sdu) {
//     // Final frame processing before transmission
//     bool channelStatus = true;
//     return channelStatus;
// }

}  // namespace TMSpaceDataLink

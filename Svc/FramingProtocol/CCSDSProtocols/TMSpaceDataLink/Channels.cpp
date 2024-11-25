#include "Channels.hpp"
#include "FpConfig.h"
#include "Os/Models/QueueBlockingTypeEnumAc.hpp"
#include "Os/Models/QueueStatusEnumAc.hpp"
#include "Os/Queue.hpp"
#include "Services.hpp"
#include "Svc/FramingProtocol/CCSDSProtocols/TMSpaceDataLink/TransferFrame.hpp"

namespace TMSpaceDataLink {

// Specialization for VCAService and TransferFrame
// NOTE VCAFramedChannel is equivalent to VirtualChannel<VCAService, OCF_None, FSH_None, TransferFrame>
template <>
bool VCAFramedChannel::transfer(Fw::ComBuffer& transferBuffer) {
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

// template class VirtualChannel<VCAService, TransferFrame>;
// NOTE could apply templating here to support VCA and VCP both in request and return types
// template <>
// bool VCAFramedChannel::ChannelGeneration_handler<VCA_Request_t, TransferFrame>(VCA_Request_t const& request,
// template <>
// bool VirtualChannel<VCAService, TransferFrame>::ChannelGeneration_handler(VCA_Request_t const& request,
//                                                                           TransferFrame& channelOut) {
//     // Generate Transfer Frame for the Virtual Channel using the VCA prim as the data field
//     // and the Frame service to create the transfer frame itself
//     bool status = true;
//     // Was something like this originally
//     // FrameSDU_t framePrim;
//     // status = sender->generateFramePrimitive(request, framePrim);
//     DataField dataField(request.sdu);
//     // TODO the ManagedParameters should propogate to this
//     MissionPhaseParameters_t missionParams;
//     TransferData_t transferData;
//     // TODO do this
//     // transferData.dataFieldDesc = request.statusFields
//     // NOTE could just as easily get this from the id but this matches the spec better
//     transferData.virtualChannelId = request.sap.VCID;
//     // Explicitly do not set this
//     // transferData.masterChannelFrameCount
//     // TODO this should some how be propogated from the transfer call or the masterChannel parent
//     static U8 vcFrameCount = 0;
//     transferData.virtualChannelFrameCount = vcFrameCount++;
//     PrimaryHeader primaryHeader(missionParams, transferData);
//     TransferFrame frame(primaryHeader, dataField);
//     // FIXME this I think is bad memory management
//     channelOut = frame;

//     FW_ASSERT(status, status);
//     return true;
// }

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

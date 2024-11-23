// VirtualChannelAccess.hpp
#ifndef SVC_VIRTUAL_CHANNEL_ACCESS_HPP
#define SVC_VIRTUAL_CHANNEL_ACCESS_HPP

#include <array>
#include "FpConfig.h"
#include "Fw/Buffer/Buffer.hpp"
#include "Fw/Com/ComBuffer.hpp"
#include "Fw/Com/ComPacket.hpp"
#include "Fw/Types/BasicTypes.hpp"
#include "Fw/Types/Serializable.hpp"
#include "Fw/Types/String.hpp"
#include "Os/Mutex.hpp"
#include "Os/Queue.hpp"
#include "Svc/FramingProtocol/CCSDSProtocols/TMSpaceDataLink/ManagedParameters.hpp"
#include "Svc/FramingProtocol/CCSDSProtocols/TMSpaceDataLink/TransferFrame.hpp"

namespace TMSpaceDataLink {

// Source Data types/Service Data Units (SDU) for services as per CCSDS 132.0-B-3 3.2.1

// Packets 3.2.2 (Not currently supported)
// using PACKET_SDU

// Virtual Access Service Data Unit 3.2.3
using VCA_SDU_t = Fw::Buffer;
// template<FwSizeType DATA_LENGTH>
// using VCA_SDU_t=std::array<U8, DATA_LENGTH>;

// Frame Secondary Header Service Data Unit  3.2.4
using FSH_SDU_t = Fw::Buffer;

// Operational Control Field Service Data Unit  3.2.5
using OCF_SDU_t = Fw::Buffer;

// Operational Control Field Service Data Unit  3.2.6
using TM_SDU_t = TransferFrame;

// CCSDS 132.0-B-3 3.4.2.3
// The Packet Order Flag (1 bit) and Segment Length ID (2 bits) may be used to convey
// information on the validity, sequence, or other status of the VCA_SDUs. Provision of this
// field is mandatory; semantics are user-optional.
typedef struct VCAStatusFields_s {
    bool isPacketOrdered : 1;
    U8 segmentLengthId : 3;
    U16 firstHeaderPointer : 11;
} __attribute__((packed)) VCAStatusFields_t;

// CCSDS 132.0-B-3 3.4.3.2
typedef struct VCARequest_s {
    VCA_SDU_t sdu;
    VCAStatusFields_t statusFields;
    GVCID_t sap;
} VCARequest_t;

typedef struct VCAServiceParameters_s {
    GVCID_t sap;
    // TODO should probs use something that doesn't require ser and deser on each request/receive
    // or leverage this better by turning things into components
    Os::Queue primQ;
} VCAServiceParameters_t;

typedef enum {
    SYNCHRONOUS = 0x1,
    ASYNCHRONOUS = 0x2,
    PERIODIC = 0x4,
} SERVICE_TRANSFER_TYPE_t;

typedef enum {
    REQUEST,
    INIDICATION,
} SERVICE_TRANSFER_DIRECTION_t;

template <typename SDU_t,
          typename SAP_t,
          typename ServiceParams_t,
          typename ServiceTransferPrimitive_t,
          SERVICE_TRANSFER_TYPE_t SERVICE_TRANSFER_TYPE>
class TMService {
  public:
    TMService(ServiceParams_t const& serviceParams, FwSizeType const qDepth);
    bool transfer(Fw::ComBuffer const& transferBuffer);
    const Fw::String serviceName = "VCA SERVICE";
    const SERVICE_TRANSFER_TYPE_t serviceTransferType = SERVICE_TRANSFER_TYPE;
    const SAP_t sap = m_serviceParams.sap;

  private:
    ServiceParams_t m_serviceParams;
};

/**
 * Virtual Channel Access Service (CCSDS 132.0-B-3 3.4)
 * Provides fixed-length data unit transfer across virtual channels
 */
class VCAService : public TMService<VCA_SDU_t, GVCID_t, VCAServiceParameters_t, VCARequest_t, SYNCHRONOUS> {
  public:
    VCAService(VCAServiceParameters_t const& serviceParams, FwSizeType const qDepth)
        : TMService<VCA_SDU_t, GVCID_t, VCAServiceParameters_t, VCARequest_t, SYNCHRONOUS>(serviceParams, qDepth),
          m_mutex() {}

  private:
    Os::Mutex m_mutex;  ///< Thread safety for queue access
};

}  // namespace TMSpaceDataLink

#endif  // SVC_VIRTUAL_CHANNEL_ACCESS_HPP

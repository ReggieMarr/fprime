// VirtualChannelAccess.hpp
#ifndef SVC_VIRTUAL_CHANNEL_ACCESS_HPP
#define SVC_VIRTUAL_CHANNEL_ACCESS_HPP

#include <array>
#include <type_traits>
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
using VCP_SDU_t = Fw::ComPacket;

// Virtual Access Service Data Unit 3.2.3
using VCA_SDU_t = Fw::Buffer;
// template<FwSizeType DATA_LENGTH>
// using VCA_SDU_t=std::array<U8, DATA_LENGTH>;

// Frame Secondary Header Service Data Unit  3.2.4
using FSH_SDU_t = Fw::Buffer;

// Operational Control Field Service Data Unit  3.2.5
using OCF_SDU_t = Fw::Buffer;

// Operational Control Field Service Data Unit  3.2.6
using FrameSDU_t = TransferFrame;

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
} VCA_Request_t;

typedef struct VCAServiceParameters_s {
    GVCID_t sap;
} VCA_ServiceParameters_t;

typedef NATIVE_UINT_TYPE VCP_Request_t;
typedef NATIVE_UINT_TYPE OCF_Request_t;
typedef NATIVE_UINT_TYPE FSH_Request_t;
typedef NATIVE_UINT_TYPE FrameRequest_t;
typedef NATIVE_UINT_TYPE VCP_ServiceParameters_t;
typedef NATIVE_UINT_TYPE OCF_ServiceParameters_t;
typedef NATIVE_UINT_TYPE FSH_ServiceParameters_t;
typedef NATIVE_UINT_TYPE FrameServiceParameters_t;

// None type for optional services
struct OCF_None {
    template <typename... Args>
    OCF_None(Args&&...) {}  // Constructor that accepts anything but does nothing
};

struct FSH_None {
    template <typename... Args>
    FSH_None(Args&&...) {}  // Constructor that accepts anything but does nothing
};

struct FrameNone {
    template <typename... Args>
    FrameNone(Args&&...) {}  // Constructor that accepts anything but does nothing
};

// Type traits to check if service is None
template <typename T>
struct is_service_active : std::true_type {};

template <>
struct is_service_active<OCF_None> : std::false_type {};
template <>
struct is_service_active<FSH_None> : std::false_type {};
template <>
struct is_service_active<FrameNone> : std::false_type {};

template <typename UserDataService,
          typename OCFService = OCF_None,
          typename FSHService = FSH_None,
          typename FrameService = FrameNone>
class ServiceProcedure
    : public UserDataService,
      public std::conditional<is_service_active<OCFService>::value, OCFService, OCF_None>::type,
      public std::conditional<is_service_active<FSHService>::value, FSHService, FSH_None>::type,
      public std::conditional<is_service_active<FrameService>::value, FrameService, FrameNone>::type {
  public:
    template <typename... Args>
    ServiceProcedure(Args&&... args)
        : UserDataService(std::forward<Args>(args)...)
          // Optional services initialized only if active
          ,
          std::conditional<is_service_active<OCFService>::value, OCFService, OCF_None>::type(args...),
          std::conditional<is_service_active<FSHService>::value, FSHService, FSH_None>::type(args...),
          std::conditional<is_service_active<FrameService>::value, FrameService, FrameNone>::type(args...) {}

    // Helper methods to check if services are available
    static constexpr bool hasOCF() { return is_service_active<OCFService>::value; }
    static constexpr bool hasFSH() { return is_service_active<FSHService>::value; }
    static constexpr bool hasFrame() { return is_service_active<FrameService>::value; }
};

class VCAService {
  public:
    VCAService(VCA_ServiceParameters_t const& serviceParams, FwSizeType const qDepth){};
};

// NOTE this is really just the TransferFrame class
class FrameService {
  public:
    FrameService(FrameServiceParameters_t const& serviceParams, FwSizeType const qDepth){};
};

// typedef enum {
//     SYNCHRONOUS = 0x1,
//     ASYNCHRONOUS = 0x2,
//     PERIODIC = 0x4,
// } SERVICE_TRANSFER_TYPE_t;

// typedef enum {
//     REQUEST,
//     INIDICATION,
// } SERVICE_TRANSFER_DIRECTION_t;

// template <typename SDU_t,
//           typename SAP_t,
//           typename ServiceParams_t,
//           typename ServiceTransferPrimitive_t,
//           SERVICE_TRANSFER_TYPE_t SERVICE_TRANSFER_TYPE>
// class TMService {
//   public:
//     TMService(ServiceParams_t const& serviceParams, FwSizeType const qDepth);
//     bool generatePrimitive();
//     const Fw::String serviceName = "VCA SERVICE";
//     const SERVICE_TRANSFER_TYPE_t serviceTransferType = SERVICE_TRANSFER_TYPE;
//     const SAP_t sap = m_serviceParams.sap;

//   private:
//     Os::Queue m_q;  // Queue for inter-task communication
//     ServiceParams_t m_serviceParams;
// };

// /**
//  * Virtual Channel Access Service (CCSDS 132.0-B-3 3.4)
//  * Provides fixed-length data unit transfer across virtual channels
//  */
// class VCAService : public TMService<VCA_SDU_t, GVCID_t, VCA_ServiceParameters_t, VCA_Request_t, SYNCHRONOUS> {
//   public:
//     VCAService(VCA_ServiceParameters_t const& serviceParams, FwSizeType const qDepth)
//         : TMService<VCA_SDU_t, GVCID_t, VCA_ServiceParameters_t, VCA_Request_t, SYNCHRONOUS>(serviceParams, qDepth)
//         {}

//   private:
// };

// class VCPService : public TMService<VCP_SDU_t, GVCID_t, VCP_ServiceParameters_t, VCP_Request_t, SYNCHRONOUS> {
//   public:
//     VCPService(VCA_ServiceParameters_t const& serviceParams, FwSizeType const qDepth)
//         : TMService<VCP_SDU_t, GVCID_t, VCP_ServiceParameters_t, VCP_Request_t, SYNCHRONOUS>(serviceParams, qDepth)
//         {}

//   private:
//     bool generatePrimitive(); // generates VCP.request
//     bool PacketProcessing_handler(const Fw::Buffer& sdu) { return false; };
// };

// class FSHService : public TMService<FSH_SDU_t, GVCID_t, FSH_ServiceParameters_t, VCA_Request_t, SYNCHRONOUS> {
//   public:
//     FSHService(FSH_ServiceParameters_t const& serviceParams, FwSizeType const qDepth)
//         : TMService<FSH_SDU_t, GVCID_t, FSH_ServiceParameters_t, FSH_Request_t, SYNCHRONOUS>(serviceParams, qDepth)
//         {}
//   private:
//     bool generatePrimitive(); // generates FSH.request
// };

// class OCFService : public TMService<OCF_SDU_t, GVCID_t, OCF_ServiceParameters_t, VCA_Request_t, SYNCHRONOUS> {
//   public:
//     OCFService(VCA_ServiceParameters_t const& serviceParams, FwSizeType const qDepth)
//         : TMService<OCF_SDU_t, GVCID_t, OCF_ServiceParameters_t, OCF_Request_t, SYNCHRONOUS>(serviceParams, qDepth)
//         {}

//   private:
//     bool generatePrimitive(); // generates OCF.request
// };

// class FrameService : public TMService<FrameSDU_t, GVCID_t, FrameServiceParameters_t, FrameRequest_t, SYNCHRONOUS> {
//   public:
//     FrameService(FrameServiceParameters_t const& serviceParams, FwSizeType const qDepth)
//         : TMService<FrameSDU_t, GVCID_t, FrameServiceParameters_t, FrameRequest_t, SYNCHRONOUS>(serviceParams,
//         qDepth) {
//     }

//   private:
//     bool generatePrimitive(); // generates Frame.request
// };

}  // namespace TMSpaceDataLink

#endif  // SVC_VIRTUAL_CHANNEL_ACCESS_HPP

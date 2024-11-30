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
#include "Svc/FramingProtocol/CCSDSProtocols/TMSpaceDataLink/TransferFrameDefs.hpp"

namespace TMSpaceDataLink {

// Source Data types/Service Data Units (SDU) for services as per CCSDS 132.0-B-3 3.2.1

// Packets 3.2.2 (Not currently supported)
// using PACKET_SDU
using VCP_SDU_t = Fw::ComPacket;

// Frame Secondary Header Service Data Unit  3.2.4
using FSH_SDU_t = Fw::Buffer;

// Operational Control Field Service Data Unit  3.2.5
using OCF_SDU_t = Fw::Buffer;

// Operational Control Field Service Data Unit  3.2.6
using FrameSDU_t = FPrimeTransferFrame;

// CCSDS 132.0-B-3 3.4.2.3
// The Packet Order Flag (1 bit) and Segment Length ID (2 bits) may be used to convey
// information on the validity, sequence, or other status of the VCA_SDUs. Provision of this
// field is mandatory; semantics are user-optional.
typedef struct VCAStatusFields_s {
    bool isPacketOrdered : 1;
    U8 segmentLengthId : 3;
    U16 firstHeaderPointer : 11;
} __attribute__((packed)) VCAStatusFields_t;

typedef NATIVE_UINT_TYPE VCP_Request_t;
typedef NATIVE_UINT_TYPE OCF_Request_t;
typedef NATIVE_UINT_TYPE FSH_Request_t;
typedef NATIVE_UINT_TYPE VCP_ServiceParameters_t;
typedef NATIVE_UINT_TYPE OCF_ServiceParameters_t;
typedef NATIVE_UINT_TYPE FSH_ServiceParameters_t;

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

// This exists so that we can declare channels like this:
// using VCAFramedChannel = VirtualChannel<VCAService, TransferFrame>;
// template class VirtualChannel<VCAService, TransferFrame>;
// however it introduces some complexity and possibly the diamon problem.
// A better strategy might just be nesting inheritance on service classes
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

typedef enum {
    SYNCHRONOUS = 0x1,
    ASYNCHRONOUS = 0x2,
    PERIODIC = 0x4,
} SERVICE_TRANSFER_TYPE_e;

typedef enum {
    REQUEST,
    INIDICATION,
} SERVICE_TRANSFER_DIRECTION_t;

template <typename SDUType,
          typename IdType,
          typename ServiceUserDataType,
          typename ServiceTransferPrimitiveType,
          SERVICE_TRANSFER_TYPE_e ServiceTransferType>
class TMServiceBase {
  public:
    using UserData_t = ServiceUserDataType;
    using Primitive_t = ServiceTransferPrimitiveType;
    using SAP_t = IdType;
    TMServiceBase(SAP_t sap) : sap(sap){};

    const Fw::String serviceName = "DEFAULT SERVICE";
    const SERVICE_TRANSFER_TYPE_e serviceTransferType = ServiceTransferType;
    const SAP_t sap;

    virtual bool generatePrimitive(UserData_t& data, Primitive_t& prim) const = 0;
};

// CCSDS 132.0-B-3 3.4.3.2

// Virtual Access Service Data Unit 3.2.3
using VCA_SDU_t = Fw::Buffer;
// template<FwSizeType DATA_LENGTH>
// using VCA_SDU_t=FPrimeTransferFrame::DataField_t;

typedef struct VCARequestPrimitive_s {
    FPrimeTransferFrame::DataField_t sdu;
    VCAStatusFields_t statusFields;
    GVCID_t sap;
} VCARequestPrimitive_t;

/**
 * Virtual Channel Access Service (CCSDS 132.0-B-3 3.4)
 * Provides fixed-length data unit transfer across virtual channels
 */

class VCAService : public TMServiceBase<VCA_SDU_t,
                                        GVCID_t,
                                        Fw::ComBuffer,
                                        VCARequestPrimitive_t,
                                        SERVICE_TRANSFER_TYPE_e::SYNCHRONOUS> {
  public:
    using Base =
        TMServiceBase<VCA_SDU_t, GVCID_t, Fw::ComBuffer, VCARequestPrimitive_t, SERVICE_TRANSFER_TYPE_e::SYNCHRONOUS>;
    using Base::TMServiceBase;  // Inheriting constructor
    using typename Base::Primitive_t;
    using typename Base::SAP_t;
    using typename Base::UserData_t;

    virtual bool generatePrimitive(Fw::ComBuffer& data, VCARequestPrimitive_t& prim) const override {
        // NOTE should handle this in some way that respect const
        // prim.sdu.serializeValue(data);
        // prim.sap = sap;
        // // TODO actually handle this
        // prim.statusFields = {};
        return true;
    }
};

typedef struct VCFUserData_s {
    FPrimeTransferFrame::DataField_t dataField;
    VCAStatusFields_t statusFields;
} VCFUserData_t;

typedef struct VCFRequestPrimitive_s {
    FPrimeTransferFrame frame;
    GVCID_t gvcid;
} VCFRequestPrimitive_t;

class VCFService : public TMServiceBase<FPrimeTransferFrame,
                                        GVCID_t,
                                        VCFUserData_t,
                                        VCFRequestPrimitive_t,
                                        SERVICE_TRANSFER_TYPE_e::SYNCHRONOUS> {
  public:
    using Base = TMServiceBase<FPrimeTransferFrame,
                               GVCID_t,
                               VCFUserData_t,
                               VCFRequestPrimitive_t,
                               SERVICE_TRANSFER_TYPE_e::SYNCHRONOUS>;
    using Base::TMServiceBase;
    using typename Base::Primitive_t;
    using typename Base::SAP_t;
    using typename Base::UserData_t;

    virtual bool generatePrimitive(VCFUserData_t& data, VCFRequestPrimitive_t& prim) const override {
        FPrimeTransferFrame frame;
        PrimaryHeader header = frame.getPrimaryHeader();
        PrimaryHeaderControlInfo_t controlInfo = {
            // Set whatever we can at this time provided the context
            // aka data.statusFields stuff
        };
        frame.setDataField(data.dataField);
    }
};

// class VCPService : public TMService<VCP_SDU_t, GVCID, VCP_ServiceParameters_t, VCP_Request_t, SYNCHRONOUS> {
//   public:
//     VCPService(VCP_ServiceParameters_t const& serviceParams, FwSizeType const qDepth)
//         : TMService<VCP_SDU_t, GVCID, VCP_ServiceParameters_t, VCP_Request_t, SYNCHRONOUS>(serviceParams, qDepth)
//         {}

//   private:
//     bool generatePrimitive();  // generates VCP.request
//     bool PacketProcessing_handler(const Fw::Buffer& sdu) { return false; };
// };

// // All this really is is something that inserts the Operational Control Flag into a transfer frame
// class OCFService : public TMService<OCF_SDU_t, GVCID, OCF_ServiceParameters_t, OCF_Request_t, SYNCHRONOUS> {
//   public:
//     OCFService(OCF_ServiceParameters_t const& serviceParams, FwSizeType const qDepth)
//         : TMService<OCF_SDU_t, GVCID, OCF_ServiceParameters_t, OCF_Request_t, SYNCHRONOUS>(serviceParams, qDepth)
//         {}

//   private:
//     bool generatePrimitive();  // generates OCF.request
// };

}  // namespace TMSpaceDataLink

#endif  // SVC_VIRTUAL_CHANNEL_ACCESS_HPP

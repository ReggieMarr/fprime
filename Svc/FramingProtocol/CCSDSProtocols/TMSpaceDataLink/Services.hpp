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
#include "TransferFrameQueue.hpp"

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

typedef enum {
    SYNCHRONOUS = 0x1,
    ASYNCHRONOUS = 0x2,
    PERIODIC = 0x4,
} SERVICE_TRANSFER_TYPE_e;

typedef enum {
    REQUEST,
    INIDICATION,
} SERVICE_TRANSFER_DIRECTION_t;

template <typename SDUType, typename IdType, typename ServiceUserDataType, typename ServiceTransferPrimitiveType>
struct TMServiceBaseTemplateParameters {
    using SDU_t = SDUType;
    using SAP_t = IdType;
    using UserData_t = ServiceUserDataType;
    using Primitive_t = ServiceTransferPrimitiveType;
};

// NOTE since these are essentially stateless
// we might consider making them structs instead of classes
template <typename TemplateCfg>
class TMServiceBase {
  public:
    using SDU_t = typename TemplateCfg::SDU_t;
    using SAP_t = typename TemplateCfg::SAP_t;
    using UserData_t = typename TemplateCfg::UserData_t;
    using Primitive_t = typename TemplateCfg::Primitive_t;

    // const SERVICE_TRANSFER_TYPE_e serviceTransferType = TemplateCfg::ServiceTransferType;
    const SAP_t sap;

    TMServiceBase(SAP_t const& sap);

    virtual bool generatePrimitive(UserData_t & data, Primitive_t& prim) const = 0;

    // Fw::String getName() { return m_serviceName; };

  protected:
    Fw::String m_serviceName;
};

// CCSDS 132.0-B-3 3.4.3.2

// Virtual Access Service Data Unit 3.2.3
typedef FPrimeTransferFrame::DataField_t VCA_SDU_t;
// template<FwSizeType DATA_LENGTH>
// using VCA_SDU_t=FPrimeTransferFrame::DataField_t;
typedef struct VCARequestPrimitive_s {
    VCA_SDU_t sdu;
    VCAStatusFields_t statusFields;
    GVCID_t sap;
} VCARequestPrimitive_t;

// typedef VCAServiceCfg_s{
//     VCAServicePrams::SAP_t,
// } VCAServiceCfg_t;

using VCAServiceTemplateParams =
    TMServiceBaseTemplateParameters<VCA_SDU_t, GVCID_t, Fw::Buffer, VCARequestPrimitive_t>;

/**
 * Virtual Channel Access Service (CCSDS 132.0-B-3 3.4)
 * Provides fixed-length data unit transfer across virtual channels
 */
class VCAService : public TMServiceBase<VCAServiceTemplateParams> {
  public:
    using Base = TMServiceBase<VCAServiceTemplateParams>;
    using Base::TMServiceBase;  // Inheriting constructor
    using typename Base::Primitive_t;
    using typename Base::SAP_t;
    using typename Base::UserData_t;

    virtual bool generatePrimitive(Fw::Buffer & data, VCARequestPrimitive_t& prim) const override;
};

typedef VCARequestPrimitive_t VCFUserData_t;

// NOTE this could likely be re-used for the MCFService
// typedef Os::Generic::TransformFrameQueue VCFRequestPrimitive_t;
typedef struct VCFRequestPrimitive_s {
    FPrimeTransferFrame frame;
    GVCID_t sap;
} VCFRequestPrimitive_t;

using VCFServiceTemplateParams =
    TMServiceBaseTemplateParameters<FPrimeTransferFrame, GVCID_t, VCFUserData_t, VCFRequestPrimitive_t>;

class VCFService : public TMServiceBase<VCFServiceTemplateParams> {
  public:
    using Base = TMServiceBase<VCFServiceTemplateParams>;
    using Base::TMServiceBase;
    using typename Base::Primitive_t;
    using typename Base::SAP_t;
    using typename Base::UserData_t;

    virtual bool generatePrimitive(VCFUserData_t & request,
                                   VCFRequestPrimitive_t& prim) const;
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

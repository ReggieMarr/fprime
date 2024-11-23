// VirtualChannelAccess.hpp
#ifndef SVC_VIRTUAL_CHANNEL_ACCESS_HPP
#define SVC_VIRTUAL_CHANNEL_ACCESS_HPP

#include <array>
#include "FpConfig.h"
#include "Fw/Buffer/Buffer.hpp"
#include "Fw/Types/BasicTypes.hpp"
#include "Fw/Types/Serializable.hpp"
#include "Os/Mutex.hpp"
#include "Os/Queue.hpp"
#include "Svc/FramingProtocol/CCSDSProtocols/TMSpaceDataLink/Channels.hpp"
#include "Svc/FramingProtocol/CCSDSProtocols/TMSpaceDataLink/TransferFrame.hpp"

namespace TMSpaceDataLink {

enum class ServiceType { VCP, VCA };

// Source Data types/Service Data Units (SDU) for services as per CCSDS 132.0-B-3 3.2.1

// Packets 3.2.2 (Not currently supported)
// using PACKET_SDU

// Virtual Access Service Data Unit 3.2.3
using VCA_SDU_t=Fw::Buffer;
// template<FwSizeType DATA_LENGTH>
// using VCA_SDU_t=std::array<U8, DATA_LENGTH>;

// Frame Secondary Header Service Data Unit  3.2.4
using FSH_SDU_t=Fw::Buffer;

// Operational Control Field Service Data Unit  3.2.5
using OCF_SDU_t=Fw::Buffer;

// Operational Control Field Service Data Unit  3.2.6
using TM_SDU_t=TransferFrame;

//NOTE we could abstract this more with templates
// once adding support for more protocols
typedef struct SendVCAServiceParams_s {
  VCA_SDU_t sdu;
} SendVCAServiceParams_t;

template<typename SDU_t, typename SAP_t, typename ServiceParams_t>
class SendTMService {
public:
    SendTMService(ServiceParams_t const &serviceParams) :
      m_serviceParams(serviceParams) {};

    virtual bool request(SAP_t const sap, SDU_t const &sdu) = 0;
private:
    ServiceParams_t m_serviceParams;
};

/**
 * Virtual Channel Access Service (CCSDS 132.0-B-3 3.4)
 * Provides fixed-length data unit transfer across virtual channels
 */
class SendVCAService: public SendTMService<GVCID_t, VCA_SDU_t> {
public:
    SendVCAService() = default;

    /**
     * VCA.request primitive (CCSDS 132.0-B-3 3.4.3.2)
     * Request transfer of SDU on specified virtual channel
     *
     * @param vcId Virtual Channel ID
     * @param sdu Service Data Unit to transfer
     * @return Status (true if accepted)
     */
    bool request(GVCID_t const sap, VCA_SDU_t const &sdu) override;

private:
    std::map<U8, VirtualChannel_t> m_channels;  ///< Virtual channels by VCID
    const U32 m_maxQueueDepth;                ///< Maximum queued SDUs per channel
    Os::Mutex m_mutex;                        ///< Thread safety for queue access
};

} // namespace Svc

#endif // SVC_VIRTUAL_CHANNEL_ACCESS_HPP

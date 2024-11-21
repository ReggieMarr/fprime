// VirtualChannelAccess.hpp
#ifndef SVC_VIRTUAL_CHANNEL_ACCESS_HPP
#define SVC_VIRTUAL_CHANNEL_ACCESS_HPP

#include "Fw/Types/BasicTypes.hpp"
#include "Fw/Types/Serializable.hpp"
#include "Os/Mutex.hpp"
#include "Os/Queue.hpp"

namespace TMSpaceDataLink {

/**
 * Virtual Channel Access Service (CCSDS 132.0-B-3 3.4)
 * Provides fixed-length data unit transfer across virtual channels
 */
class VirtualChannelAccess {
public:
    static constexpr U8 MAX_VIRTUAL_CHANNELS = 8;  // 3-bit VCID allows 8 channels
    static constexpr U32 MAX_SDU_SIZE = 1024;  // Default SDU size in bytes

    /**
     * VCA Service Data Unit Status (CCSDS 132.0-B-3 3.4.2.3)
     * Optional status fields for VCA_SDU
     */
    struct VcaStatus {
        bool packetOrderFlag{false};     ///< User-defined ordering status
        U8 segmentLengthId{0};          ///< User-defined segment status (2 bits)
        bool sduLossFlag{false};         ///< Indicates sequence discontinuity
        U8 verificationStatus{0};        ///< SDLS protocol status
    };

    /**
     * VCA Service Data Unit (CCSDS 132.0-B-3 3.4.2.2)
     * Fixed-length data unit for virtual channel transfer
     */
    struct VcaSdu {
        U8* data;                        ///< SDU payload
        U32 size;                        ///< Size must match configured size
        VcaStatus status;                ///< Optional status fields
    };

    /**
     * Virtual Channel Configuration
     */
    struct VcConfig {
        U8 virtualChannelId;             ///< Virtual Channel ID (0-7)
        U32 sduSize{MAX_SDU_SIZE};   ///< Fixed SDU size for this channel
        U8 priority{0};                  ///< Channel priority (0 highest)
    };

    /**
     * Constructor
     * @param maxQueueDepth Maximum SDUs queued per channel
     */
    explicit VirtualChannelAccess(U32 maxQueueDepth = 10);

    /**
     * Configure a virtual channel
     * @param config Channel configuration
     * @return Status (true if successful)
     */
    bool configureChannel(const VcConfig& config);

    /**
     * VCA.request primitive (CCSDS 132.0-B-3 3.4.3.2)
     * Request transfer of SDU on specified virtual channel
     *
     * @param vcId Virtual Channel ID
     * @param sdu Service Data Unit to transfer
     * @return Status (true if accepted)
     */
    bool request(U8 vcId, const VcaSdu& sdu);

    /**
     * Get next available SDU from highest priority channel
     * @param vcId [out] Virtual Channel ID of retrieved SDU
     * @param sdu [out] Retrieved Service Data Unit
     * @return True if SDU retrieved, false if no data
     */
    bool getNextSdu(U8& vcId, VcaSdu& sdu);

    /**
     * Check if any virtual channel has data
     * @return True if data available
     */
    bool hasData() const;

private:
    typedef struct VirtualChannel_s {
        VcConfig config;
        Os::Queue sduQueue;
        U32 frameCount{0};
    } VirtualChannel_t;

    std::map<U8, VirtualChannel_t> m_channels;  ///< Virtual channels by VCID
    const U32 m_maxQueueDepth;                ///< Maximum queued SDUs per channel
    Os::Mutex m_mutex;                        ///< Thread safety for queue access
};

} // namespace Svc

#endif // SVC_VIRTUAL_CHANNEL_ACCESS_HPP

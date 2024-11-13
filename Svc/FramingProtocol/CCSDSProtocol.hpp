// ======================================================================
// \title  CCSDSProtocol.hpp
// \author Reginald Marr
// \brief  hpp file for CCSDSProtocol class
//
// \copyright
// Copyright 2009-2021, by the California Institute of Technology.
// ALL RIGHTS RESERVED.  United States Government Sponsorship
// acknowledged.
//
// ======================================================================

#ifndef SVC_CCSDS_PROTOCOL_HPP
#define SVC_CCSDS_PROTOCOL_HPP

#include <Svc/FramingProtocol/FramingProtocol.hpp>
#include "Svc/FrameAccumulator/FrameDetector/CCSDSFrameDetector.hpp"
#include "config/FpConfig.h"

namespace Svc {


class SpacePacketFraming : public FramingProtocol {
  public:
    SpacePacketFraming() = default;

    //! Declares the frame method
    void frame(const U8* const data,                     //!< The data
               const U32 size,                           //!< The data size in bytes
               Fw::ComPacket::ComPacketType packet_type  //!< The packet type
               ) override;

  private:
    U16 m_apid{0};
    U16 m_sequenceCount{0};
};

//! \brief Implements the TCSpaceDataLink framing protocol
class TCSpaceDataLinkFraming : public FramingProtocol {

  typedef enum {
      // CCSDS 232.0-B-4 Section 4.1.2.2.3
      DISABLED = 0, // Type-A, perform normal Frame Acceptance Checks
      ENABLED = 1, // Type-B, byepass normal Frame Acceptance Checks
  } BypassFlag;

  typedef enum {
      // CCSDS 232.0-B-4 Section 4.1.2.3.2
      DATA = 0, // Type-D
      COMMAND = 1, // Type-C
  } ControlCommandFlag;

  public:
    //! Constructor
    TCSpaceDataLinkFraming() = default;

    //! Declares the frame method
    void frame(const U8* const data,                     //!< The data
               const U32 size,                           //!< The data size in bytes
               Fw::ComPacket::ComPacketType packet_type  //!< The packet type
               ) override;
    private:
        using TC_CheckSum = FrameDetectors::CCSDSChecksum;

        // Put this in a struct
        U8 m_version{0};
        U8 m_bypassFlag{BypassFlag::ENABLED};
        U8 m_controlCommandFlag{ControlCommandFlag::COMMAND};
        U16 m_spacecraftId{CCSDS_SCID};
        U8 m_virtualChannelId{0};
        U8 m_frameSequence{0};
};

//! \brief Implements the TMSpaceDataLink framing protocol
class TMSpaceDataLinkFraming : public FramingProtocol {

  typedef enum {
      // CCSDS 232.0-B-4 Section 4.1.2.2.3
      DISABLED = 0, // Type-A, perform normal Frame Acceptance Checks
      ENABLED = 1, // Type-B, byepass normal Frame Acceptance Checks
  } BypassFlag;

  typedef enum {
      // CCSDS 232.0-B-4 Section 4.1.2.3.2
      COMMAND = 0, // Type-D
      DATA = 1, // Type-C
  } ControlCommandFlag;

  public:
    //! Constructor
    TMSpaceDataLinkFraming() = default;

    //! Declares the frame method
    void frame(const U8* const data,                     //!< The data
               const U32 size,                           //!< The data size in bytes
               Fw::ComPacket::ComPacketType packet_type  //!< The packet type
               ) override;
    private:
        using CheckSum = FrameDetectors::CCSDSChecksum;

        // Member variables
        // First 2 bits of master channel ID
        U8 m_tfVersionNumber{0}; // 2 bits
        // Next 10 bits of master channel ID
        U16 m_spacecraftId{CCSDS_SCID}; // 10 bits
        U8 m_virtualChannelId{0};          // 3 bits
        bool m_operationalControlFlag{false}; // 1 bit
        U8 m_masterChannelFrameCount{0};    // 8 bits
        U8 m_virtualChannelFrameCount{0};   // 8 bits
        bool m_hasSecondaryHeader{false};
        bool m_isVcaSdu{false};


        U16 constructDataFieldStatus() {
            // Implementation depends on mission-specific requirements
            // This is a placeholder implementation
            U16 status = 0;

            // Example fields that might be included:
            // - Secondary Header Present
            // - Synchronization Flag
            // - Packet Order Flag
            // - Segment Length ID
            // - First Header Pointer

            return status;
        }
};

}  // namespace Svc
#endif  // SVC_CCSDS_PROTOCOL_HPP

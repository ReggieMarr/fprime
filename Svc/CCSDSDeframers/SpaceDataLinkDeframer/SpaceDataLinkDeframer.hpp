// ======================================================================
// \title  SpaceDataLinkDeframer.hpp
// \author chammard
// \brief  hpp file for SpaceDataLinkDeframer component implementation class
// ======================================================================

#ifndef Svc_SpaceDataLinkDeframer_HPP
#define Svc_SpaceDataLinkDeframer_HPP

#include "Svc/CCSDSDeframers/SpaceDataLinkDeframer/SpaceDataLinkDeframerComponentAc.hpp"
#include "Svc/FramingProtocol/CCSDSProtocols/CCSDSProtocolDefs.hpp"

namespace Svc {

// Implemented as per CCSDS 232.0-B-4
class SpaceDataLinkDeframer : public SpaceDataLinkDeframerComponentBase {

  static const U8 TC_SPACE_DATA_LINK_HEADER_SIZE = 5;
  static const U8 TC_SPACE_DATA_LINK_TRAILER_SIZE = 2;
  // CCSDS 232.0-B-4 Section 4.1.2.7.3
  static const U16 TC_SPACE_DATA_LINK_MAX_TRANSFER_FRAME_SIZE = 1024;
  public:
    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct SpaceDataLinkDeframer object
    SpaceDataLinkDeframer(const char* const compName  //!< The component name
    );

    //! Destroy SpaceDataLinkDeframer object
    ~SpaceDataLinkDeframer();

  PRIVATE:
    // ----------------------------------------------------------------------
    // Handler implementations for user-defined typed input ports
    // ----------------------------------------------------------------------

    //! Handler implementation for framedIn
    //!
    //! Port to receive framed data
    void framedIn_handler(FwIndexType portNum,  //!< The port number
                          Fw::Buffer& data,
                          Fw::Buffer& context) override;
};

}  // namespace Svc

#endif

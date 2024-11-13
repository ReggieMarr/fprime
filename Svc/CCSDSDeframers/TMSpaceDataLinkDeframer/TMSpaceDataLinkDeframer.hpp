// ======================================================================
// \title  TMSpaceDataLinkDeframer.hpp
// \author chammard
// \brief  hpp file for TMSpaceDataLinkDeframer component implementation class
// ======================================================================

#ifndef Svc_TMSpaceDataLinkDeframer_HPP
#define Svc_TMSpaceDataLinkDeframer_HPP

#include "Svc/CCSDSDeframers/TMSpaceDataLinkDeframer/TMSpaceDataLinkDeframerComponentAc.hpp"

namespace Svc {

// Implemented as per CCSDS 232.0-B-4
class TMSpaceDataLinkDeframer : public TMSpaceDataLinkDeframerComponentBase {

  static const U8 TC_SPACE_DATA_LINK_HEADER_SIZE = 5;
  static const U8 TC_SPACE_DATA_LINK_TRAILER_SIZE = 2;
  // CCSDS 232.0-B-4 Section 4.1.2.7.3
  static const U16 TC_SPACE_DATA_LINK_MAX_TRANSFER_FRAME_SIZE = 1024;
  public:
    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct TMSpaceDataLinkDeframer object
    TMSpaceDataLinkDeframer(const char* const compName  //!< The component name
    );

    //! Destroy TMSpaceDataLinkDeframer object
    ~TMSpaceDataLinkDeframer();

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

    void processDataFieldStatus(U16 status);
    // Constants
    static constexpr U32 TM_SPACE_DATA_LINK_HEADER_SIZE = 6;
    static constexpr U32 TM_SPACE_DATA_LINK_TRAILER_SIZE = 2;  // For Frame Error Control Field
    static constexpr U32 TM_SPACE_DATA_LINK_MAX_TRANSFER_FRAME_SIZE = 1115;  // Example size
};

}  // namespace Svc

#endif

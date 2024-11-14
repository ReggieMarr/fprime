// ======================================================================
// \title  TMSpaceDataLinkDeframer.hpp
// \author chammard
// \brief  hpp file for TMSpaceDataLinkDeframer component implementation class
// ======================================================================

#ifndef Svc_TMSpaceDataLinkDeframer_HPP
#define Svc_TMSpaceDataLinkDeframer_HPP

#include "FpConfig.h"
#include "Svc/CCSDSDeframers/TMSpaceDataLinkDeframer/TMSpaceDataLinkDeframerComponentAc.hpp"
#include "Svc/FramingProtocol/CCSDSProtocolDefs.hpp"

namespace Svc {

// Implemented as per CCSDS 232.0-B-4
class TMSpaceDataLinkDeframer : public TMSpaceDataLinkDeframerComponentBase {
  public:
    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct TMSpaceDataLinkDeframer object
    TMSpaceDataLinkDeframer(const char* const compName  //!< The component name
    );

    FwSizeType dataFieldSize;

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
};

}  // namespace Svc

#endif

// ======================================================================
// \title  SpacePacketFramer.hpp
// \author user
// \brief  hpp file for SpacePacketFramer component implementation class
// ======================================================================

#ifndef Svc_SpacePacketFramer_HPP
#define Svc_SpacePacketFramer_HPP

#include "Svc/CCSDSFramers/SpacePacketFramer/SpacePacketFramerComponentAc.hpp"
#include "Svc/Framer/Framer.hpp"

namespace Svc {

class SpacePacketFramer : public SpacePacketFramerComponentBase, public Framer {
  public:
    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct SpacePacketFramer object
    SpacePacketFramer(const char* const compName  //!< The component name
    );

    //! Destroy SpacePacketFramer object
    ~SpacePacketFramer();
    // Explicitly bring Framer's implementations into this scope
    using Framer::allocate;
    using Framer::bufferIn_handler;
    using Framer::comIn_handler;
    using Framer::comStatusIn_handler;
    using Framer::handle_framing;
    using Framer::send;
    using Framer::setup;
};

}  // namespace Svc

#endif

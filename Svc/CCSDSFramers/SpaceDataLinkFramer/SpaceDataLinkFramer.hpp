// ======================================================================
// \title  SpaceDataLinkFramer.hpp
// \author user
// \brief  hpp file for SpaceDataLinkFramer component implementation class
// ======================================================================

#ifndef Svc_SpaceDataLinkFramer_HPP
#define Svc_SpaceDataLinkFramer_HPP

#include "Svc/CCSDSFramers/SpaceDataLinkFramer/SpaceDataLinkFramerComponentAc.hpp"
#include "Svc/Framer/Framer.hpp"

namespace Svc {

class SpaceDataLinkFramer : public SpaceDataLinkFramerComponentBase,
                            public Framer

{
  public:
    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct SpaceDataLinkFramer object
    SpaceDataLinkFramer(const char* const compName  //!< The component name
    );

    //! Destroy SpaceDataLinkFramer object
    ~SpaceDataLinkFramer();
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

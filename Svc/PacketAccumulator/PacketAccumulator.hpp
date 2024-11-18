// ======================================================================
// \title  PacketAccumulator.hpp
// \author user
// \brief  hpp file for PacketAccumulator component implementation class
// ======================================================================

#ifndef Svc_PacketAccumulator_HPP
#define Svc_PacketAccumulator_HPP

#include "Svc/PacketAccumulator/PacketAccumulatorComponentAc.hpp"

namespace Svc {

  class PacketAccumulator :
    public PacketAccumulatorComponentBase
  {

    public:

      // ----------------------------------------------------------------------
      // Component construction and destruction
      // ----------------------------------------------------------------------

      //! Construct PacketAccumulator object
      PacketAccumulator(
          const char* const compName //!< The component name
      );

      //! Destroy PacketAccumulator object
      ~PacketAccumulator();

    PRIVATE:

      // ----------------------------------------------------------------------
      // Handler implementations for user-defined typed input ports
      // ----------------------------------------------------------------------

      //! Handler implementation for TODO
      //!
      //! TODO
      void TODO_handler(
          FwIndexType portNum, //!< The port number
          U32 context //!< The call order
      ) override;
      void comIn_handler(const NATIVE_INT_TYPE portNum, Fw::ComBuffer& data, U32 context);
      void bufferIn_handler(const NATIVE_INT_TYPE portNum, Fw::Buffer& fwBuffer);

  };

}

#endif

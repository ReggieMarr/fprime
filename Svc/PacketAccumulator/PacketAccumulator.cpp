// ======================================================================
// \title  PacketAccumulator.cpp
// \author user
// \brief  cpp file for PacketAccumulator component implementation class
// ======================================================================

#include "FpConfig.hpp"
#include "PacketAccumulator.hpp"
#include "Svc/PacketAccumulator/PacketAccumulator.hpp"

namespace Svc {

  // ----------------------------------------------------------------------
  // Component construction and destruction
  // ----------------------------------------------------------------------

  PacketAccumulator ::
    PacketAccumulator(const char* const compName) :
      PacketAccumulatorComponentBase(compName)
  {

  }

  PacketAccumulator ::
    ~PacketAccumulator()
  {

  }

  void PacketAccumulator ::comIn_handler(const NATIVE_INT_TYPE portNum, Fw::ComBuffer& data, U32 context) {
      // this->handle_framing(data.getBuffAddr(), data.getBuffLength(), Fw::ComPacket::FW_PACKET_UNKNOWN);
  }

  void PacketAccumulator ::bufferIn_handler(const NATIVE_INT_TYPE portNum, Fw::Buffer& fwBuffer) {
      // this->handle_framing(fwBuffer.getData(), fwBuffer.getSize(), Fw::ComPacket::FW_PACKET_FILE);
      // // Deallocate the buffer after it was processed by the framing protocol
      // this->bufferDeallocate_out(0, fwBuffer);
  }
  // ----------------------------------------------------------------------
  // Handler implementations for user-defined typed input ports
  // ----------------------------------------------------------------------

  void PacketAccumulator ::
    TODO_handler(
        FwIndexType portNum,
        U32 context
    )
  {
    // TODO
  }

}

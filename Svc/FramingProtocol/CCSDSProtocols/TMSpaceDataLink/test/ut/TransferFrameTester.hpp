// TMSpaceDataLinkTest.hpp
#ifndef TM_SPACE_DATA_LINK_TEST_HPP
#define TM_SPACE_DATA_LINK_TEST_HPP

#include "Fw/Com/ComBuffer.hpp"
#include "Svc/FramingProtocol/CCSDSProtocols/TMSpaceDataLink/TransferFrame.hpp"
#include "Svc/FramingProtocol/CCSDSProtocols/TMSpaceDataLink/TransferFrameDefs.hpp"

namespace TMSpaceDataLink {

class FprimeTransferFrameTester {
  public:

    // ----------------------------------------------------------------------
    // Construction
    // ----------------------------------------------------------------------

    //! Construct a FramingTester
    FprimeTransferFrameTester();
    // ----------------------------------------------------------------------
    // Public member functions
    // ----------------------------------------------------------------------

    //! Check framing
  void check(PrimaryHeaderControlInfo_t const &header, FPrimeDataField const &data);

    // ----------------------------------------------------------------------
    // Private member functions
    // ----------------------------------------------------------------------

  private:

    //! Get the packet size from the buffer
    // FpFrameHeader::TokenType getPacketSize();

    //! Check the packet size in the buffer
    // void checkPacketSize(
    //     FpFrameHeader::TokenType packetSize //!< The packet size
    // );

    //! Check the packet type in the buffer
    // void checkPacketType();

    //! Check the start word in the buffer
    // void checkStartWord();

    //! Check the data in the buffer
    // void checkData();

    //! Check the hash value in the buffer
    // void checkHash(
    //     FpFrameHeader::TokenType packetSize //!< The packet size
    // );

  protected:
    void setup();

    void teardown();
};

}  // namespace TMSpaceDataLink

#endif

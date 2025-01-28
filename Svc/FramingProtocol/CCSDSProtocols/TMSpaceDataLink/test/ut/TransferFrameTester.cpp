// ======================================================================
// \title  FramingTester.cpp
// \author bocchino
// \brief  cpp file for FramingTester class
// ======================================================================

#include "STest/Pick/Pick.hpp"
#include "Svc/FramingProtocol/CCSDSProtocols/TMSpaceDataLink/test/ut/TransferFrameTester.hpp"
#include "Svc/FramingProtocol/CCSDSProtocols/TMSpaceDataLink/TransferFrame.hpp"
#include "gtest/gtest.h"

namespace TMSpaceDataLink {

  // ----------------------------------------------------------------------
  // Construction
  // ----------------------------------------------------------------------

  FprimeTransferFrameTester ::
    FprimeTransferFrameTester()
  {
    // FW_ASSERT(this->dataSize <= MAX_DATA_SIZE);
    // this->fprimeFraming.setup(this->interface);
    // // Fill in random data
    // for (U32 i = 0; i < sizeof(this->data); ++i) {
    //   this->data[i] = STest::Pick::lowerUpper(0, 0xFF);
    // }
    // memset(this->bufferStorage, 0, sizeof this->bufferStorage);
  }

  // ----------------------------------------------------------------------
  // Public member functions
  // ----------------------------------------------------------------------

  void FprimeTransferFrameTester ::
    check(PrimaryHeaderControlInfo_t const &header, FPrimeDataField const &data)
  {

    // frame.insert();

  }

  // ----------------------------------------------------------------------
  // Private member functions
  // ----------------------------------------------------------------------

  // void FramingTester ::
  //   checkPacketType()
  // {
  //   SerialPacketType serialPacketType = 0;
  //   Fw::SerialBuffer sb(
  //       &this->bufferStorage[PACKET_TYPE_OFFSET],
  //       sizeof serialPacketType
  //   );
  //   sb.fill();
  //   const Fw::SerializeStatus status = sb.deserialize(serialPacketType);
  //   FW_ASSERT(status == Fw::FW_SERIALIZE_OK, status);
  //   typedef Fw::ComPacket::ComPacketType PacketType;
  //   const PacketType pt = static_cast<PacketType>(serialPacketType);
  //   ASSERT_EQ(pt, this->packetType);
  // }

  // void FramingTester ::
  //   checkStartWord()
  // {
  //   FpFrameHeader::TokenType startWord = 0;
  //   Fw::SerialBuffer sb(
  //       &this->bufferStorage[START_WORD_OFFSET],
  //       sizeof startWord
  //   );
  //   sb.fill();
  //   const Fw::SerializeStatus status = sb.deserialize(startWord);
  //   FW_ASSERT(status == Fw::FW_SERIALIZE_OK, status);
  //   ASSERT_EQ(startWord, FpFrameHeader::START_WORD);
  // }

  // void FramingTester ::
  //   checkData()
  // {
  //   U32 dataOffset = PACKET_TYPE_OFFSET;
  //   if (this->packetType != Fw::ComPacket::FW_PACKET_UNKNOWN) {
  //     // Packet type is stored in header
  //     dataOffset += sizeof(SerialPacketType);
  //   }
  //   const I32 result = memcmp(
  //       this->data,
  //       &this->bufferStorage[dataOffset],
  //       this->dataSize
  //   );
  //   ASSERT_EQ(result, 0);
  // }

  // void FramingTester ::
  //   checkHash(FpFrameHeader::TokenType packetSize)
  // {
  //   Utils::Hash hash;
  //   Utils::HashBuffer hashBuffer;
  //   const U32 dataSize = FpFrameHeader::SIZE + packetSize;
  //   hash.update(this->bufferStorage,  dataSize);
  //   hash.final(hashBuffer);
  //   const U8 *const hashAddr = hashBuffer.getBuffAddr();
  //   const I32 result = memcmp(
  //       &this->bufferStorage[dataSize],
  //       hashAddr,
  //       HASH_DIGEST_LENGTH
  //   );
  //   ASSERT_EQ(result, 0);
  // }

}

#include <cstdio>
#include <cstring>

#include "Fw/Test/UnitTest.hpp"
#include "STest/Pick/Pick.hpp"
#include "STest/Random/Random.hpp"
#include "Svc/FramingProtocol/CCSDSProtocols/TMSpaceDataLink/TransferFrameDefs.hpp"
#include "Svc/FramingProtocol/CCSDSProtocols/TMSpaceDataLink/TransferFrame.hpp"
#include "Fw/Com/ComBuffer.hpp"
#include "Svc/FramingProtocol/test/ut/DeframingTester.hpp"
#include "Svc/FramingProtocol/test/ut/FramingTester.hpp"
#include "Fw/Test/UnitTest.hpp"
#include "gtest/gtest.h"

// ----------------------------------------------------------------------
// Tests
// ----------------------------------------------------------------------

static void setRandomControlInfo(TMSpaceDataLink::PrimaryHeaderControlInfo_t &ci) {
  ci.virtualChannelFrameCount = STest::Pick::lowerUpper(0, 0xFF);
  ci.masterChannelFrameCount = STest::Pick::lowerUpper(0, 0xFF);
  ci.spacecraftId = STest::Pick::lowerUpper(0, 0b1111111111);
  ci.transferFrameVersion = STest::Pick::lowerUpper(0, 0b11);
  ci.virtualChannelId = STest::Pick::lowerUpper(0, 0b111);

  ci.operationalControlFlag = STest::Pick::lowerUpper(0, 1);
  ci.dataFieldStatus.hasSecondaryHeader = STest::Pick::lowerUpper(0, 1);
  ci.dataFieldStatus.isSyncFlagEnabled = STest::Pick::lowerUpper(0, 1);
  ci.dataFieldStatus.isPacketOrdered = STest::Pick::lowerUpper(0, 1);
  ci.dataFieldStatus.segmentLengthId = STest::Pick::lowerUpper(0, 0b111);
  ci.dataFieldStatus.firstHeaderPointer = STest::Pick::lowerUpper(0, 0b11111111111);
}

TEST(FPrimeFraming, HeaderSetterTest) {
  COMMENT("Testing PrimaryHeader set/get");
  // REQUIREMENT("Svc-FramingProtocol-003");
  Fw::ComBuffer inputBuffer;
  TMSpaceDataLink::PrimaryHeaderControlInfo_t controlInfoIn;
  TMSpaceDataLink::PrimaryHeaderControlInfo_t controlInfoOut;
  TMSpaceDataLink::PrimaryHeader header;

  setRandomControlInfo(controlInfoIn);
  setRandomControlInfo(controlInfoOut);

  //gurantee at least some differences
  controlInfoOut.operationalControlFlag = !controlInfoIn.operationalControlFlag;
  controlInfoOut.dataFieldStatus.hasSecondaryHeader = !controlInfoIn.dataFieldStatus.hasSecondaryHeader;
  controlInfoOut.dataFieldStatus.isSyncFlagEnabled = !controlInfoIn.dataFieldStatus.isSyncFlagEnabled;

  ASSERT_NE(controlInfoOut, controlInfoIn);

  header.set(controlInfoIn);
  header.get(controlInfoOut);
  ASSERT_EQ(controlInfoOut, controlInfoIn);
}

TEST(FPrimeFraming, FrameSetterTest) {
  COMMENT("Test the FPrimeFrame set/get");
  // REQUIREMENT("Svc-FramingProtocol-003");

  bool status;
  Fw::ComBuffer buffer;
  TMSpaceDataLink::PrimaryHeaderControlInfo_t controlInfo;
  TMSpaceDataLink::FPrimeTransferFrame frameIn;
  TMSpaceDataLink::FPrimeTransferFrame frameOut;
  TMSpaceDataLink::FPrimeDataField::FieldValue_t data;

  setRandomControlInfo(controlInfo);

  frameIn.primaryHeader.set(controlInfo);

  // Fill in random data
  for (U32 i = 0; i < data.size(); i++) {
    data.at(i) = STest::Pick::lowerUpper(0, 0xFF);
  }

  frameIn.dataField.set(data);

  (void)std::memset(buffer.getBuffAddr(), 0, frameIn.SERIALIZED_SIZE);
  buffer.setBuffLen(frameIn.SERIALIZED_SIZE);
  buffer.resetSer();

  status = frameIn.insert(buffer);
  ASSERT_EQ(status, true);

  status = frameOut.extract(buffer);
  ASSERT_EQ(status, true);

  ASSERT_EQ(frameIn, frameOut);
}

// ----------------------------------------------------------------------
// Main function
// ----------------------------------------------------------------------

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    STest::Random::seed();
    return RUN_ALL_TESTS();
}

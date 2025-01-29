#include <array>
#include <cstdio>
#include <cstring>

#include "Fw/Buffer/Buffer.hpp"
#include "Fw/Com/ComBuffer.hpp"
#include "Fw/Test/UnitTest.hpp"
#include "STest/Pick/Pick.hpp"
#include "STest/Random/Random.hpp"
#include "Svc/FramingProtocol/CCSDSProtocols/TMSpaceDataLink/TransferFrame.hpp"
#include "Svc/FramingProtocol/CCSDSProtocols/TMSpaceDataLink/TransferFrameDefs.hpp"
#include "Svc/FramingProtocol/test/ut/DeframingTester.hpp"
#include "Svc/FramingProtocol/test/ut/FramingTester.hpp"
#include "gtest/gtest.h"

// ----------------------------------------------------------------------
// Tests
// ----------------------------------------------------------------------

static void setRandomControlInfo(TMSpaceDataLink::PrimaryHeaderControlInfo_t& ci) {
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

static void setRandomData(TMSpaceDataLink::FPrimeDataField::FieldValue_t& data) {
    for (U32 i = 0; i < data.size(); i++) {
        // U8 originalData = data.at(i);
        data.at(i) = STest::Pick::lowerUpper(0, 0xFF);
        // we expected that the data has changed, this won't always be the case
        // but we should be notified when it is
        // EXPECT_NE(originalData, data.at(i));
    }
}

TEST(FPrimeFraming, HeaderSetterTest) {
    COMMENT("Testing PrimaryHeader set/get");
    Fw::ComBuffer buffer;
    TMSpaceDataLink::PrimaryHeaderControlInfo_t controlInfoIn;
    TMSpaceDataLink::PrimaryHeaderControlInfo_t controlInfoOut;
    TMSpaceDataLink::PrimaryHeader header;

    setRandomControlInfo(controlInfoIn);
    setRandomControlInfo(controlInfoOut);

    // gurantee at least some differences
    controlInfoOut.operationalControlFlag = !controlInfoIn.operationalControlFlag;
    controlInfoOut.dataFieldStatus.hasSecondaryHeader = !controlInfoIn.dataFieldStatus.hasSecondaryHeader;
    controlInfoOut.dataFieldStatus.isSyncFlagEnabled = !controlInfoIn.dataFieldStatus.isSyncFlagEnabled;

    ASSERT_NE(controlInfoOut, controlInfoIn);

    header.set(controlInfoIn);
    header.get(controlInfoOut);
    ASSERT_EQ(controlInfoOut, controlInfoIn);
}

TEST(FPrimeFraming, DataFieldValidate) {
    COMMENT("Testing DataField and FrameErrorControlField setting/getting");
    Fw::ComBuffer comBuff;
    TMSpaceDataLink::FPrimeDataField::FieldValue_t dataIn, dataOut;
    TMSpaceDataLink::FPrimeDataField dataFieldIn, dataFieldOut;
    TMSpaceDataLink::FPrimeErrorControlField errorControlField;
    bool status = false;;

    ASSERT_GT(comBuff.getBuffCapacity(), dataFieldIn.SERIALIZED_SIZE);

    // Fill both data sets with unique sets of unique data
    setRandomData(dataIn);
    setRandomData(dataOut);

    dataFieldIn.set(dataIn);

    dataFieldOut.set(dataOut);
    ASSERT_NE(dataFieldIn, dataFieldOut);

    dataFieldOut.get(dataIn);
    ASSERT_EQ(dataIn, dataOut);

    // The error control fields CRC check assumes that the buffer starts with a header
    comBuff.resetSer();
    status = dataFieldIn.insert(comBuff);
    ASSERT_EQ(status, true);

    U16 calculatedCrc, retrievedCrc;
    Fw::Buffer buff(comBuff.getBuffAddr(), dataFieldIn.SERIALIZED_SIZE);
    errorControlField.get(buff, calculatedCrc);
    status = errorControlField.insert(comBuff);
    ASSERT_EQ(status, true);

    dataFieldOut.extract(comBuff);
    ASSERT_EQ(dataFieldIn, dataFieldOut);

    errorControlField.extract(comBuff, retrievedCrc);
    ASSERT_EQ(calculatedCrc, retrievedCrc);
}

TEST(FPrimeFraming, FrameSetterTest) {
    COMMENT("Test the FPrimeFrame set/get");

    bool status;
    Fw::ComBuffer comBuff;
    TMSpaceDataLink::PrimaryHeaderControlInfo_t controlInfo;
    TMSpaceDataLink::FPrimeTransferFrame frameIn;
    TMSpaceDataLink::FPrimeTransferFrame frameOut;
    TMSpaceDataLink::FPrimeDataField::FieldValue_t data;
    TMSpaceDataLink::FPrimeDataField dataField;

    setRandomControlInfo(controlInfo);

    frameIn.primaryHeader.set(controlInfo);

    // Fill in random data
    for (U32 i = 0; i < data.size(); i++) {
        data.at(i) = STest::Pick::lowerUpper(0, 0xFF);
    }

    frameIn.dataField.set(data);

    // (void)std::memset(comBuff.getBuffAddr(), 0, frameIn.SERIALIZED_SIZE);
    comBuff.resetSer();

    U16 calculatedCrc, retrievedCrc;

    status = frameIn.insert(comBuff);
    ASSERT_EQ(status, true);

    frameIn.errorControlField.get(calculatedCrc);

    status = frameOut.extract(comBuff);
    ASSERT_EQ(status, true);

    ASSERT_EQ(frameIn.primaryHeader, frameOut.primaryHeader);
    ASSERT_EQ(frameIn.secondaryHeader, frameOut.secondaryHeader);
    ASSERT_EQ(frameIn.operationalControlField, frameOut.operationalControlField);
    ASSERT_EQ(frameIn.dataField, frameOut.dataField);
    ASSERT_EQ(frameIn.errorControlField, frameOut.errorControlField);
    ASSERT_EQ(frameIn, frameOut);

    frameOut.errorControlField.get(retrievedCrc);
    ASSERT_EQ(retrievedCrc, calculatedCrc);

}

// TEST(FPrimeFraming, FrameDetectorTest) {
//     COMMENT("Test the FPrimeFrameDetector/accumulator stuff");

//     // TODO
// }

// ----------------------------------------------------------------------
// Main function
// ----------------------------------------------------------------------

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    STest::Random::seed();
    return RUN_ALL_TESTS();
}

#include <array>
#include <cstdio>
#include <cstring>
#include <limits>

#include "FpConfig.h"
#include "Fw/Buffer/Buffer.hpp"
#include "Fw/Com/ComBuffer.hpp"
#include "Fw/Test/UnitTest.hpp"
#include "STest/Random/Random.hpp"
#include "Svc/FramingProtocol/CCSDSProtocols/TMSpaceDataLink/Channels.hpp"
#include "Svc/FramingProtocol/CCSDSProtocols/TMSpaceDataLink/ManagedParameters.hpp"
#include "Svc/FramingProtocol/CCSDSProtocols/TMSpaceDataLink/ProtocolInterface.hpp"
#include "Svc/FramingProtocol/CCSDSProtocols/TMSpaceDataLink/TransferFrame.hpp"
#include "Svc/FramingProtocol/CCSDSProtocols/TMSpaceDataLink/TransferFrameDefs.hpp"
#include "Svc/FramingProtocol/test/ut/DeframingTester.hpp"
#include "Svc/FramingProtocol/test/ut/FramingTester.hpp"
#include "gtest/gtest.h"

// ----------------------------------------------------------------------
// Tests
// ----------------------------------------------------------------------

static void pickRandomHeader(TMSpaceDataLink::PrimaryHeaderControlInfo_t& ci) {
    ci.virtualChannelFrameCount = STest::Random::lowerUpper(0, 0xFF);
    ci.masterChannelFrameCount = STest::Random::lowerUpper(0, 0xFF);
    ci.spacecraftId = STest::Random::lowerUpper(0, 0b1111111111);
    ci.transferFrameVersion = STest::Random::lowerUpper(0, 0b11);
    ci.virtualChannelId = STest::Random::lowerUpper(0, 0b111);

    ci.operationalControlFlag = STest::Random::lowerUpper(0, 1);
    ci.dataFieldStatus.hasSecondaryHeader = STest::Random::lowerUpper(0, 1);
    ci.dataFieldStatus.isSyncFlagEnabled = STest::Random::lowerUpper(0, 1);
    ci.dataFieldStatus.isPacketOrdered = STest::Random::lowerUpper(0, 1);
    ci.dataFieldStatus.segmentLengthId = STest::Random::lowerUpper(0, 0b111);
    ci.dataFieldStatus.firstHeaderPointer = STest::Random::lowerUpper(0, 0b11111111111);
}

static void setRandomData(TMSpaceDataLink::FPrimeDataField::FieldValue_t& data) {
    for (U32 i = 0; i < data.size(); i++) {
        // U8 originalData = data.at(i);
        data.at(i) = STest::Random::lowerUpper(0, 0xFF);
        // we expected that the data has changed, this won't always be the case
        // but we should be notified when it is
        // EXPECT_NE(originalData, data.at(i));
    }
}

static TMSpaceDataLink::VirtualChannel::Id_t pickRandomGVCID() {
    TMSpaceDataLink::VirtualChannel::Id_t vcId = {
        .MCID =
            {
                .SCID = static_cast<U8>(STest::Random::lowerUpper(0, std::numeric_limits<U16>::max())),
                .TFVN = static_cast<U8>(STest::Random::lowerUpper(0, std::numeric_limits<U8>::max())),
            },
        .VCID = static_cast<U8>(STest::Random::lowerUpper(0, std::numeric_limits<U8>::max()))};
    return vcId;
}

TEST(FPrimeFraming, HeaderSetterTest) {
    Fw::ComBuffer buffer;
    TMSpaceDataLink::PrimaryHeaderControlInfo_t controlInfoIn;
    TMSpaceDataLink::PrimaryHeaderControlInfo_t controlInfoOut;
    TMSpaceDataLink::PrimaryHeader headerIn, headerOut;

    COMMENT("Populate control info in/out with unique sets of random data");
    pickRandomHeader(controlInfoIn);
    controlInfoIn.masterChannelFrameCount = 1;
    pickRandomHeader(controlInfoOut);
    controlInfoOut.operationalControlFlag = !controlInfoIn.operationalControlFlag;
    controlInfoOut.dataFieldStatus.hasSecondaryHeader = !controlInfoIn.dataFieldStatus.hasSecondaryHeader;
    controlInfoOut.dataFieldStatus.isSyncFlagEnabled = !controlInfoIn.dataFieldStatus.isSyncFlagEnabled;
    ASSERT_NE(controlInfoOut, controlInfoIn);

    COMMENT("Validate the header setter/getter");
    headerIn.set(controlInfoIn);
    headerIn.get(controlInfoOut);
    ASSERT_EQ(controlInfoOut, controlInfoIn);

    headerOut.set(controlInfoOut);
    ASSERT_EQ(headerIn, headerOut);

    COMMENT("Populate the control info out with a new and different set of data from the input");
    pickRandomHeader(controlInfoOut);
    controlInfoOut.operationalControlFlag = !controlInfoIn.operationalControlFlag;
    controlInfoOut.dataFieldStatus.hasSecondaryHeader = !controlInfoIn.dataFieldStatus.hasSecondaryHeader;
    controlInfoOut.dataFieldStatus.isSyncFlagEnabled = !controlInfoIn.dataFieldStatus.isSyncFlagEnabled;
    ASSERT_NE(controlInfoOut, controlInfoIn);

    headerOut.set(controlInfoOut);
    ASSERT_NE(headerIn, headerOut);

    COMMENT("Serialize the header into a combuffer");
    headerIn.insert(buffer);

    headerOut.extract(buffer);

    COMMENT("After extracting a header object from the buffer validate they are the same");
    ASSERT_EQ(headerIn, headerOut);
}

TEST(FPrimeFraming, DataFieldValidate) {
    COMMENT("Testing DataField and FrameErrorControlField setting/getting");
    Fw::ComBuffer comBuff;
    TMSpaceDataLink::FPrimeDataField::FieldValue_t dataIn, dataOut;
    TMSpaceDataLink::FPrimeDataField dataFieldIn, dataFieldOut;
    TMSpaceDataLink::FPrimeErrorControlField errorControlField;
    bool status = false;

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

    pickRandomHeader(controlInfo);

    frameIn.primaryHeader.set(controlInfo);

    // Fill in random data
    for (U32 i = 0; i < data.size(); i++) {
        data.at(i) = STest::Random::lowerUpper(0, 0xFF);
    }

    frameIn.dataField.set(data);

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

TEST(ChannelTest, VirtualChannelTest) {
    bool status;
    TMSpaceDataLink::VirtualChannel::Id_t vcId = {
        .MCID =
            {
                .SCID = static_cast<U8>(STest::Random::lowerUpper(0, std::numeric_limits<U16>::max())),
                .TFVN = static_cast<U8>(STest::Random::lowerUpper(0, std::numeric_limits<U8>::max())),
            },
        .VCID = static_cast<U8>(STest::Random::lowerUpper(0, std::numeric_limits<U8>::max()))};

    TMSpaceDataLink::VirtualChannel vc(vcId);
    ASSERT_EQ(vc.id, vcId);

    static constexpr FwSizeType dataSize = TMSpaceDataLink::FPrimeTransferFrame::SERIALIZED_SIZE;
    enum {
        IN_IDX = 0,
        OUT_IDX,
        IDX_SIZE,
    };
    std::array<std::array<std::array<U8, dataSize>, vc.DEPTH>, IDX_SIZE> rawBuff;

    // Ensure we're starting with an empty queue
    ASSERT_EQ(vc.m_externalQueue.getMessagesAvailable(), 0);

    // Fill the queue to the max
    for (FwSizeType i = 0; i < vc.DEPTH; i++) {
        U8 dataVal = STest::Random::lowerUpper(0, std::numeric_limits<U8>::max());
        std::memset(rawBuff.at(IN_IDX).at(i).data(), dataVal, rawBuff.at(IN_IDX).at(i).size());

        Fw::Buffer data(rawBuff.at(IN_IDX).at(i).data(), rawBuff.at(IN_IDX).at(i).size());
        status = vc.transfer(data);
        ASSERT_TRUE(status);

        ASSERT_EQ(vc.m_externalQueue.getMessagesAvailable(), i + 1);
    }

    ASSERT_EQ(vc.m_externalQueue.getMessagesAvailable(), vc.m_externalQueue.getMessageHighWaterMark());
    // NOTE unsure why this static cast is neccessary
    ASSERT_EQ(static_cast<FwSizeType>(vc.DEPTH), vc.m_externalQueue.getMessageHighWaterMark());

    Os::QueueInterface::BlockingType m_blockType = Os::QueueInterface::BlockingType::NONBLOCKING;

    for (FwSizeType i = 0; i < vc.DEPTH; i++) {
        Os::Queue::Status qStatus;
        FwQueuePriorityType currentPriority = 0;
        FwSizeType actualSize;
        qStatus = vc.m_externalQueue.receive(rawBuff.at(OUT_IDX).at(i).data(), rawBuff.at(OUT_IDX).at(i).size(),
                                             m_blockType, actualSize, currentPriority);
        ASSERT_EQ(qStatus, Os::Queue::Status::OP_OK);

        ASSERT_EQ(rawBuff.at(OUT_IDX).at(i).size(), rawBuff.at(IN_IDX).at(i).size());
        ASSERT_TRUE(std::memcmp(rawBuff.at(OUT_IDX).at(i).data(), rawBuff.at(IN_IDX).at(i).data(),
                                rawBuff.at(IN_IDX).at(i).size()));
    }

    // Check that queue has responded as expected
    ASSERT_EQ(vc.m_externalQueue.getMessagesAvailable(), 0);
    ASSERT_NE(vc.m_externalQueue.getMessagesAvailable(), vc.m_externalQueue.getMessageHighWaterMark());
    ASSERT_EQ(static_cast<FwSizeType>(vc.DEPTH), vc.m_externalQueue.getMessageHighWaterMark());

    // Refill the queue
    for (FwSizeType i = 0; i < vc.DEPTH; i++) {
        U8 dataVal = STest::Random::lowerUpper(0, std::numeric_limits<U8>::max());
        std::memset(rawBuff.at(IN_IDX).at(i).data(), dataVal, rawBuff.at(IN_IDX).at(i).size());

        Fw::Buffer data(rawBuff.at(IN_IDX).at(i).data(), rawBuff.at(IN_IDX).at(i).size());
        status = vc.transfer(data);
        ASSERT_TRUE(status);

        ASSERT_EQ(vc.m_externalQueue.getMessagesAvailable(), i + 1);
    }

    // Attempt to exceed queue depth - should assert
    Fw::Buffer data(rawBuff.at(IN_IDX).at(0).data(), rawBuff.at(IN_IDX).at(0).size());
    ASSERT_DEATH({ vc.transfer(data); }, "Assertion");
}

TEST(ChannelTest, VirtualChannelFrameContent) {
    TMSpaceDataLink::VirtualChannel::Id_t vcId = {
        .MCID =
            {
                .SCID = static_cast<U8>(STest::Random::lowerUpper(0, std::numeric_limits<U16>::max())),
                .TFVN = static_cast<U8>(STest::Random::lowerUpper(0, std::numeric_limits<U8>::max())),
            },
        .VCID = static_cast<U8>(STest::Random::lowerUpper(0, std::numeric_limits<U8>::max()))};
    TMSpaceDataLink::VirtualChannel vc(vcId);

    TMSpaceDataLink::FPrimeTransferFrame frame;
    TMSpaceDataLink::PrimaryHeaderControlInfo_t headerIn;
    bool status;

    pickRandomHeader(headerIn);

    headerIn.spacecraftId = vcId.MCID.SCID;
    headerIn.virtualChannelId = vcId.VCID;
    headerIn.virtualChannelFrameCount = 0;
    headerIn.transferFrameVersion = vcId.MCID.TFVN;

    frame.primaryHeader.set(headerIn);

    enum {
        IN_IDX = 0,
        OUT_IDX,
        IDX_SIZE,
    };
    std::array<std::array<U8, frame.SERIALIZED_SIZE>, IDX_SIZE> rawBuff;
    Fw::Buffer bufferIn(rawBuff.at(IN_IDX).data(), rawBuff.at(IN_IDX).size());
    Fw::Buffer bufferOut(rawBuff.at(OUT_IDX).data(), rawBuff.at(OUT_IDX).size());
    Fw::ComBuffer comBuffIn;

    status = frame.insert(comBuffIn);
    ASSERT_TRUE(status);

    status = vc.transfer(bufferIn);
    ASSERT_TRUE(status);

    Os::Queue::Status qStatus;
    FwQueuePriorityType currentPriority = 0;
    FwSizeType actualSize;
    Os::QueueInterface::BlockingType m_blockType = Os::QueueInterface::BlockingType::NONBLOCKING;
    qStatus = vc.m_externalQueue.receive(rawBuff.at(OUT_IDX).data(), rawBuff.at(OUT_IDX).size(), m_blockType,
                                         actualSize, currentPriority);
    ASSERT_EQ(qStatus, Os::Queue::Status::OP_OK);

    ASSERT_EQ(rawBuff.at(OUT_IDX).size(), rawBuff.at(IN_IDX).size());

    TMSpaceDataLink::FPrimeTransferFrame frameOut;
    TMSpaceDataLink::PrimaryHeaderControlInfo_t headerOut;

    frameOut.extract(comBuffIn);

    frame.primaryHeader.get(headerOut);

    ASSERT_EQ(headerOut.spacecraftId, vcId.MCID.SCID);
    ASSERT_EQ(headerOut.virtualChannelId, vcId.VCID);
    ASSERT_EQ(headerOut.virtualChannelFrameCount, 0);  // First transfer increments after
    ASSERT_EQ(headerOut.transferFrameVersion, vcId.MCID.TFVN);
}

TEST(ChannelTest, MasterChannelAggregatesFrames) {
    // Setup MasterChannel with two VirtualChannels
    TMSpaceDataLink::GVCID_t mcId;
    mcId.MCID.SCID = 1;
    mcId.MCID.TFVN = 0;

    std::array<TMSpaceDataLink::VirtualChannel, NUM_VIRTUAL_CHANNELS> vcs = {
        TMSpaceDataLink::VirtualChannel({mcId.MCID, 0}),
        TMSpaceDataLink::VirtualChannel({mcId.MCID, 1}),
        TMSpaceDataLink::VirtualChannel({mcId.MCID, 2}),
    };

    TMSpaceDataLink::MasterChannel<NUM_VIRTUAL_CHANNELS> mc(mcId.MCID, vcs);

    // Transfer data into each VirtualChannel
    TMSpaceDataLink::MasterChannel<NUM_VIRTUAL_CHANNELS>::TransferOut_t frameIn;
    FwSizeType vcIdx = 0;
    for (TMSpaceDataLink::VirtualChannel& vc : mc.m_subChannels) {
        std::array<U8, TMSpaceDataLink::FPrimeTransferFrame::SERIALIZED_SIZE> rawBuff = {};
        Fw::Buffer buffer(rawBuff.data(), rawBuff.size());
        TMSpaceDataLink::PrimaryHeaderControlInfo_t headerIn;
        TMSpaceDataLink::VirtualChannel::Id_t vcId = vc.id;

        headerIn.masterChannelFrameCount = 0;
        headerIn.virtualChannelFrameCount = 0;
        headerIn.spacecraftId = vcId.MCID.SCID;
        headerIn.virtualChannelId = vcId.VCID;
        headerIn.virtualChannelFrameCount = 0;
        headerIn.transferFrameVersion = vcId.MCID.TFVN;
        frameIn.at(vcIdx).primaryHeader.set(headerIn);

        Fw::ComBuffer comBuff(rawBuff.data(), rawBuff.size());
        ASSERT_TRUE(vc.transfer(buffer));
    }

    // Perform transfer through MasterChannel
    std::nullptr_t nullArg = nullptr;
    bool status;
    TMSpaceDataLink::MasterChannel<NUM_VIRTUAL_CHANNELS>::TransferOut_t frameOut;
    status = mc.transfer(nullArg);
    ASSERT_TRUE(status);

    // Check each frame's master count
    vcIdx = 0;
    for (TMSpaceDataLink::FPrimeTransferFrame& frame : frameOut) {
        Os::Queue::Status qStatus;
        FwQueuePriorityType currentPriority = 0;
        FwSizeType actualSize;
        Os::QueueInterface::BlockingType m_blockType = Os::QueueInterface::BlockingType::NONBLOCKING;

        std::array<U8, TMSpaceDataLink::FPrimeTransferFrame::SERIALIZED_SIZE> rawBuff = {};
        Fw::Buffer buffer(rawBuff.data(), rawBuff.size());

        qStatus = mc.m_externalQueue.receive(rawBuff.data(), rawBuff.size(), m_blockType, actualSize, currentPriority);
        ASSERT_EQ(qStatus, Os::Queue::Status::OP_OK);

        Fw::ComBuffer comBuff(rawBuff.data(), rawBuff.size());
        frame.extract(comBuff);

        TMSpaceDataLink::PrimaryHeaderControlInfo_t header;
        frame.primaryHeader.get(header);
        ASSERT_EQ(header.masterChannelFrameCount, 1);
        ASSERT_EQ(header.virtualChannelFrameCount, 1);
        ASSERT_EQ(header.virtualChannelId, vcIdx++);
    }
}

TEST(ChannelTest, PhysicalChannelTransferTest) {
    COMMENT("Setup PhysicalChannel with one MasterChannel");
    TMSpaceDataLink::GVCID_t mcId;
    mcId.MCID.SCID = 1;
    mcId.MCID.TFVN = 0;

    COMMENT("Create MasterChannel with two VirtualChannels");
    std::array<TMSpaceDataLink::VirtualChannel, NUM_VIRTUAL_CHANNELS> vcs = {
        TMSpaceDataLink::VirtualChannel({mcId.MCID, 0}),
        TMSpaceDataLink::VirtualChannel({mcId.MCID, 1}),
        TMSpaceDataLink::VirtualChannel({mcId.MCID, 2}),
    };

    TMSpaceDataLink::MasterChannel<NUM_VIRTUAL_CHANNELS> master_channel(mcId.MCID, vcs);

    std::array<TMSpaceDataLink::MasterChannel<NUM_VIRTUAL_CHANNELS>, NUM_MASTER_CHANNELS> mcs = {master_channel};
    TMSpaceDataLink::PhysicalChannel<NUM_MASTER_CHANNELS> pc("TestPC", mcs);

    COMMENT("Transfer data into each VirtualChannel");
    TMSpaceDataLink::MasterChannel<NUM_VIRTUAL_CHANNELS>::TransferOut_t frameIn;

    COMMENT("Transfer through PhysicalChannel");
    std::nullptr_t nullArg = nullptr;
    bool status;
    TMSpaceDataLink::PhysicalChannel<NUM_MASTER_CHANNELS>::TransferOut_t frameOut;

    FwSizeType vcIdx;
    for (TMSpaceDataLink::MasterChannel<NUM_VIRTUAL_CHANNELS>& mc : pc.m_subChannels) {
        vcIdx = 0;
        for (TMSpaceDataLink::VirtualChannel& vc : mc.m_subChannels) {
            std::array<U8, TMSpaceDataLink::FPrimeTransferFrame::SERIALIZED_SIZE> rawBuff = {};
            Fw::Buffer buffer(rawBuff.data(), rawBuff.size());
            TMSpaceDataLink::PrimaryHeaderControlInfo_t headerIn;
            TMSpaceDataLink::VirtualChannel::Id_t vcId = vc.id;

            headerIn.masterChannelFrameCount = 0;
            headerIn.virtualChannelFrameCount = 0;
            headerIn.spacecraftId = vcId.MCID.SCID;
            headerIn.virtualChannelId = vcId.VCID;
            headerIn.virtualChannelFrameCount = 0;
            headerIn.transferFrameVersion = vcId.MCID.TFVN;
            frameIn.at(vcIdx).primaryHeader.set(headerIn);

            Fw::ComBuffer comBuff(rawBuff.data(), rawBuff.size());
            ASSERT_TRUE(vc.transfer(buffer));
        }

        status = mc.transfer(nullArg);
        ASSERT_TRUE(status);
    }

    status = pc.transfer(nullArg);
    ASSERT_TRUE(status);

    // Check each frame's master count
    vcIdx = 0;
    for (TMSpaceDataLink::FPrimeTransferFrame& frame : frameOut) {
        Os::Queue::Status qStatus;
        FwQueuePriorityType currentPriority = 0;
        FwSizeType actualSize;
        Os::QueueInterface::BlockingType m_blockType = Os::QueueInterface::BlockingType::NONBLOCKING;

        std::array<U8, TMSpaceDataLink::FPrimeTransferFrame::SERIALIZED_SIZE> rawBuff = {};
        Fw::Buffer buffer(rawBuff.data(), rawBuff.size());

        qStatus = pc.m_externalQueue.receive(rawBuff.data(), rawBuff.size(), m_blockType, actualSize, currentPriority);
        ASSERT_EQ(qStatus, Os::Queue::Status::OP_OK);

        Fw::ComBuffer comBuff(rawBuff.data(), rawBuff.size());
        frame.extract(comBuff);

        TMSpaceDataLink::PrimaryHeaderControlInfo_t header;
        frame.primaryHeader.get(header);
        ASSERT_EQ(header.masterChannelFrameCount, 1);
        ASSERT_EQ(header.virtualChannelFrameCount, 1);
        ASSERT_EQ(header.virtualChannelId, vcIdx++);
    }
}

TEST(ChannelTest, GetChannelValidGVCID) {
    TMSpaceDataLink::MCID_t mcid = {
                .SCID = static_cast<U8>(STest::Random::lowerUpper(0, std::numeric_limits<U16>::max())),
                .TFVN = static_cast<U8>(STest::Random::lowerUpper(0, std::numeric_limits<U8>::max()))};

    std::array<TMSpaceDataLink::VirtualChannel::Id_t, NUM_VIRTUAL_CHANNELS> vcIds = {
        TMSpaceDataLink::VirtualChannel::Id_t({
                                             .MCID = {.SCID = mcid.SCID, .TFVN = mcid.TFVN},
                                             .VCID = static_cast<U8>(STest::Random::lowerUpper(0, std::numeric_limits<U8>::max()))
                                              }),
        TMSpaceDataLink::VirtualChannel::Id_t({
                                             .MCID = {.SCID = mcid.SCID, .TFVN = mcid.TFVN},
                                             .VCID = static_cast<U8>(STest::Random::lowerUpper(0, std::numeric_limits<U8>::max()))
                                              }),
        TMSpaceDataLink::VirtualChannel::Id_t({
                                             .MCID = {.SCID = mcid.SCID, .TFVN = mcid.TFVN},
                                             .VCID = static_cast<U8>(STest::Random::lowerUpper(0, std::numeric_limits<U8>::max()))
                                              }),

    };

    std::array<TMSpaceDataLink::VirtualChannel, NUM_VIRTUAL_CHANNELS> vcs = {
        TMSpaceDataLink::VirtualChannel(vcIds.at(0)),
        TMSpaceDataLink::VirtualChannel(vcIds.at(1)),
        TMSpaceDataLink::VirtualChannel(vcIds.at(2)),
    };

    TMSpaceDataLink::MasterChannel<NUM_VIRTUAL_CHANNELS> mc(mcid, vcs);

    for (TMSpaceDataLink::VirtualChannel::Id_t id: vcIds) {
        TMSpaceDataLink::VirtualChannel& vc = mc.getChannel(id);
        ASSERT_EQ(vc.id, id);
    };
}

typedef struct {
  const FwOpcodeType opCode;
  const U32 cmdSeq;
  const U32 testCnt;
} loopbackMsgHeader_t;

constexpr FwSizeType MessageNum = NUM_VIRTUAL_CHANNELS;
constexpr FwSizeType MessageSize = 55;

// Config
// Assumes largest message looks like this "{opCode: 123, testCnt: 1234, cmdSeq: 1234, pld: '12345678'}"
// Provides enough space for that + padding for others
// constexpr std::array<std::array<U8, MessageSize>, MessageNum> ChannelMsgs = {{
//     {'B', 'O', 'N', 'J', 'O', 'U', 'R'}, // Channel 0: "BONJOUR"
//     {'S', 'A', 'L', 'U', 'T', 0, 0},  // Channel 1: "SALUT" (padded with 0s)
//     {0xF0, 0x9F, 0x8D, 0x81, 0, 0, 0} // Channel 2: Oh Canada (padded with 0s)
// }};

static constexpr std::array<TMSpaceDataLink::VirtualChannelParams_t, MessageNum>
    VCParams = {
        {{
             0,           // virtualChannelId = 0,
             MessageSize, // VCA_SDULength = 7 (channelMessages max size),
             0,           // VC_FSHLength = 0,
             false,       // isVC_OCFPresent = false,
         },
         {
             1,           // virtualChannelId = 1,
             MessageSize, // VCA_SDULength = 7 (channelMessages max size),
             0,           // VC_FSHLength = 0,
             false,       // isVC_OCFPresent = false,
         },
         {
             2,           // virtualChannelId = 2,
             MessageSize, // VCA_SDULength = 7 (channelMessages max size),
             0,           // VC_FSHLength = 0,
             false,       // isVC_OCFPresent = false,
         }}};

static constexpr TMSpaceDataLink::MasterChannelParams_t MasterChannelParams = {
    .spaceCraftId = CCSDS_SCID,
    .numSubChannels = 3,
    .subChannels = VCParams,
    .vcMuxScheme = VC_MUX_TYPE::VC_MUX_TIME_DIVSION,
    .MC_FSHLength = 0,
    .isMC_OCFPresent = false,
};

TMSpaceDataLink::ProtocolEntity createProtocolEntity() {

  // NOTE this must be defined on the stack as a consequence of using the
  // non-trivial type Fw::String
  // TODO consider removing that constraint
  const TMSpaceDataLink::PhysicalChannelParams_t PhysicalChannelParams = {
      .channelName = "Loopback Channel",
      .transferFrameSize = 255,
      .transferFrameVersion = 0x00,
      .numSubChannels = 1,
      .subChannels = {MasterChannelParams},
      .mcMuxScheme = MC_MUX_TYPE::MC_MUX_TIME_DIVSION,
      .isFrameErrorControlEnabled = true,
  };

  TMSpaceDataLink::ManagedParameters_t ManagedParams = {
      .physicalParams = PhysicalChannelParams,
  };

  return TMSpaceDataLink::ProtocolEntity(ManagedParams);
}

static void routeMessage(TMSpaceDataLink::ProtocolEntity &protocolEntity, Fw::Buffer &messageBuffer, const U8 vcIdx) {

  // Only one master channel supported
  TMSpaceDataLink::PhysicalChannelParams_t params =
      protocolEntity.m_params.physicalParams;
  FW_ASSERT(params.numSubChannels == 1, params.subChannels.size());
  constexpr FwSizeType masterChannelIdx = 0;
  TMSpaceDataLink::MCID_t mcid = {
      .SCID = params.subChannels.at(masterChannelIdx).spaceCraftId,
      .TFVN = params.transferFrameVersion};
  TMSpaceDataLink::GVCID_t gvcid = {.MCID = mcid,
                                    .VCID =
                                        params.subChannels.at(masterChannelIdx)
                                            .subChannels.at(vcIdx)
                                            .virtualChannelId};
  U32 gvcidVal;
  TMSpaceDataLink::GVCID_t::toVal(gvcid, gvcidVal);

  // NOTE manually offsetting to data for now
  FwSizeType messageSendSize =
      params.subChannels.at(0).subChannels.at(vcIdx).VCA_SDULength;
  std::string messageText(
      reinterpret_cast<const char *>(messageBuffer.getData() +
                                     sizeof(FwPacketDescriptorType)),
      messageSendSize);
  Fw::Logger::log(messageText.c_str());

  protocolEntity.UserComIn_handler(messageBuffer, gvcidVal);
}

static void pickRandomDataFieldData(TMSpaceDataLink::FPrimeDataField::FieldValue_t& data) {
    COMMENT("Populate Message data");
    loopbackMsgHeader_t header = {
        .opCode = static_cast<FwOpcodeType>(STest::Random::lowerUpper(0, std::numeric_limits<U16>::max())),
        .cmdSeq = STest::Random::lowerUpper(0, std::numeric_limits<U32>::max()),
        .testCnt = STest::Random::lowerUpper(0, std::numeric_limits<U32>::max()),
    };

    std::memcpy(data.data(), &header, sizeof(loopbackMsgHeader_t));
    for (U32 j = sizeof(loopbackMsgHeader_t); j < data.size(); j++) {
        data.at(j) = STest::Random::lowerUpper(0, 0xFF);
    }
}

static void sendProtocolEntityData(TMSpaceDataLink::ProtocolEntity &protocol, Fw::Buffer &buffer, U8 const &vcId) {

    COMMENT("Push Message into the ProtocolEntity");
    routeMessage(protocol, buffer, vcId);

    COMMENT("Ensure that master channels are transfered");
    std::nullptr_t null_arg = nullptr;
    for (U32 i = 0; i < protocol.m_physicalChannel.m_subChannels.size(); i++) {
        protocol.m_physicalChannel.m_subChannels.at(i).transfer(null_arg);
    }
}

static void receiveProtocolEntityData(TMSpaceDataLink::ProtocolEntity &protocol,
                                      std::array<U8, TMSpaceDataLink::FPrimeDataField::SERIALIZED_SIZE> & msgs) {
    COMMENT("Push Messages into the ProtocolEntity");
    Fw::Buffer buffer(msgs.data(), msgs.size());
    protocol.generateNextFrame(buffer);
}

TEST(ProtocolInterfaceTester, TestInterfaceMapping) {
    TMSpaceDataLink::ProtocolEntity protocol = createProtocolEntity();
    std::array<std::array<U8, TMSpaceDataLink::FPrimeDataField::SERIALIZED_SIZE>, MessageNum> msgsIn, msgsOut;

    TMSpaceDataLink::FPrimeTransferFrame frameIn, frameOut;
    TMSpaceDataLink::PrimaryHeaderControlInfo_t headerInfoIn;
    TMSpaceDataLink::FPrimeDataField::FieldValue_t data;
    TMSpaceDataLink::FPrimeDataField dataField;

    for (U32 i = 0; i < MessageNum; i++) {
        pickRandomHeader(headerInfoIn);
        frameIn.primaryHeader.set(headerInfoIn);
        pickRandomDataFieldData(data);
        frameIn.dataField.set(data);
        Fw::Buffer msgBuffer(msgsIn.at(i).data(), msgsIn.at(i).size());

        sendProtocolEntityData(protocol, msgBuffer, headerInfoIn.virtualChannelFrameCount);

        COMMENT("Ensure these buffs are different from one another");
        for (FwSizeType msgIdx = 0; msgIdx < msgsOut.size(); msgIdx++) {
            for (FwSizeType buffIdx = 0; buffIdx < msgsOut.size(); buffIdx++) {
                msgsOut.at(msgIdx).at(buffIdx) = ~msgsIn.at(msgIdx).at(buffIdx);
            }
        }
    }


    for (U32 i = 0; i < MessageNum; i++) {
        receiveProtocolEntityData(protocol, msgsOut.at(i));
    }

}

// ----------------------------------------------------------------------
// Main function
// ----------------------------------------------------------------------

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    STest::Random::seed();
    return RUN_ALL_TESTS();
}

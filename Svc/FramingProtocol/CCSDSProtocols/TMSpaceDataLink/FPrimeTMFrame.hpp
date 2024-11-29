#ifndef FPRIME_TM_TRANSFER_FRAME_H_
#define FPRIME_TM_TRANSFER_FRAME_H_
#include "FpConfig.h"
#include "Svc/FramingProtocol/CCSDSProtocols/TMSpaceDataLink/ProtocolDataUnits.hpp"
#include "Svc/FramingProtocol/CCSDSProtocols/TMSpaceDataLink/TransferFrame.hpp"

namespace TMSpaceDataLink {
constexpr FwSizeType FPRIME_VCA_DATA_FIELD_SIZE = 64;
class FPrimeVCA: public DataField<FPRIME_VCA_DATA_FIELD_SIZE>  {
  public:
    // Inherit parent's type definitions
    using Base = DataField<FPRIME_VCA_DATA_FIELD_SIZE>;

    // Inherit constructors
    using Base::DataField;
    // This might get overwritten here
    using typename Base::FieldValue_t;
    using Base::operator=;

    // DataField(Fw::Buffer& srcBuff) {
    //     FW_ASSERT(srcBuff.getSize() == FieldSize, srcBuff.getSize());
    //     FW_ASSERT(this->extract(srcBuff.getSerializeRepr()));
    // }
};

class FPrimeTMFrame : public TransferFrameBase<NullSecondaryHeader, FPrimeVCA, NullOperationalControlField, FrameErrorControlField> {
  public:
    PrimaryHeader Header;
    // No secondary header for this specialization
    FrameErrorControlField ErrorControlField;
    FPrimeVCA DataField;

    FPrimeTMFrame(const MissionPhaseParameters_t& missionParams, Fw::Buffer& dataBuff)
        : Header(missionParams), DataField(dataBuff) {}

    bool insert(Fw::SerializeBufferBase& buffer) const;
    bool insert(Fw::SerializeBufferBase& buffer, TransferData_t transferData, const U8* const data, const U32 size);
    bool extract(Fw::SerializeBufferBase& buffer);
};

}  // namespace TMSpaceDataLink

#endif // FPRIME_TM_TRANSFER_FRAME_H_

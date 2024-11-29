#ifndef FPRIME_TM_TRANSFER_FRAME_H_
#define FPRIME_TM_TRANSFER_FRAME_H_
#include <cstddef>
#include "FpConfig.h"
#include "Svc/FramingProtocol/CCSDSProtocols/TMSpaceDataLink/ProtocolDataUnits.hpp"
#include "Svc/FramingProtocol/CCSDSProtocols/TMSpaceDataLink/TransferFrame.hpp"

namespace TMSpaceDataLink {
constexpr FwSizeType FPRIME_VCA_DATA_FIELD_SIZE = 64;

class FPrimeVCA : public DataField<FPRIME_VCA_DATA_FIELD_SIZE> {
  public:
    // Inherit parent's type definitions
    using Base = DataField<FPRIME_VCA_DATA_FIELD_SIZE>;

    // Inherit constructors
    using Base::DataField;
    // This might get overwritten here
    // using typename Base::FieldValue_t;
    using Base::operator=;

  bool set(U8 *data, FwSizeType size) {

  }
};

class FPrimeTMFrame : public TransferFrameBase<NullField, FPrimeVCA, NullField, FrameErrorControlField> {
  public:
    using Base = TransferFrameBase<NullField, FPrimeVCA, NullField, FrameErrorControlField>;
    using Base::TransferFrameBase;  // Inheriting constructor
    using Base::operator=;          // Inheriting assignment

    // NOTE No secondary header or operational control field for this specialization
    // FPrimeTMFrame(const MissionPhaseParameters_t& missionParams, Fw::Buffer& dataBuff)
    //     : Header(missionParams), DataField(dataBuff) {}

    bool insert(Fw::SerializeBufferBase& buffer, TransferData_t transferData, U8 const* data, FwSizeType const size);
};

}  // namespace TMSpaceDataLink

#endif  // FPRIME_TM_TRANSFER_FRAME_H_

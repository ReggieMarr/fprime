// ======================================================================
// \title  TMSpaceDataLink FPrimeTM.cpp
// \author Reginald Marr
// \brief  Implementation of TMSpaceDataLink TransferFrame for
//         transporting Fprime Telemetry
//
// ======================================================================

#include "FPrimeTMFrame.hpp"
#include <array>
#include "FpConfig.h"
#include "FpConfig.hpp"
#include "Fw/Logger/Logger.hpp"
#include "Fw/Types/Assert.hpp"
#include "Fw/Types/Serializable.hpp"
#include "ProtocolDataUnits.hpp"
#include "ProtocolInterface.hpp"
#include "Svc/FrameAccumulator/FrameDetector.hpp"
#include "Svc/FramingProtocol/CCSDSProtocols/CCSDSProtocolDefs.hpp"
#include "Svc/FramingProtocol/CCSDSProtocols/TMSpaceDataLink/ProtocolDataUnits.hpp"
#include "TransferFrame.hpp"
#include "Utils/Hash/Hash.hpp"
#include "Utils/Types/CircularBuffer.hpp"

namespace TMSpaceDataLink {

bool FPrimeTMFrame::insert(Fw::SerializeBufferBase& buffer, TransferData_t transferData, U8 const* data, FwSizeType const size) {
    return true;
}

}  // namespace TMSpaceDataLink

// ======================================================================
// \title  Router.cpp
// \author thomas-bc
// \brief  cpp file for Router component implementation class
// ======================================================================

#include "Svc/Router/Router.hpp"
#include "FpConfig.hpp"
#include "Fw/Com/ComPacket.hpp"
#include "Fw/Logger/Logger.hpp"
#include "Fw/Types/StringBase.hpp"

namespace Svc {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

Router ::Router(const char* const compName) : RouterComponentBase(compName) {}

Router ::~Router() {}

// ----------------------------------------------------------------------
// Handler implementations for user-defined typed input ports
// ----------------------------------------------------------------------

bool Router::routePacket(NATIVE_INT_TYPE portNum, Fw::Buffer& packet, FwPacketDescriptorType type) {
    // Process the packet
    switch (type) {
        // Handle a command packet
        case Fw::ComPacket::FW_PACKET_COMMAND: {
            if (!isConnected_commandOut_OutputPort(portNum)) {
                Fw::Logger::log("[ERROR] Attempted to route packet to disconnected commandOut port %d\n", portNum);
                return false;
            }
            // Send the com buffer
            Fw::ComBuffer com(packet.getData(), packet.getSize());
            // NOTE currently we don't supprt passing context
            // TODO look into command context and determine if its currently neccessary
            U32 context = 0;
            commandOut_out(portNum, com, context);
            break;
        }
        case Fw::ComPacket::FW_PACKET_PACKETIZED_TLM: {
            Fw::Logger::log("[ERROR] Telem Packet Routing is currently unsupported\n", portNum);
            return false;
            break;
        }
        case Fw::ComPacket::FW_PACKET_TELEM: {
            Fw::Logger::log("[ERROR] Telem Packet Routing is currently unsupported\n", portNum);
            return false;
            break;
        }
        // Handle a file packet
        case Fw::ComPacket::FW_PACKET_FILE: {
            // If the file uplink output port is connected,
            // send the file packet. Otherwise take no action.
            if (!isConnected_fileOut_OutputPort(portNum)) {
                Fw::Logger::log("[ERROR] Attempted to route packet to disconnected fileOut port %d\n", portNum);
                return false;
            }

            // Shift the packet buffer to skip the packet type
            // The FileUplink component does not expect the packet
            // type to be there.
            packet.setData(packet.getData() + sizeof(type));
            packet.setSize(static_cast<U32>(packet.getSize() - sizeof(type)));
            // Send the packet buffer
            fileOut_out(portNum, packet);
            break;
        }
        // Take no action for other packet types
        default:
            // Indicate we failed to find a home for this packet
            Fw::Logger::log("[ERROR] Invalid packet type %d\n", type);
            return false;
    }

    return true;
}

void Router ::dataIn_handler(NATIVE_INT_TYPE portNum, Fw::Buffer& packetBuffer, Fw::Buffer& contextBuffer) {
    // Read the packet type from the packet buffer
    // Fw::ComPacket::ComPacketType_t packetType = Fw::ComPacket::ComPacketType_e::FW_PACKET_UNKNOWN;
    U8 packetType = Fw::ComPacket::ComPacketType::FW_PACKET_UNKNOWN;
    Fw::SerializeStatus status = Fw::FW_SERIALIZE_OK;
    {
        Fw::SerializeBufferBase& serial = packetBuffer.getSerializeRepr();
        status = serial.setBuffLen(packetBuffer.getSize());
        FW_ASSERT(status == Fw::FW_SERIALIZE_OK);
        status = serial.deserialize(packetType);
        FW_ASSERT(status == Fw::FW_SERIALIZE_OK);
        // TODO we used to include this info when asserting, update asserts to use CHECK pattern to account for this
    }

    bool packetRouted = routePacket(portNum, packetBuffer, packetType);

    // Assumes a transfer ownership of the buffer to the receiver
    if (!packetRouted) {
        // Deallocate the packet buffer
        Fw::Logger::log("[WARNING] Failed to route packet type %d from port %d\n", packetType, portNum);
        bufferDeallocate_out(portNum, packetBuffer);
    }
}

void Router ::cmdResponseIn_handler(NATIVE_INT_TYPE portNum,
                                    FwOpcodeType opcode,
                                    U32 cmdSeq,
                                    const Fw::CmdResponse& response) {
    Fw::String respStr;
    response.toString(respStr);
    Fw::Logger::log("portNum: %d opcode: %d cmdSeq: %d \n \t cmdResp %s \n",
                    portNum, opcode, cmdSeq, respStr.toChar());
}
}  // namespace Svc

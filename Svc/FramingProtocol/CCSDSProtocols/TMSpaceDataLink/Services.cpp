// VirtualChannelAccess.cpp
#include "Services.hpp"
#include "Fw/Types/String.hpp"

namespace TMSpaceDataLink {

VirtualChannelAccess::VirtualChannelAccess(U32 maxQueueDepth)
    : m_maxQueueDepth(maxQueueDepth) {}

bool VirtualChannelAccess::configureChannel(const VcConfig& config) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Validate VCID
    if (config.virtualChannelId >= MAX_VIRTUAL_CHANNELS) {
        return false;
    }

    // Create or update channel configuration
    m_channels[config.virtualChannelId].config = config;
    return true;
}

bool VirtualChannelAccess::request(U8 vcId, const VcaSdu& sdu) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_channels.find(vcId);
    if (it == m_channels.end()) {
        return false;  // Channel not configured
    }

    VirtualChannel& channel = it->second;

    // Validate SDU size
    if (sdu.size != channel.config.sduSize) {
        return false;
    }

    // Check queue depth
    if (channel.sduQueue.size() >= m_maxQueueDepth) {
        return false;
    }

    // Copy SDU data and enqueue
    VcaSdu newSdu;
    newSdu.data = new U8[sdu.size];
    newSdu.size = sdu.size;
    std::copy(sdu.data, sdu.data + sdu.size, newSdu.data);
    newSdu.status = sdu.status;

    channel.sduQueue.push(newSdu);
    channel.frameCount++;

    return true;
}

bool VirtualChannelAccess::getNextSdu(U8& vcId, VcaSdu& sdu) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Find highest priority channel with data
    auto it = std::min_element(
        m_channels.begin(),
        m_channels.end(),
        [](const auto& a, const auto& b) {
            if (a.second.sduQueue.empty()) return false;
            if (b.second.sduQueue.empty()) return true;
            return a.second.config.priority < b.second.config.priority;
        }
    );

    if (it == m_channels.end() || it->second.sduQueue.empty()) {
        return false;
    }

    // Get SDU from selected channel
    vcId = it->first;
    sdu = it->second.sduQueue.front();
    it->second.sduQueue.pop();

    return true;
}

bool VirtualChannelAccess::configureChannel(const VcConfig& config) {
    if (config.virtualChannelId >= MAX_VIRTUAL_CHANNELS) {
        return false;
    }

    VirtualChannel_t& channel = m_channels[config.virtualChannelId];

    // Create OS queue for this channel
    Fw::String queueName;
    queueName.format("VC%d", config.virtualChannelId);

    Os::Queue::Status status = channel.queue.create(
        queueName,
        10,  // depth
        MAX_SDU_SIZE
    );

    if (status != Os::Queue::Status::OP_OK) {
        return false;
    }

    channel.isValid = true;
    channel.config = config;
    return true;
}

bool VirtualChannelAccess::request(U8 vcId, const U8* data, U32 size) {
    if (vcId >= MAX_VIRTUAL_CHANNELS ||
        !m_channels[vcId].isValid ||
        size > MAX_SDU_SIZE) {
        return false;
    }

    VirtualChannel_t& channel = m_channels[vcId];

    // Send to OS queue
    Os::Queue::Status status = channel.queue.send(
        data,
        size,
        0,  // priority
        Os::Queue::BlockingType::NONBLOCKING
    );

    return (status == Os::Queue::Status::OP_OK);
}

bool VirtualChannelAccess::getNextSdu(U8& vcId, U8* data, U32& size) {
    // Check each channel in priority order
    for (U8 id = 0; id < MAX_VIRTUAL_CHANNELS; id++) {
        VirtualChannel_t& channel = m_channels[id];
        if (!channel.isValid) {
            continue;
        }

        FwSizeType actualSize;
        PlatformIntType priority;

        Os::Queue::Status status = channel.queue.receive(
            data,
            MAX_SDU_SIZE,
            Os::Queue::BlockingType::NONBLOCKING
            actualSize,
            priority
        );

        if (status == Os::Queue::Status::OP_OK) {
            vcId = id;
            size = actualSize;
            return true;
        }
    }
    return false;
}

bool VirtualChannelAccess::hasData() const {
    std::lock_guard<std::mutex> lock(m_mutex);

    return std::any_of(
        m_channels.begin(),
        m_channels.end(),
        [](const auto& channel) {
            return !channel.second.sduQueue.empty();
        }
    );
}

} // namespace TMSpaceDataLink

module;

#include <vulkan/vulkan.hpp>

module stellar.render.vulkan;

std::expected<std::pair<Device, Queue>, VkResult> Adapter::open() const {
    uint32_t queue_family_count;
    vkGetPhysicalDeviceQueueFamilyProperties(adapter, &queue_family_count, nullptr);
    std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(adapter, &queue_family_count, queue_families.data());

    std::optional<uint32_t> graphics_family_{};
    for (uint32_t i = 0; i < queue_family_count; i++) {
        if (queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            graphics_family_ = i;
            break;
        }
    }

    const uint32_t graphics_family = graphics_family_.value();
    constexpr float queue_priority = 1.0f;
    VkDeviceQueueCreateInfo queue_create_info{};
    queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_create_info.queueFamilyIndex = graphics_family;
    queue_create_info.queueCount = 1;
    queue_create_info.pQueuePriorities = &queue_priority;

    VkPhysicalDeviceFeatures device_features{};
    std::vector<const char*> device_extensions{ VK_KHR_SWAPCHAIN_EXTENSION_NAME, VK_KHR_MAINTENANCE1_EXTENSION_NAME };
    
    VkPhysicalDeviceVulkan13Features features13{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES };
    features13.synchronization2 = true;
    features13.dynamicRendering = true;

    VkPhysicalDeviceVulkan12Features features12{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES };
    features12.pNext = &features13;
    features12.descriptorIndexing = true;
    features12.descriptorBindingStorageBufferUpdateAfterBind = true;
    features12.descriptorBindingPartiallyBound = true;
    features12.descriptorBindingUpdateUnusedWhilePending = true;
    features12.runtimeDescriptorArray = true;

    VkDeviceCreateInfo device_create_info{};
    device_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_create_info.pNext = &features12;
    device_create_info.pEnabledFeatures = &device_features;
    device_create_info.enabledExtensionCount = device_extensions.size();
    device_create_info.ppEnabledExtensionNames = device_extensions.data();
    device_create_info.queueCreateInfoCount = 1;
    device_create_info.pQueueCreateInfos = &queue_create_info;

    Device device{};
    if (const auto res = vkCreateDevice(adapter, &device_create_info, nullptr, &device.device); res != VK_SUCCESS) {
        return std::unexpected(res);
    }
    device.adapter = adapter;
    device.instance = instance;
    device.initialize();

    Queue queue{};
    vkGetDeviceQueue(device.device, graphics_family, 0, &queue.queue);
    queue.family_index = graphics_family;

    return std::make_pair(std::move(device), queue);
}

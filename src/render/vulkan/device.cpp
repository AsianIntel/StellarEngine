module;

#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.hpp>

module stellar.render.vulkan;

std::expected<void, VkResult> DescriptorHeap::initialize(VkDevice device, VkDescriptorPool pool, VkDescriptorSetLayout set_layout, size_t capacity_) {
    VkDescriptorSetAllocateInfo alloc_info{ .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
    alloc_info.descriptorPool = pool;
    alloc_info.descriptorSetCount = 1;
    alloc_info.pSetLayouts = &set_layout;
    if (const auto res = vkAllocateDescriptorSets(device, &alloc_info, &set); res != VK_SUCCESS) {
        return std::unexpected(res);
    }

    capacity = capacity_;
    len = 0;

    return {};
}

size_t DescriptorHeap::allocate() {
    assert(len < capacity);
    size_t index;
    if (freelist.empty()) {
        index = len;
        len++;
    } else {
        index = freelist.front();
        freelist.pop_front();
    }

    return index;
}

void DescriptorHeap::free(const size_t index) {
    freelist.push_back(index);   
}

std::expected<void, VkResult> Device::initialize() {
    const VmaAllocatorCreateInfo allocator_create_info{
        .physicalDevice = adapter,
        .device = device,
        .instance = instance,
    };
    if (const auto res = vmaCreateAllocator(&allocator_create_info, &allocator); res != VK_SUCCESS) {
        return std::unexpected(res);
    }

    // Create bindless descriptor set layout
    VkDescriptorBindingFlags flags = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_UNUSED_WHILE_PENDING_BIT;
    VkDescriptorSetLayoutBindingFlagsCreateInfo binding_flags {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
        .bindingCount = 1,
        .pBindingFlags = &flags
    };
    VkDescriptorSetLayoutBinding buffer_binding {};
    buffer_binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    buffer_binding.descriptorCount = 1000;
    buffer_binding.stageFlags = VK_SHADER_STAGE_ALL;
    buffer_binding.binding = 0;
    VkDescriptorSetLayoutCreateInfo buffer_set_layout_create_info{ .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
    buffer_set_layout_create_info.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
    buffer_set_layout_create_info.pNext = &binding_flags;
    buffer_set_layout_create_info.bindingCount = 1;
    buffer_set_layout_create_info.pBindings = &buffer_binding;
    if (const auto res = vkCreateDescriptorSetLayout(device, &buffer_set_layout_create_info, nullptr, &buffer_set_layout); res != VK_SUCCESS) {
        return std::unexpected(res);
    }

    // Create bindless pipeline layout
    constexpr VkPushConstantRange push_constant{
        .stageFlags = VK_SHADER_STAGE_ALL,
        .offset = 0,
        .size = 128
    };
    VkPipelineLayoutCreateInfo layout_create_info { .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
    layout_create_info.setLayoutCount = 1;
    layout_create_info.pSetLayouts = &buffer_set_layout;
    layout_create_info.pushConstantRangeCount = 1;
    layout_create_info.pPushConstantRanges = &push_constant;
    if (const auto res = vkCreatePipelineLayout(device, &layout_create_info, nullptr, &bindless_pipeline_layout); res != VK_SUCCESS) {
        return std::unexpected(res);
    }

    // Create bindless descriptor pool
    constexpr VkDescriptorPoolSize buffer_pool_size {
        .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .descriptorCount = 1000
    };
    VkDescriptorPoolCreateInfo pool_create_info { .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
    pool_create_info.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
    pool_create_info.maxSets = 1;
    pool_create_info.poolSizeCount = 1;
    pool_create_info.pPoolSizes = &buffer_pool_size;
    if (const auto res = vkCreateDescriptorPool(device, &pool_create_info, nullptr, &bindless_descriptor_pool); res != VK_SUCCESS) {
        return std::unexpected(res);
    }

    buffer_heap.initialize(device, bindless_descriptor_pool, buffer_set_layout, 1000);

    return {};
}

void Device::destroy() {
    vkDestroyPipelineLayout(device, bindless_pipeline_layout, nullptr);
    vkDestroyDescriptorPool(device, bindless_descriptor_pool, nullptr);
    vkDestroyDescriptorSetLayout(device, buffer_set_layout, nullptr);
    vkDestroyDevice(device, nullptr);
    device = nullptr;
}

std::expected<void, VkResult> Device::wait_for_fence(const Fence& fence) const {
    if (const auto res = vkWaitForFences(device, 1, &fence.fence, VK_TRUE, UINT64_MAX); res != VK_SUCCESS) {
        return std::unexpected(res);
    }
    if (const auto res = vkResetFences(device, 1, &fence.fence); res != VK_SUCCESS) {
        return std::unexpected(res);
    }
    
    return {};
}

std::expected<Swapchain, VkResult> Device::create_swapchain(VkSurfaceKHR surface, uint32_t queue_family, const SurfaceConfiguration& config) const {
    Swapchain swapchain{};
    swapchain.device = device;
    
    VkSwapchainCreateInfoKHR swapchain_create_info { .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
    swapchain_create_info.surface = surface;
    swapchain_create_info.minImageCount = 3;
    swapchain_create_info.imageFormat = map_texture_format(config.format);
    swapchain_create_info.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    swapchain_create_info.imageExtent.width = config.extent.width;
    swapchain_create_info.imageExtent.height = config.extent.height;
    swapchain_create_info.imageArrayLayers = 1;
    swapchain_create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    swapchain_create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    swapchain_create_info.queueFamilyIndexCount = 1;
    swapchain_create_info.pQueueFamilyIndices = &queue_family;
    swapchain_create_info.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    swapchain_create_info.compositeAlpha = map_composite_alpha(config.composite_alpha);
    swapchain_create_info.presentMode = map_present_mode(config.present_mode);
    swapchain_create_info.clipped = true;
    if (const auto res = vkCreateSwapchainKHR(device, &swapchain_create_info, nullptr, &swapchain.swapchain); res != VK_SUCCESS) {
        return std::unexpected(res);
    }

    uint32_t image_count;
    vkGetSwapchainImagesKHR(device, swapchain.swapchain, &image_count, nullptr);
    std::vector<VkImage> swapchain_images(image_count);
    vkGetSwapchainImagesKHR(device, swapchain.swapchain, &image_count, swapchain_images.data());

    swapchain.swapchain_images.resize(image_count);
    for (uint32_t i = 0; i < image_count; i++) {
        VkImageViewCreateInfo view_info{ .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
        view_info.image = swapchain_images[i];
        view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        view_info.format = map_texture_format(config.format);
        view_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        view_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        view_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        view_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        view_info.subresourceRange.baseMipLevel = 0;
        view_info.subresourceRange.levelCount = 1;
        view_info.subresourceRange.baseArrayLayer = 0;
        view_info.subresourceRange.layerCount = 1;
        VkImageView view;
        if (const auto res = vkCreateImageView(device, &view_info, nullptr, &view); res != VK_SUCCESS) {
            return std::unexpected(res);
        }

        swapchain.swapchain_images[i] = SurfaceTexture { .texture = swapchain_images[i], .view = view, .swapchain_index = i };
    }

    return swapchain;
}

void Device::destroy_swapchain(const Swapchain& swapchain) const {
    for (const auto image: swapchain.swapchain_images) {
        vkDestroyImageView(device, image.view.view, nullptr);
    }
    vkDestroySwapchainKHR(device, swapchain.swapchain, nullptr);
}

std::expected<CommandEncoder, VkResult> Device::create_command_encoder(const CommandEncoderDescriptor& descriptor) const {
    CommandEncoder encoder{};
    encoder.device = device;

    VkCommandPoolCreateInfo create_info { .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
    create_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    create_info.queueFamilyIndex = descriptor.queue->family_index;
    if (const auto res = vkCreateCommandPool(device, &create_info, nullptr, &encoder.pool); res != VK_SUCCESS) {
        return std::unexpected(res);
    }

    return encoder;
}

void Device::destroy_command_encoder(const CommandEncoder& encoder) const {
    assert(encoder.active == VK_NULL_HANDLE);
    vkDestroyCommandPool(device, encoder.pool, nullptr);
}

std::expected<Fence, VkResult> Device::create_fence(const bool signaled) const {
    VkFenceCreateInfo create_info { .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
    create_info.flags = signaled ? VK_FENCE_CREATE_SIGNALED_BIT : 0;

    Fence fence{};
    if (const auto res = vkCreateFence(device, &create_info, nullptr, &fence.fence); res != VK_SUCCESS) {
        return std::unexpected(res);
    }

    return fence;
}

void Device::destroy_fence(const Fence& fence) const {
    vkDestroyFence(device, fence.fence, nullptr);
}

std::expected<Semaphore, VkResult> Device::create_semaphore() const {
    VkSemaphoreCreateInfo create_info { .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };

    Semaphore semaphore{};
    if (const auto res = vkCreateSemaphore(device, &create_info, nullptr, &semaphore.semaphore); res != VK_SUCCESS) {
        return std::unexpected(res);
    }

    return semaphore;
}

void Device::destroy_semaphore(const Semaphore& semaphore) const {
    vkDestroySemaphore(device, semaphore.semaphore, nullptr);   
}

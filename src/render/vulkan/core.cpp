module;

#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.hpp>
#include <vector>
#include <expected>
#include <iostream>
#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

module stellar.render.vulkan;

VkBool32 debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
                        VkDebugUtilsMessageTypeFlagsEXT message_type, const VkDebugUtilsMessengerCallbackDataEXT* data,
                        void* user_data);
VkResult create_debug_utils_messenger_ext(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
                                          const VkAllocationCallbacks* pAllocator,
                                          VkDebugUtilsMessengerEXT* pDebugMessenger);
void destroy_debug_utils_messenger_ext(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger,
                                       const VkAllocationCallbacks* pAllocator);

std::expected<void, VkResult> Instance::initialize(const InstanceDescriptor& descriptor) {
    VkApplicationInfo app_info{};
    app_info.pApplicationName = "Stellar Engine";
    app_info.applicationVersion = VK_MAKE_VERSION(0, 0, 1);
    app_info.pEngineName = "Stellar Engine";
    app_info.engineVersion = VK_MAKE_VERSION(0, 0, 1);
    app_info.apiVersion = VK_API_VERSION_1_3;

    std::vector<const char*> layers{};
    if (descriptor.validation || descriptor.gpu_based_validation) {
        layers.push_back("VK_LAYER_KHRONOS_validation");
    }

    std::vector<const char*> extensions{VK_KHR_SURFACE_EXTENSION_NAME, "VK_KHR_win32_surface"};
    if (descriptor.validation || descriptor.gpu_based_validation) {
        extensions.push_back("VK_EXT_debug_utils");
    }

    VkInstanceCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create_info.pApplicationInfo = &app_info;
    create_info.enabledExtensionCount = extensions.size();
    create_info.ppEnabledExtensionNames = extensions.data();
    create_info.enabledLayerCount = layers.size();
    create_info.ppEnabledLayerNames = layers.data();
    if (VkResult res = vkCreateInstance(&create_info, nullptr, &instance); res != VK_SUCCESS) {
        return std::unexpected(res);
    }

    if (descriptor.validation || descriptor.gpu_based_validation) {
        VkDebugUtilsMessengerCreateInfoEXT debug_create_info{};
        debug_create_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        debug_create_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        debug_create_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        debug_create_info.pfnUserCallback = debug_callback;
        if (VkResult res = create_debug_utils_messenger_ext(instance, &debug_create_info, nullptr, &debug_messenger)
            ; res != VK_SUCCESS) {
            return std::unexpected(res);
        }
    }

    return {};
}

void Instance::destroy() {
    destroy_debug_utils_messenger_ext(instance, debug_messenger, nullptr);
    vkDestroyInstance(instance, nullptr);
    debug_messenger = nullptr;
    instance = nullptr;
}

std::expected<std::vector<Adapter>, VkResult> Instance::enumerate_adapters() const {
    uint32_t count = 0;
    if (const auto res = vkEnumeratePhysicalDevices(instance, &count, nullptr); res != VK_SUCCESS) {
        return std::unexpected(res);
    }
    std::vector<VkPhysicalDevice> raw_adapters(count);
    if (const auto res = vkEnumeratePhysicalDevices(instance, &count, raw_adapters.data()); res != VK_SUCCESS) {
        return std::unexpected(res);
    }

    std::vector<Adapter> adapters;
    for (const auto raw_adapter : raw_adapters) {
        VkPhysicalDeviceProperties properties;
        vkGetPhysicalDeviceProperties(raw_adapter, &properties);

        DeviceType type;
        switch (properties.deviceType) {
        case VK_PHYSICAL_DEVICE_TYPE_CPU:
            type = DeviceType::Cpu;
            break;
        case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
            type = DeviceType::VirtualGpu;
            break;
        case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
            type = DeviceType::Gpu;
            break;
        case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
            type = DeviceType::IntegratedGpu;
            break;
        default:
            type = DeviceType::Other;
        }

        adapters.push_back(Adapter{
            .adapter = raw_adapter,
            .instance = instance,
            .info = AdapterInfo{
                .type = type
            }
        });
    }

    return adapters;
}

std::expected<Surface, VkResult> Instance::create_surface(HWND hwnd, HINSTANCE hinstance) const {
    VkWin32SurfaceCreateInfoKHR create_info{.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR};
    create_info.hwnd = hwnd;
    create_info.hinstance = hinstance;
    Surface surface{};
    if (const auto res = vkCreateWin32SurfaceKHR(instance, &create_info, nullptr, &surface.surface); res !=
        VK_SUCCESS) {
        return std::unexpected(res);
    }

    return surface;
}

void Instance::destroy_surface(const Surface& surface) const {
    vkDestroySurfaceKHR(instance, surface.surface, nullptr);
}

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
    std::vector<const char*> device_extensions{VK_KHR_SWAPCHAIN_EXTENSION_NAME, VK_KHR_MAINTENANCE1_EXTENSION_NAME};

    VkPhysicalDeviceVulkan13Features features13{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
    features13.synchronization2 = true;
    features13.dynamicRendering = true;

    VkPhysicalDeviceVulkan12Features features12{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
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

std::expected<void, VkResult> CommandEncoder::begin_encoding() {
    VkCommandBuffer buffer;
    if (free.empty()) {
        VkCommandBufferAllocateInfo alloc_info{.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        alloc_info.commandPool = pool;
        alloc_info.commandBufferCount = 1;
        if (const auto res = vkAllocateCommandBuffers(device, &alloc_info, &buffer); res != VK_SUCCESS) {
            return std::unexpected(res);
        }
    }
    else {
        buffer = free.front();
        free.pop_front();
        if (const auto res = vkResetCommandBuffer(buffer, 0); res != VK_SUCCESS) {
            return std::unexpected(res);
        }
    }

    VkCommandBufferBeginInfo begin_info{.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    if (const auto res = vkBeginCommandBuffer(buffer, &begin_info); res != VK_SUCCESS) {
        return std::unexpected(res);
    }
    vkCmdBindDescriptorSets(buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, bindless_pipeline_layout, 0, 1,
                            &bindless_buffer_set, 0, nullptr);

    active = buffer;
    return {};
}

void CommandEncoder::begin_render_pass(const RenderPassDescriptor& descriptor) const {
    std::vector<VkRenderingAttachmentInfo> color_attachments{};
    for (const auto attachment : descriptor.color_attachments) {
        VkRenderingAttachmentInfo attachment_info{.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
        attachment_info.imageView = attachment.target.view->view;
        attachment_info.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

        if ((attachment.ops & AttachmentOps::Load) == AttachmentOps::Load) {
            attachment_info.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
        }
        else {
            attachment_info.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            attachment_info.clearValue = VkClearValue{
                .color = VkClearColorValue{
                    {attachment.clear.r, attachment.clear.g, attachment.clear.b, attachment.clear.a}
                }
            };
        }
        color_attachments.push_back(attachment_info);
    }

    VkRenderingInfo rendering_info{.sType = VK_STRUCTURE_TYPE_RENDERING_INFO};
    rendering_info.colorAttachmentCount = color_attachments.size();
    rendering_info.pColorAttachments = color_attachments.data();
    rendering_info.renderArea.offset = VkOffset2D{0, 0};
    rendering_info.renderArea.extent = VkExtent2D{descriptor.extent.width, descriptor.extent.height};
    rendering_info.layerCount = 1;

    if (descriptor.depth_attachment.has_value()) {
        const DepthAttachment& attachment = descriptor.depth_attachment.value();
        VkRenderingAttachmentInfo depth_attachment{.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
        depth_attachment.imageView = attachment.target.view->view;
        depth_attachment.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

        if ((attachment.ops & AttachmentOps::Load) == AttachmentOps::Load) {
            depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
        }
        else {
            depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            depth_attachment.clearValue = VkClearValue{
                .depthStencil = VkClearDepthStencilValue{.depth = attachment.depth_clear}
            };
        }

        rendering_info.pDepthAttachment = &depth_attachment;
    }

    vkCmdBeginRendering(active, &rendering_info);

    VkViewport viewport;
    viewport.x = 0.0f;
    viewport.y = descriptor.extent.height;
    viewport.width = descriptor.extent.width;
    viewport.height = -1 * static_cast<float>(descriptor.extent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(active, 0, 1, &viewport);

    VkRect2D scissor;
    scissor.offset = {0, 0};
    scissor.extent = {descriptor.extent.width, descriptor.extent.height};
    vkCmdSetScissor(active, 0, 1, &scissor);
}

void CommandEncoder::transition_textures(const std::span<TextureBarrier>& transitions) const {
    std::vector<VkImageMemoryBarrier2> barriers{};
    for (const auto transition : transitions) {
        VkImageMemoryBarrier2 barrier{.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
        barrier.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
        barrier.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT;
        barrier.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
        barrier.dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT;
        barrier.oldLayout = map_texture_layout(transition.before);
        barrier.newLayout = map_texture_layout(transition.after);
        barrier.image = transition.texture->texture;
        barrier.subresourceRange.aspectMask = map_format_aspect(transition.range.aspect);
        barrier.subresourceRange.baseArrayLayer = transition.range.base_array_layer;
        barrier.subresourceRange.layerCount = transition.range.array_layer_count;
        barrier.subresourceRange.baseMipLevel = transition.range.base_mip_level;
        barrier.subresourceRange.levelCount = transition.range.mip_level_count;

        barriers.push_back(barrier);
    }

    VkDependencyInfo dependency_info{.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
    dependency_info.imageMemoryBarrierCount = barriers.size();
    dependency_info.pImageMemoryBarriers = barriers.data();
    vkCmdPipelineBarrier2(active, &dependency_info);
}

void CommandEncoder::bind_pipeline(const Pipeline& pipeline) const {
    vkCmdBindPipeline(active, pipeline.bind_point, pipeline.pipeline);
}

void CommandEncoder::bind_index_buffer(const Buffer& buffer) const {
    vkCmdBindIndexBuffer(active, buffer.buffer, 0, VK_INDEX_TYPE_UINT32);
}

void CommandEncoder::set_push_constants(const std::span<uint32_t>& push_constants) const {
    vkCmdPushConstants(active, bindless_pipeline_layout, VK_SHADER_STAGE_ALL, 0, 4 * push_constants.size(),
                       push_constants.data());
}

void CommandEncoder::draw(const uint32_t vertex_count, const uint32_t instance_count, const uint32_t first_vertex,
                          const uint32_t first_instance) const {
    vkCmdDraw(active, vertex_count, instance_count, first_vertex, first_instance);
}

void CommandEncoder::draw_indexed(const uint32_t index_count, const uint32_t instance_count, const uint32_t first_index,
                                  const uint32_t vertex_offset, const uint32_t first_instance) const {
    vkCmdDrawIndexed(active, index_count, instance_count, first_index, vertex_offset, first_instance);
}

void CommandEncoder::end_render_pass() const {
    vkCmdEndRendering(active);
}

std::expected<CommandBuffer, VkResult> CommandEncoder::end_encoding() {
    if (const auto res = vkEndCommandBuffer(active); res != VK_SUCCESS) {
        return std::unexpected(res);
    }
    const VkCommandBuffer buffer = active;
    active = VK_NULL_HANDLE;
    return CommandBuffer{buffer};
}

void CommandEncoder::reset_all(const std::span<CommandBuffer>& command_buffers) {
    for (const auto buffer : command_buffers) {
        free.push_back(buffer.buffer);
    }
    vkResetCommandPool(device, pool, 0);
}

std::expected<void, VkResult> DescriptorHeap::initialize(VkDevice device, VkDescriptorPool pool,
                                                         VkDescriptorSetLayout set_layout, size_t capacity_) {
    VkDescriptorSetAllocateInfo alloc_info{.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
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
    }
    else {
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

    shader_compiler.initialize().value();

    // Create bindless descriptor set layout
    VkDescriptorBindingFlags flags = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT |
        VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_UNUSED_WHILE_PENDING_BIT;
    VkDescriptorSetLayoutBindingFlagsCreateInfo binding_flags{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
        .bindingCount = 1,
        .pBindingFlags = &flags
    };
    VkDescriptorSetLayoutBinding buffer_binding{};
    buffer_binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    buffer_binding.descriptorCount = 1000;
    buffer_binding.stageFlags = VK_SHADER_STAGE_ALL;
    buffer_binding.binding = 0;
    VkDescriptorSetLayoutCreateInfo buffer_set_layout_create_info{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO
    };
    buffer_set_layout_create_info.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
    buffer_set_layout_create_info.pNext = &binding_flags;
    buffer_set_layout_create_info.bindingCount = 1;
    buffer_set_layout_create_info.pBindings = &buffer_binding;
    if (const auto res = vkCreateDescriptorSetLayout(device, &buffer_set_layout_create_info, nullptr,
                                                     &buffer_set_layout); res != VK_SUCCESS) {
        return std::unexpected(res);
    }

    // Create bindless pipeline layout
    constexpr VkPushConstantRange push_constant{
        .stageFlags = VK_SHADER_STAGE_ALL,
        .offset = 0,
        .size = 128
    };
    VkPipelineLayoutCreateInfo layout_create_info{.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    layout_create_info.setLayoutCount = 1;
    layout_create_info.pSetLayouts = &buffer_set_layout;
    layout_create_info.pushConstantRangeCount = 1;
    layout_create_info.pPushConstantRanges = &push_constant;
    if (const auto res = vkCreatePipelineLayout(device, &layout_create_info, nullptr, &bindless_pipeline_layout); res !=
        VK_SUCCESS) {
        return std::unexpected(res);
    }

    // Create bindless descriptor pool
    constexpr VkDescriptorPoolSize buffer_pool_size{
        .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .descriptorCount = 1000
    };
    VkDescriptorPoolCreateInfo pool_create_info{.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    pool_create_info.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
    pool_create_info.maxSets = 1;
    pool_create_info.poolSizeCount = 1;
    pool_create_info.pPoolSizes = &buffer_pool_size;
    if (const auto res = vkCreateDescriptorPool(device, &pool_create_info, nullptr, &bindless_descriptor_pool); res !=
        VK_SUCCESS) {
        return std::unexpected(res);
    }

    buffer_heap.initialize(device, bindless_descriptor_pool, buffer_set_layout, 1000);

    return {};
}

void Device::destroy() {
    vkDestroyPipelineLayout(device, bindless_pipeline_layout, nullptr);
    vkDestroyDescriptorPool(device, bindless_descriptor_pool, nullptr);
    vkDestroyDescriptorSetLayout(device, buffer_set_layout, nullptr);
    vmaDestroyAllocator(allocator);
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

size_t Device::add_binding(const Buffer& buffer) {
    const size_t index = buffer_heap.allocate();
    VkDescriptorBufferInfo buffer_info;
    buffer_info.buffer = buffer.buffer;
    buffer_info.offset = 0;
    buffer_info.range = VK_WHOLE_SIZE;

    VkWriteDescriptorSet set_write{.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    set_write.dstBinding = 0;
    set_write.dstSet = buffer_heap.set;
    set_write.descriptorCount = 1;
    set_write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    set_write.dstArrayElement = index;
    set_write.pBufferInfo = &buffer_info;

    vkUpdateDescriptorSets(device, 1, &set_write, 0, nullptr);
    return index;
}

void* Device::map_buffer(const Buffer& buffer) const {
    void* data;
    vmaMapMemory(allocator, buffer.allocation, &data);
    return data;
}

void Device::unmap_buffer(const Buffer& buffer) const {
    vmaUnmapMemory(allocator, buffer.allocation);
}

std::expected<Swapchain, VkResult> Device::create_swapchain(VkSurfaceKHR surface, uint32_t queue_family,
                                                            const SurfaceConfiguration& config) const {
    Swapchain swapchain{};
    swapchain.device = device;

    VkSwapchainCreateInfoKHR swapchain_create_info{.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
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
    if (const auto res = vkCreateSwapchainKHR(device, &swapchain_create_info, nullptr, &swapchain.swapchain); res !=
        VK_SUCCESS) {
        return std::unexpected(res);
    }

    uint32_t image_count;
    vkGetSwapchainImagesKHR(device, swapchain.swapchain, &image_count, nullptr);
    std::vector<VkImage> swapchain_images(image_count);
    vkGetSwapchainImagesKHR(device, swapchain.swapchain, &image_count, swapchain_images.data());

    swapchain.swapchain_images.resize(image_count);
    for (uint32_t i = 0; i < image_count; i++) {
        VkImageViewCreateInfo view_info{.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
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

        swapchain.swapchain_images[i] = SurfaceTexture{
            .texture = swapchain_images[i], .view = view, .swapchain_index = i
        };
    }

    return swapchain;
}

void Device::destroy_swapchain(const Swapchain& swapchain) const {
    for (const auto image : swapchain.swapchain_images) {
        vkDestroyImageView(device, image.view.view, nullptr);
    }
    vkDestroySwapchainKHR(device, swapchain.swapchain, nullptr);
}

std::expected<CommandEncoder, VkResult>
Device::create_command_encoder(const CommandEncoderDescriptor& descriptor) const {
    CommandEncoder encoder{};
    encoder.device = device;
    encoder.bindless_buffer_set = buffer_heap.set;
    encoder.bindless_pipeline_layout = bindless_pipeline_layout;

    VkCommandPoolCreateInfo create_info{.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
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
    VkFenceCreateInfo create_info{.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
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
    VkSemaphoreCreateInfo create_info{.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};

    Semaphore semaphore{};
    if (const auto res = vkCreateSemaphore(device, &create_info, nullptr, &semaphore.semaphore); res != VK_SUCCESS) {
        return std::unexpected(res);
    }

    return semaphore;
}

void Device::destroy_semaphore(const Semaphore& semaphore) const {
    vkDestroySemaphore(device, semaphore.semaphore, nullptr);
}

std::expected<Buffer, VkResult> Device::create_buffer(const BufferDescriptor& descriptor) const {
    VkBufferCreateInfo create_info{.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    create_info.size = descriptor.size;
    create_info.usage = map_buffer_usage(descriptor.usage);

    VmaAllocationCreateInfo alloc_info{};
    if ((descriptor.usage & BufferUsage::MapReadWrite) == BufferUsage::MapReadWrite) {
        alloc_info.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
    }
    else {
        alloc_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    }

    Buffer buffer;
    buffer.size = descriptor.size;
    if (const auto res = vmaCreateBuffer(allocator, &create_info, &alloc_info, &buffer.buffer, &buffer.allocation,
                                         nullptr); res != VK_SUCCESS) {
        return std::unexpected(res);
    }

    return buffer;
}

void Device::destroy_buffer(const Buffer& buffer) const {
    vmaDestroyBuffer(allocator, buffer.buffer, buffer.allocation);
}

std::expected<Pipeline, VkResult> Device::create_graphics_pipeline(const PipelineDescriptor& descriptor) const {
    VkPipelineShaderStageCreateInfo vertex_stage{.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
    vertex_stage.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertex_stage.module = descriptor.vertex_shader->module;
    vertex_stage.pName = descriptor.vertex_shader->entrypoint.c_str();

    VkPipelineShaderStageCreateInfo fragment_stage{.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
    fragment_stage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragment_stage.module = descriptor.fragment_shader->module;
    fragment_stage.pName = descriptor.fragment_shader->entrypoint.c_str();

    VkPipelineViewportStateCreateInfo viewport{.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
    viewport.viewportCount = 1;
    viewport.scissorCount = 1;

    VkPipelineColorBlendAttachmentState color_blend_attachment{};
    color_blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    color_blend_attachment.blendEnable = false;

    VkPipelineColorBlendStateCreateInfo color_blending{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO
    };
    color_blending.logicOpEnable = false;
    color_blending.logicOp = VK_LOGIC_OP_COPY;
    color_blending.attachmentCount = 1;
    color_blending.pAttachments = &color_blend_attachment;

    VkPipelineVertexInputStateCreateInfo vertex_input{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO
    };

    std::array dynamic_states{VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamic_state_info{.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
    dynamic_state_info.dynamicStateCount = dynamic_states.size();
    dynamic_state_info.pDynamicStates = dynamic_states.data();

    VkPipelineInputAssemblyStateCreateInfo input_assembly{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO
    };
    input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    input_assembly.primitiveRestartEnable = false;

    VkPipelineRasterizationStateCreateInfo rasterizer{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO
    };
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_NONE;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

    VkPipelineMultisampleStateCreateInfo multisampling{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO
    };
    multisampling.sampleShadingEnable = false;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisampling.minSampleShading = 1.0f;
    multisampling.alphaToCoverageEnable = false;
    multisampling.alphaToOneEnable = false;

    VkFormat color_attachment_format = map_texture_format(descriptor.render_format);
    VkPipelineRenderingCreateInfo rendering_info{.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
    rendering_info.colorAttachmentCount = 1;
    rendering_info.pColorAttachmentFormats = &color_attachment_format;

    VkPipelineDepthStencilStateCreateInfo depth_stencil{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO
    };
    if (descriptor.depth_stencil.has_value()) {
        const DepthStencilState& depth_stencil_state = descriptor.depth_stencil.value();
        depth_stencil.depthTestEnable = true;
        depth_stencil.depthWriteEnable = depth_stencil_state.depth_write_enabled;
        depth_stencil.depthCompareOp = map_compare_function(depth_stencil_state.compare);

        rendering_info.depthAttachmentFormat = map_texture_format(depth_stencil_state.format);
    }
    else {
        depth_stencil.depthTestEnable = false;
        depth_stencil.depthWriteEnable = false;
        depth_stencil.depthCompareOp = VK_COMPARE_OP_NEVER;
    }
    depth_stencil.depthBoundsTestEnable = false;
    depth_stencil.stencilTestEnable = false;
    depth_stencil.minDepthBounds = 0.0f;
    depth_stencil.maxDepthBounds = 1.0f;

    std::array shader_stages{vertex_stage, fragment_stage};
    VkGraphicsPipelineCreateInfo create_info{.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
    create_info.pNext = &rendering_info;
    create_info.stageCount = shader_stages.size();
    create_info.pStages = shader_stages.data();
    create_info.pVertexInputState = &vertex_input;
    create_info.pInputAssemblyState = &input_assembly;
    create_info.pViewportState = &viewport;
    create_info.pRasterizationState = &rasterizer;
    create_info.pMultisampleState = &multisampling;
    create_info.pColorBlendState = &color_blending;
    create_info.pDepthStencilState = &depth_stencil;
    create_info.pDynamicState = &dynamic_state_info;
    create_info.layout = bindless_pipeline_layout;

    Pipeline pipeline{};
    pipeline.bind_point = VK_PIPELINE_BIND_POINT_GRAPHICS;
    if (const auto res = vkCreateGraphicsPipelines(device, nullptr, 1, &create_info, nullptr, &pipeline.pipeline); res
        != VK_SUCCESS) {
        return std::unexpected(res);
    }

    return pipeline;
}

void Device::destroy_pipeline(const Pipeline& pipeline) const {
    vkDestroyPipeline(device, pipeline.pipeline, nullptr);
}

std::expected<ShaderModule, VkResult> Device::create_shader_module(const ShaderModuleDescriptor& descriptor) const {
    std::string target;
    switch (descriptor.stage) {
    case ShaderStage::Vertex:
        target = "vs_6_5";
        break;
    case ShaderStage::Fragment:
        target = "ps_6_5";
        break;
    case ShaderStage::Compute:
        target = "cs_6_5";
    }
    auto shader_code = shader_compiler.compile(descriptor.code, descriptor.entrypoint, target);

    VkShaderModuleCreateInfo create_info{.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    create_info.codeSize = shader_code.size();
    create_info.pCode = reinterpret_cast<uint32_t*>(shader_code.data());

    ShaderModule module;
    module.entrypoint = descriptor.entrypoint;
    if (const auto res = vkCreateShaderModule(device, &create_info, nullptr, &module.module); res != VK_SUCCESS) {
        return std::unexpected(res);
    }

    return module;
}

void Device::destroy_shader_module(const ShaderModule& module) const {
    vkDestroyShaderModule(device, module.module, nullptr);
}

std::expected<Texture, VkResult> Device::create_texture(const TextureDescriptor& descriptor) const {
    VkImageCreateInfo create_info{.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    create_info.imageType = map_image_type(descriptor.dimension);
    create_info.format = map_texture_format(descriptor.format);
    create_info.extent.width = descriptor.size.width;
    create_info.extent.height = descriptor.size.height;
    create_info.extent.depth = 1;
    create_info.mipLevels = descriptor.mip_level_count;
    create_info.arrayLayers = 1;
    create_info.samples = static_cast<VkSampleCountFlagBits>(descriptor.sample_count);
    create_info.tiling = VK_IMAGE_TILING_OPTIMAL;
    create_info.usage = map_texture_usage(descriptor.usage);
    create_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VmaAllocationCreateInfo alloc_info{
        .usage = VMA_MEMORY_USAGE_GPU_ONLY
    };

    Texture texture{};
    texture.format = descriptor.format;
    if (const auto res = vmaCreateImage(allocator, &create_info, &alloc_info, &texture.texture, &texture.allocation,
                                        nullptr); res != VK_SUCCESS) {
        return std::unexpected(res);
    }

    return texture;
}

void Device::destroy_texture(const Texture& texture) const {
    vmaDestroyImage(allocator, texture.texture, texture.allocation);
}

std::expected<TextureView, VkResult> Device::create_texture_view(const Texture& texture,
                                                                 const TextureViewDescriptor& descriptor) const {
    VkImageViewCreateInfo create_info{.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    create_info.image = texture.texture;
    create_info.viewType = map_image_view_type(descriptor.dimension);
    create_info.format = map_texture_format(texture.format);
    create_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    create_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    create_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    create_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    create_info.subresourceRange.aspectMask = map_format_aspect(descriptor.range.aspect);
    create_info.subresourceRange.baseMipLevel = descriptor.range.base_mip_level;
    create_info.subresourceRange.levelCount = descriptor.range.mip_level_count;
    create_info.subresourceRange.baseArrayLayer = descriptor.range.base_array_layer;
    create_info.subresourceRange.layerCount = descriptor.range.array_layer_count;

    TextureView view;
    if (const auto res = vkCreateImageView(device, &create_info, nullptr, &view.view); res != VK_SUCCESS) {
        return std::unexpected(res);
    }

    return view;
}

void Device::destroy_texture_view(const TextureView& view) const {
    vkDestroyImageView(device, view.view, nullptr);
}

VkResult create_debug_utils_messenger_ext(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
                                          const VkAllocationCallbacks* pAllocator,
                                          VkDebugUtilsMessengerEXT* pDebugMessenger) {
    if (const auto func = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(
        instance, "vkCreateDebugUtilsMessengerEXT")); func != nullptr) {
        return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
    }
    else {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
}

void destroy_debug_utils_messenger_ext(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger,
                                       const VkAllocationCallbacks* pAllocator) {
    if (const auto func = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(
        instance, "vkDestroyDebugUtilsMessengerEXT")); func != nullptr) {
        func(instance, debugMessenger, pAllocator);
    }
}

VkBool32 debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
                        VkDebugUtilsMessageTypeFlagsEXT message_type, const VkDebugUtilsMessengerCallbackDataEXT* data,
                        void* user_data) {
    std::cout << data->pMessage << "\n";
    return VK_FALSE;
}

module;

#include <vulkan/vulkan.hpp>

module stellar.render.vulkan;

std::expected<void, VkResult> CommandEncoder::begin_encoding() {
    VkCommandBuffer buffer;
    if (free.empty()) {
        VkCommandBufferAllocateInfo alloc_info { .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
        alloc_info.commandPool = pool;
        alloc_info.commandBufferCount = 1;
        if (const auto res = vkAllocateCommandBuffers(device, &alloc_info, &buffer); res != VK_SUCCESS) {
            return std::unexpected(res);
        }
    } else {
        buffer = free.front();
        free.pop_front();
        if (const auto res = vkResetCommandBuffer(buffer, 0); res != VK_SUCCESS) {
            return std::unexpected(res);
        }
    }

    VkCommandBufferBeginInfo begin_info { .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    if (const auto res = vkBeginCommandBuffer(buffer, &begin_info); res != VK_SUCCESS) {
        return std::unexpected(res);
    }
    vkCmdBindDescriptorSets(buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, bindless_pipeline_layout, 0, 1, &bindless_buffer_set, 0, nullptr);

    active = buffer;
    return {};
}

void CommandEncoder::begin_render_pass(const RenderPassDescriptor& descriptor) const {
    std::vector<VkRenderingAttachmentInfo> color_attachments{};
    for (const auto attachment: descriptor.color_attachments) {
        VkRenderingAttachmentInfo attachment_info { .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };
        attachment_info.imageView = attachment.target.view->view;
        attachment_info.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

        if ((attachment.ops & AttachmentOps::Load) == AttachmentOps::Load) {
            attachment_info.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
        } else {
            attachment_info.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            attachment_info.clearValue = VkClearValue { .color = VkClearColorValue { { attachment.clear.r, attachment.clear.g, attachment.clear.b, attachment.clear.a } } };
        }
        color_attachments.push_back(attachment_info);
    }

    VkRenderingInfo rendering_info { .sType = VK_STRUCTURE_TYPE_RENDERING_INFO };
    rendering_info.colorAttachmentCount = color_attachments.size();
    rendering_info.pColorAttachments = color_attachments.data();
    rendering_info.renderArea.offset = VkOffset2D { 0, 0 };
    rendering_info.renderArea.extent = VkExtent2D { descriptor.extent.width, descriptor.extent.height };
    rendering_info.layerCount = 1;

    if (descriptor.depth_attachment.has_value()) {
        const DepthAttachment& attachment = descriptor.depth_attachment.value();
        VkRenderingAttachmentInfo depth_attachment { .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };
        depth_attachment.imageView = attachment.target.view->view;
        depth_attachment.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

        if ((attachment.ops & AttachmentOps::Load) == AttachmentOps::Load) {
            depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
        } else {
            depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            depth_attachment.clearValue = VkClearValue { .depthStencil = VkClearDepthStencilValue { .depth = attachment.depth_clear } };
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
    scissor.extent = { descriptor.extent.width, descriptor.extent.height };
    vkCmdSetScissor(active, 0, 1, &scissor);
}

void CommandEncoder::transition_textures(const std::span<TextureBarrier>& transitions) const {
    std::vector<VkImageMemoryBarrier2> barriers{};
    for (const auto transition: transitions) {
        VkImageMemoryBarrier2 barrier{ .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2 };
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

    VkDependencyInfo dependency_info { .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
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
    vkCmdPushConstants(active, bindless_pipeline_layout, VK_SHADER_STAGE_ALL, 0, 4 * push_constants.size(), push_constants.data());   
}

void CommandEncoder::draw(const uint32_t vertex_count, const uint32_t instance_count, const uint32_t first_vertex, const uint32_t first_instance) const {
    vkCmdDraw(active, vertex_count, instance_count, first_vertex, first_instance);   
}

void CommandEncoder::draw_indexed(const uint32_t index_count, const uint32_t instance_count, const uint32_t first_index, const uint32_t vertex_offset, const uint32_t first_instance) const {
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
    return CommandBuffer { buffer };
}

void CommandEncoder::reset_all(const std::span<CommandBuffer>& command_buffers) {
    for (const auto buffer: command_buffers) {
        free.push_back(buffer.buffer);
    }
    vkResetCommandPool(device, pool, 0);
}

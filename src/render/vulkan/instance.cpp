module;

#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.hpp>
#include <vector>
#include <expected>
#include <iostream>

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
    for (const auto raw_adapter: raw_adapters) {
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

        adapters.push_back(Adapter {
            .adapter = raw_adapter,
            .instance = instance,
            .info = AdapterInfo {
                .type = type
            }
        });
    }

    return adapters;
}

std::expected<Surface, VkResult> Instance::create_surface(HWND hwnd, HINSTANCE hinstance) const {
    VkWin32SurfaceCreateInfoKHR create_info{ .sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR };
    create_info.hwnd = hwnd;
    create_info.hinstance = hinstance;
    Surface surface{};
    if (const auto res = vkCreateWin32SurfaceKHR(instance, &create_info, nullptr, &surface.surface); res != VK_SUCCESS) {
        return std::unexpected(res);
    }

    return surface;
}

void Instance::destroy_surface(const Surface& surface) const {
    vkDestroySurfaceKHR(instance, surface.surface, nullptr);   
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

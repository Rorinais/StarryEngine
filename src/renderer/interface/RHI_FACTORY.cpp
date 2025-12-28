#include "RHI_INTERFACE.hpp"
#include "RHI_FACTORY.hpp"
//#include "RHI_VULKAN.hpp"    // Vulkan实现头文件
//#include "RHI_DX12.hpp"      // DirectX12实现头文件
//#include "RHI_DX11.hpp"      // DirectX11实现头文件
//#include "RHI_METAL.hpp"     // Metal实现头文件
//#include "RHI_OPENGL.hpp"    // OpenGL实现头文件
#include <array>

namespace StarryEngine::RHI {

    // ==================== RHIFactory 实现 ====================

    std::vector<API> RHIFactory::getAvailableAPIs() {
        std::vector<API> availableAPIs;

        // 根据平台和编译条件检测可用API
#ifdef _WIN32
        // Windows平台
        availableAPIs.push_back(API::DirectX12);
        availableAPIs.push_back(API::DirectX11);
        availableAPIs.push_back(API::Vulkan);
        availableAPIs.push_back(API::OpenGL);

#elif defined(__APPLE__)
        // macOS/iOS平台
        availableAPIs.push_back(API::Metal);
        availableAPIs.push_back(API::Vulkan);  // 通过MoltenVK
        availableAPIs.push_back(API::OpenGL);

#elif defined(__ANDROID__) || defined(ANDROID)
        // Android平台
        availableAPIs.push_back(API::Vulkan);
        availableAPIs.push_back(API::OpenGLES);

#elif defined(__linux__)
        // Linux平台
        availableAPIs.push_back(API::Vulkan);
        availableAPIs.push_back(API::OpenGL);

#else
        // 其他平台
        availableAPIs.push_back(API::OpenGL);
#endif

        // 移除不支持或未启用的API
        std::vector<API> supportedAPIs;
        for (API api : availableAPIs) {
            if (isAPISupported(api)) {
                supportedAPIs.push_back(api);
            }
        }

        return supportedAPIs;
    }

    std::string RHIFactory::getAPIName(API api) {
        static const std::array<const char*, 6> apiNames = {
            "Vulkan",
            "DirectX 12",
            "DirectX 11",
            "Metal",
            "OpenGL",
            "OpenGL ES"
        };

        size_t index = static_cast<size_t>(api);
        if (index < apiNames.size()) {
            return apiNames[index];
        }
        return "Unknown";
    }

    Version RHIFactory::getAPIVersion(API api) {
        // 这里应该查询实际API版本，这里返回默认值
        switch (api) {
        case API::Vulkan:
            return { 1, 3, 0 };
        case API::DirectX12:
            return { 12, 0, 0 };
        case API::DirectX11:
            return { 11, 0, 0 };
        case API::Metal:
            return { 3, 0, 0 };
        case API::OpenGL:
            return { 4, 6, 0 };
        case API::OpenGLES:
            return { 3, 2, 0 };
        default:
            return { 1, 0, 0 };
        }
    }

    bool RHIFactory::isAPISupported(API api) {
        // 这里应该进行实际的API支持检测
        // 简化实现，总是返回true
        return true;
    }

    // 私有工厂方法 - 实际创建具体实现
    std::unique_ptr<IRHIContext> RHIFactory::createVulkanContext() {
        try {
#ifdef RHI_VULKAN_ENABLED
            // 实际创建Vulkan上下文
            // return std::make_unique<VulkanContext>();
#endif
            return nullptr;
        }
        catch (...) {
            return nullptr;
        }
    }

    std::unique_ptr<IRHIContext> RHIFactory::createDX12Context() {
        try {
#ifdef RHI_DX12_ENABLED
            // 实际创建DirectX12上下文
            // return std::make_unique<DX12Context>();
#endif
            return nullptr;
        }
        catch (...) {
            return nullptr;
        }
    }

    std::unique_ptr<IRHIContext> RHIFactory::createDX11Context() {
        try {
#ifdef RHI_DX11_ENABLED
            // 实际创建DirectX11上下文
            // return std::make_unique<DX11Context>();
#endif
            return nullptr;
        }
        catch (...) {
            return nullptr;
        }
    }

    std::unique_ptr<IRHIContext> RHIFactory::createMetalContext() {
        try {
#ifdef RHI_METAL_ENABLED
            // 实际创建Metal上下文
            // return std::make_unique<MetalContext>();
#endif
            return nullptr;
        }
        catch (...) {
            return nullptr;
        }
    }

    std::unique_ptr<IRHIContext> RHIFactory::createGLContext() {
        try {
#ifdef RHI_OPENGL_ENABLED
            // 实际创建OpenGL上下文
            // return std::make_unique<GLContext>();
#endif
            return nullptr;
        }
        catch (...) {
            return nullptr;
        }
    }

    std::unique_ptr<IRHIContext> RHIFactory::createGLESContext() {
        try {
#ifdef RHI_OPENGLES_ENABLED
            // 实际创建OpenGLES上下文
            // return std::make_unique<GLESContext>();
#endif
            return nullptr;
        }
        catch (...) {
            return nullptr;
        }
    }

} // namespace StarryEngine::RHI
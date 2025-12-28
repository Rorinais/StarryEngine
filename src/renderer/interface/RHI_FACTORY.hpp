#include "RHI_INTERFACE.hpp"
#include <cmath>
#include <algorithm>
#include <random>
#include <cstring>
#include <array>
#include <fstream>
#include <sstream>

namespace StarryEngine::RHI {
    class RHIFactory {
    public:
        static std::unique_ptr<IRHIContext> createContext(API api) {
            switch (api) {
            case API::Vulkan:
                return createVulkanContext();
            case API::DirectX12:
                return createDX12Context();
            case API::DirectX11:
                return createDX11Context();
            case API::Metal:
                return createMetalContext();
            case API::OpenGL:
                return createGLContext();
            case API::OpenGLES:
                return createGLESContext();
            default:
                return nullptr;
            }
        }

        static std::unique_ptr<IRHIContext> createContextFromConfig(const RHIInitConfig& config) {
            auto context = createContext(config.api);
            if (context && context->initialize(config)) {
                return context;
            }
            return nullptr;
        }

        static std::vector<API> getAvailableAPIs();
        static std::string getAPIName(API api);
        static Version getAPIVersion(API api);
        static bool isAPISupported(API api);

    private:
        static std::unique_ptr<IRHIContext> createVulkanContext();
        static std::unique_ptr<IRHIContext> createDX12Context();
        static std::unique_ptr<IRHIContext> createDX11Context();
        static std::unique_ptr<IRHIContext> createMetalContext();
        static std::unique_ptr<IRHIContext> createGLContext();
        static std::unique_ptr<IRHIContext> createGLESContext();
    };
}
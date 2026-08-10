#include <assets/loader/ShaderLoader.hpp>

#include <application/Application.hpp>
#include "type.hpp"

using namespace StarryEngine;

int main() {
#ifdef __linux__
    char exePath[PATH_MAX];
    ssize_t count = readlink("/proc/self/exe", exePath, sizeof(exePath) - 1);
    if (count != -1) {
        exePath[count] = '\0';
        char* lastSlash = strrchr(exePath, '/');
        if (lastSlash) {
            *lastSlash = '\0';
            std::string layerPath = std::string(exePath) + "/layers";
            setenv("VK_LAYER_PATH", layerPath.c_str(), 1);
            std::string libPath = std::string(exePath);
            std::string currentLdPath = getenv("LD_LIBRARY_PATH") ? getenv("LD_LIBRARY_PATH") : "";
            setenv("LD_LIBRARY_PATH", (libPath + ":" + currentLdPath).c_str(), 1);
        }
    }
#elif _WIN32
    _putenv_s("VK_LAYER_PATH", "layers");
#endif
    StarryEngine::Logger::init();
    StarryEngine::Logger::setShowSourceLoc(true);

    StarryEngine::Application app;

	Assets::ShaderLoader shaderLoader(app.getResourceManager());

    auto vertexShader = shaderLoader.loadFromFile("assets/shaders/pbr/shpere_pbr.frag", RHI::ShaderStage::Fragment);
    if (!vertexShader) {
        LOG_ERROR("Failed to load vertex shader");
        return -1;
    }
    LOG_INFO(vertexShader.value().reflection);

    StarryEngine::Logger::shutdown();
}



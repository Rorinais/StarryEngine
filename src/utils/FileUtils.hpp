#pragma once
#include<iostream>
#include<string>
#include <sstream>
#include<fstream>
#include<vector>
#include <shaderc/shaderc.hpp>

namespace StarryEngine::Utils {

    class FileUtils {
    public:
        static std::string readTextFile(const std::string& path);
        static std::vector<uint32_t> readBinaryFile(const std::string& path);
        static bool fileExists(const std::string& filepath);
        static std::vector<uint32_t> compileGlslToSpirv(
            const std::string& source,
            const std::string& name,
            shaderc_shader_kind kind);
    };

} // namespace StarryEngine::Utils
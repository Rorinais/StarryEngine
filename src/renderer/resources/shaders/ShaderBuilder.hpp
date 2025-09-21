#pragma once
#include <string>
#include <vector>
#include <sstream>
#include <map>
#include "../../core/VulkanCore/VulkanCore.hpp"
#include "../../resources/shaders/shaderUtils.hpp"
#include "../../resources/shaders/ShaderProgram.hpp"
namespace StarryEngine {
    class ShaderBuilder {
    public:
        using Ptr = std::shared_ptr<ShaderBuilder>;
        static Ptr create(ShaderType type, const std::string& version = "#version 450") {
            return std::make_shared<ShaderBuilder>(type, version);
        }

        ShaderBuilder(ShaderType type, const std::string& version = "#version 450");
        ~ShaderBuilder() {
        }

        void addInput(const std::string& type, const std::string& name, int location = -1);

        void addOutput(const std::string& type, const std::string& name, int location = -1);

        void addUniformBuffer(const std::string& name, int binding, const std::vector<std::string>& members);

        void addSampler(const std::string& type, const std::string& name, int binding);

        void addUniformBuffer(
            const std::string& name,
            int set,
            int binding,
            const std::vector<std::string>& members,
            const std::string& layoutQualifiers = ""
        );

        void addSampler(
            const std::string& type,
            const std::string& name,
            int set,
            int binding,
            const std::string& layoutQualifiers = ""
        );

        void setMainBody(const std::string& body);

        void addCustomCode(const std::string& code);

        void addStruct(const std::string& name, const std::vector<std::string>& members);

        void addFunction(const std::string& signature, const std::string& body);

        std::string getSource() const;

        ShaderType getType() const { return m_type; }

    private:
        ShaderType m_type;
        std::string m_version;
        std::ostringstream m_inputs;
        std::ostringstream m_outputs;
        std::ostringstream m_uniforms;
        std::ostringstream m_structs;
        std::ostringstream m_functions;
        std::ostringstream m_extensions;
        std::ostringstream m_customCode;
        std::string m_mainBody;
    };
}

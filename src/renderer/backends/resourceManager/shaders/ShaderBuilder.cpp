#include "ShaderBuilder.hpp"
namespace StarryEngine {
    ShaderBuilder::ShaderBuilder(ShaderType type, const std::string& version) : m_type(type), m_version(version) {}

    void ShaderBuilder::addInput(const std::string& type, const std::string& name, int location) {
        if (location >= 0) {
            m_inputs << "layout(location = " << location << ") in " << type << " " << name << ";\n";
        }
        else {
            m_inputs << "in " << type << " " << name << ";\n";
        }
    }

    void ShaderBuilder::addOutput(const std::string& type, const std::string& name, int location) {
        if (location >= 0) {
            m_outputs << "layout(location = " << location << ") out " << type << " " << name << ";\n";
        }
        else {
            m_outputs << "out " << type << " " << name << ";\n";
        }
    }

    void ShaderBuilder::addUniformBuffer(const std::string& name, int binding, const std::vector<std::string>& members) {
        m_uniforms << "layout(binding = " << binding << ") uniform " << name << " {\n";
        for (const auto& member : members) {
            m_uniforms << "    " << member << ";\n";
        }
        m_uniforms << "};\n\n";
    }

    void ShaderBuilder::addSampler(const std::string& type, const std::string& name, int binding) {
        m_uniforms << "layout(binding = " << binding << ") uniform " << type << " " << name << ";\n";
    }

    void ShaderBuilder::addUniformBuffer(
        const std::string& name,
        int set,
        int binding,
        const std::vector<std::string>& members,
        const std::string& layoutQualifiers
    ) {
        if (!layoutQualifiers.empty()) {
            m_uniforms << "layout(" << layoutQualifiers << ", set=" << set << ", binding=" << binding << ") ";
        }
        else {
            m_uniforms << "layout(set=" << set << ", binding=" << binding << ") ";
        }
        m_uniforms << "uniform " << name << " {\n";
        for (const auto& member : members) {
            m_uniforms << "    " << member << ";\n";
        }
        m_uniforms << "} ubo;\n\n";
    }

    void ShaderBuilder::addSampler(
        const std::string& type,
        const std::string& name,
        int set,
        int binding,
        const std::string& layoutQualifiers
    ) {
        if (!layoutQualifiers.empty()) {
            m_uniforms << "layout(" << layoutQualifiers << ", set=" << set << ", binding=" << binding << ") ";
        }
        else {
            m_uniforms << "layout(set=" << set << ", binding=" << binding << ") ";
        }
        m_uniforms << "uniform " << type << " " << name << ";\n";
    }

    void ShaderBuilder::setMainBody(const std::string& body) {
        m_mainBody = body;
    }

    void ShaderBuilder::addCustomCode(const std::string& code) {
        m_customCode << code << "\n";
    }

    void ShaderBuilder::addStruct(const std::string& name, const std::vector<std::string>& members) {
        m_structs << "struct " << name << " {\n";
        for (const auto& member : members) {
            m_structs << "    " << member << ";\n";
        }
        m_structs << "};\n\n";
    }

    void ShaderBuilder::addFunction(const std::string& signature, const std::string& body) {
        m_functions << signature << " {\n" << body << "\n}\n\n";
    }

    std::string ShaderBuilder::getSource() const {
        std::ostringstream oss;

        oss << m_version << "\n";

        oss << m_extensions.str();
        oss << m_customCode.str() << "\n";

        oss << m_structs.str();

        if (!m_uniforms.str().empty()) {
            oss << "// Uniforms\n" << m_uniforms.str();
        }

        oss << m_functions.str();

        if (m_type == ShaderType::Vertex) {
            oss << "// Vertex Inputs\n" << m_inputs.str() << "\n";
            oss << "// Vertex Outputs\n" << m_outputs.str() << "\n";
        }
        else if (m_type == ShaderType::Fragment) {
            oss << "// Fragment Inputs\n" << m_inputs.str() << "\n";
            oss << "// Fragment Outputs\n" << m_outputs.str() << "\n";
        }

        oss << "void main() {\n";
        oss << m_mainBody;
        oss << "}\n";

        return oss.str();
    }
}

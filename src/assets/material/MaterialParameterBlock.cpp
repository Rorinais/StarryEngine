#include"MaterialParameterBlock.hpp"

namespace StarryEngine::Assets {

    MaterialParameterBlock::MaterialParameterBlock(const RHI::ResourceBinding& binding) {
        uint32_t totalSize = 0;
        for (const auto& m : binding.members) {
            totalSize = std::max(totalSize, m.offset + m.size);
            m_members[m.name] = { m.offset, m.size };
        }
        m_data.resize(totalSize, 0);
        m_dirty = true;  
    }

    void MaterialParameterBlock::setFloat(const std::string& name, float value) {
        auto it = m_members.find(name);
        if (it == m_members.end()) { return; }   
        std::memcpy(m_data.data() + it->second.offset, &value, sizeof(float));
        m_dirty = true;
    }

    void MaterialParameterBlock::setInt(const std::string& name, int32_t value) {
        auto it = m_members.find(name);
        if (it == m_members.end()) { return; }
        std::memcpy(m_data.data() + it->second.offset, &value, sizeof(int32_t));
        m_dirty = true;
    }

    void MaterialParameterBlock::setUint(const std::string& name, uint32_t value) {
        auto it = m_members.find(name);
        if (it == m_members.end()) { return; }
        std::memcpy(m_data.data() + it->second.offset, &value, sizeof(uint32_t));
        m_dirty = true;
    }

    void MaterialParameterBlock::setVec2(const std::string& name, const glm::vec2& v) {
        auto it = m_members.find(name);
        if (it == m_members.end()) { return; }
        std::memcpy(m_data.data() + it->second.offset, glm::value_ptr(v), sizeof(glm::vec2));
        m_dirty = true;
    }

    void MaterialParameterBlock::setVec3(const std::string& name, const glm::vec3& v) {
        auto it = m_members.find(name);
        if (it == m_members.end()) { return; }
        std::memcpy(m_data.data() + it->second.offset, glm::value_ptr(v), sizeof(glm::vec3));
        m_dirty = true;
    }

    void MaterialParameterBlock::setVec4(const std::string& name, const glm::vec4& v) {
        auto it = m_members.find(name);
        if (it == m_members.end()) { return; }
        std::memcpy(m_data.data() + it->second.offset, glm::value_ptr(v), sizeof(glm::vec4));
        m_dirty = true;
    }

    void MaterialParameterBlock::setMat4(const std::string& name, const glm::mat4& m) {
        auto it = m_members.find(name);
        if (it == m_members.end()) { return; }
        std::memcpy(m_data.data() + it->second.offset, glm::value_ptr(m), sizeof(glm::mat4));
        m_dirty = true;
    }

    void MaterialParameterBlock::writeRaw(uint32_t offset, const void* data, size_t size) {
        if (offset + size > m_data.size()) { return; }
        std::memcpy(m_data.data() + offset, data, size);
        m_dirty = true;
    }

} // namespace StarryEngine::Assets
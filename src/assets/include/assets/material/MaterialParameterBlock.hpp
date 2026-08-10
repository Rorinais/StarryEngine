#pragma once
#include <iostream>
#include <vector>
#include <string>
#include <cstring>
#include <unordered_map>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <assets/AssetType.hpp>      

namespace StarryEngine::Assets {

    class MaterialParameterBlock {
    public:
        MaterialParameterBlock() = default;
        MaterialParameterBlock(const RHI::ResourceBinding& binding);

        void setFloat(const std::string& name, float value);

        void setInt(const std::string& name, int32_t value);

        void setUint(const std::string& name, uint32_t value);

        void setVec2(const std::string& name, const glm::vec2& v);

        void setVec3(const std::string& name, const glm::vec3& v);

        void setVec4(const std::string& name, const glm::vec4& v);

        void setMat4(const std::string& name, const glm::mat4& m);

        void writeRaw(uint32_t offset, const void* data, size_t size);

        const uint8_t* data() const { return m_data.data(); }
        uint8_t* data() { return m_data.data(); }
        size_t size() const { return m_data.size(); }

        void markDirty() { m_dirty = true; }
        bool isDirty() const { return m_dirty; }
        void clearDirty() { m_dirty = false; }

        MaterialParameterBlock* getBlock(const std::string& blockName);

    private:
        void buildReflectionCache();

    private:
        std::unordered_map<std::string, RHI::ResourceBinding> m_blockLayouts;

        struct MemberInfo {
            uint32_t offset;
            uint32_t size;    
        };
        std::unordered_map<std::string, MemberInfo> m_members;
        std::vector<uint8_t> m_data;
        bool m_dirty = false;
    };

} // namespace StarryEngine::Assets
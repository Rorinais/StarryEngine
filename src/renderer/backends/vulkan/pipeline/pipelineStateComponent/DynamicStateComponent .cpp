#include "DynamicStateComponent.hpp"

namespace StarryEngine {

    DynamicStateComponent::DynamicStateComponent(const std::string& name) {
        setName(name);
        reset();  // 使用reset初始化默认值
    }

    DynamicStateComponent& DynamicStateComponent::reset() {
        // 设置Vulkan默认的动态状态（无动态状态）
        mCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .dynamicStateCount = 0,
            .pDynamicStates = nullptr
        };

        mDynamicStates.clear();
        return *this;
    }

    DynamicStateComponent& DynamicStateComponent::addDynamicState(VkDynamicState state) {
        // 避免重复添加
        for (const auto& existing : mDynamicStates) {
            if (existing == state) {
                return *this;  // 已存在，直接返回
            }
        }

        mDynamicStates.push_back(state);
        updateCreateInfo();
        return *this;
    }

    DynamicStateComponent& DynamicStateComponent::addDynamicStates(const std::vector<VkDynamicState>& states) {
        for (const auto& state : states) {
            addDynamicState(state);
        }
        return *this;
    }

    DynamicStateComponent& DynamicStateComponent::removeDynamicState(VkDynamicState state) {
        auto it = std::remove(mDynamicStates.begin(), mDynamicStates.end(), state);
        if (it != mDynamicStates.end()) {
            mDynamicStates.erase(it, mDynamicStates.end());
            updateCreateInfo();
        }
        return *this;
    }

    DynamicStateComponent& DynamicStateComponent::clearDynamicStates() {
        mDynamicStates.clear();
        updateCreateInfo();
        return *this;
    }

    bool DynamicStateComponent::hasDynamicState(VkDynamicState state) const {
        return std::find(mDynamicStates.begin(), mDynamicStates.end(), state) != mDynamicStates.end();
    }

    DynamicStateComponent& DynamicStateComponent::setDynamicStates(const std::vector<VkDynamicState>& states) {
        mDynamicStates = states;
        updateCreateInfo();
        return *this;
    }

    DynamicStateComponent& DynamicStateComponent::addViewportScissorStates() {
        addDynamicState(VK_DYNAMIC_STATE_VIEWPORT);
        addDynamicState(VK_DYNAMIC_STATE_SCISSOR);
        return *this;
    }

    DynamicStateComponent& DynamicStateComponent::addLineWidthState() {
        addDynamicState(VK_DYNAMIC_STATE_LINE_WIDTH);
        return *this;
    }

    DynamicStateComponent& DynamicStateComponent::addDepthStencilStates() {
        addDynamicState(VK_DYNAMIC_STATE_DEPTH_BIAS);
        addDynamicState(VK_DYNAMIC_STATE_DEPTH_BOUNDS);
        addDynamicState(VK_DYNAMIC_STATE_STENCIL_COMPARE_MASK);
        addDynamicState(VK_DYNAMIC_STATE_STENCIL_WRITE_MASK);
        addDynamicState(VK_DYNAMIC_STATE_STENCIL_REFERENCE);
        return *this;
    }

    DynamicStateComponent& DynamicStateComponent::addColorBlendStates() {
        addDynamicState(VK_DYNAMIC_STATE_BLEND_CONSTANTS);
        return *this;
    }

    DynamicStateComponent& DynamicStateComponent::addVertexInputState() {
        // 注意：VK_DYNAMIC_STATE_VERTEX_INPUT_BINDING_STRIDE_EXT 需要扩展
        // 这里我们使用条件编译或运行时检查
#ifdef VK_EXT_extended_dynamic_state
        addDynamicState(VK_DYNAMIC_STATE_VERTEX_INPUT_BINDING_STRIDE_EXT);
#endif
        return *this;
    }

    std::string DynamicStateComponent::getDescription() const {
        if (mDynamicStates.empty()) {
            return "Dynamic States: NONE";
        }

        std::string desc = "Dynamic States: ";

        for (size_t i = 0; i < mDynamicStates.size(); ++i) {
            if (i > 0) desc += ", ";

            switch (mDynamicStates[i]) {
            case VK_DYNAMIC_STATE_VIEWPORT: desc += "VIEWPORT"; break;
            case VK_DYNAMIC_STATE_SCISSOR: desc += "SCISSOR"; break;
            case VK_DYNAMIC_STATE_LINE_WIDTH: desc += "LINE_WIDTH"; break;
            case VK_DYNAMIC_STATE_DEPTH_BIAS: desc += "DEPTH_BIAS"; break;
            case VK_DYNAMIC_STATE_BLEND_CONSTANTS: desc += "BLEND_CONSTANTS"; break;
            case VK_DYNAMIC_STATE_DEPTH_BOUNDS: desc += "DEPTH_BOUNDS"; break;
            case VK_DYNAMIC_STATE_STENCIL_COMPARE_MASK: desc += "STENCIL_COMPARE_MASK"; break;
            case VK_DYNAMIC_STATE_STENCIL_WRITE_MASK: desc += "STENCIL_WRITE_MASK"; break;
            case VK_DYNAMIC_STATE_STENCIL_REFERENCE: desc += "STENCIL_REFERENCE"; break;
#ifdef VK_EXT_extended_dynamic_state
            case VK_DYNAMIC_STATE_VERTEX_INPUT_BINDING_STRIDE_EXT: desc += "VERTEX_INPUT_BINDING_STRIDE"; break;
#endif
            default: desc += "UNKNOWN(" + std::to_string(mDynamicStates[i]) + ")"; break;
            }
        }

        return desc;
    }

    bool DynamicStateComponent::isValid() const {
        // 检查基础结构类型
        if (mCreateInfo.sType != VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO) {
            return false;
        }

        // 检查动态状态数量与指针的一致性
        if (mDynamicStates.empty()) {
            if (mCreateInfo.dynamicStateCount != 0 || mCreateInfo.pDynamicStates != nullptr) {
                return false;
            }
        }
        else {
            if (mCreateInfo.dynamicStateCount != mDynamicStates.size() ||
                mCreateInfo.pDynamicStates != mDynamicStates.data()) {
                return false;
            }
        }

        // 检查动态状态是否有效（没有重复）
        std::set<VkDynamicState> uniqueStates;
        for (const auto& state : mDynamicStates) {
            if (!uniqueStates.insert(state).second) {
                return false;  // 发现重复的动态状态
            }
        }

        // 检查动态状态值是否在有效范围内
        // 这里我们只检查一些常见的动态状态，不完整的列表
        for (const auto& state : mDynamicStates) {
            switch (state) {
            case VK_DYNAMIC_STATE_VIEWPORT:
            case VK_DYNAMIC_STATE_SCISSOR:
            case VK_DYNAMIC_STATE_LINE_WIDTH:
            case VK_DYNAMIC_STATE_DEPTH_BIAS:
            case VK_DYNAMIC_STATE_BLEND_CONSTANTS:
            case VK_DYNAMIC_STATE_DEPTH_BOUNDS:
            case VK_DYNAMIC_STATE_STENCIL_COMPARE_MASK:
            case VK_DYNAMIC_STATE_STENCIL_WRITE_MASK:
            case VK_DYNAMIC_STATE_STENCIL_REFERENCE:
                // 有效的标准动态状态
                break;
            default:
#ifdef VK_EXT_extended_dynamic_state
                if (state == VK_DYNAMIC_STATE_VERTEX_INPUT_BINDING_STRIDE_EXT) {
                    break;  // 有效的扩展动态状态
                }
#endif
                return false;  // 无效的动态状态
            }
        }

        return true;
    }

    void DynamicStateComponent::updateCreateInfo() {
        // 确保结构体类型正确
        mCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        mCreateInfo.pNext = nullptr;

        // 更新动态状态计数和指针
        mCreateInfo.dynamicStateCount = static_cast<uint32_t>(mDynamicStates.size());
        mCreateInfo.pDynamicStates = mDynamicStates.empty() ? nullptr : mDynamicStates.data();
    }

} // namespace StarryEngine
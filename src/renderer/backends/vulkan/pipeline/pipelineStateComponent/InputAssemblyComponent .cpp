#include "InputAssemblyComponent.hpp"

namespace StarryEngine {

    InputAssemblyComponent::InputAssemblyComponent(const std::string& name) {
        setName(name);
        reset();  // 使用reset初始化默认值
    }

    InputAssemblyComponent& InputAssemblyComponent::reset() {
        // 设置Vulkan默认的输入装配状态
        mCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,  // 默认三角形列表
            .primitiveRestartEnable = VK_FALSE                // 默认禁用图元重启
        };
        return *this;
    }

    InputAssemblyComponent& InputAssemblyComponent::setTopology(VkPrimitiveTopology topology) {
        mCreateInfo.topology = topology;
        return *this;
    }

    InputAssemblyComponent& InputAssemblyComponent::enablePrimitiveRestart(VkBool32 enable) {
        mCreateInfo.primitiveRestartEnable = enable;
        return *this;
    }

    InputAssemblyComponent& InputAssemblyComponent::setTriangleList() {
        mCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        mCreateInfo.primitiveRestartEnable = VK_FALSE;
        return *this;
    }

    InputAssemblyComponent& InputAssemblyComponent::setTriangleStrip() {
        mCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
        mCreateInfo.primitiveRestartEnable = VK_TRUE;  // 通常在使用三角形带时启用图元重启
        return *this;
    }

    InputAssemblyComponent& InputAssemblyComponent::setLineList() {
        mCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
        mCreateInfo.primitiveRestartEnable = VK_FALSE;
        return *this;
    }

    InputAssemblyComponent& InputAssemblyComponent::setLineStrip() {
        mCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
        mCreateInfo.primitiveRestartEnable = VK_TRUE;  // 通常在使用线带时启用图元重启
        return *this;
    }

    InputAssemblyComponent& InputAssemblyComponent::setPointList() {
        mCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
        mCreateInfo.primitiveRestartEnable = VK_FALSE;
        return *this;
    }

    std::string InputAssemblyComponent::getDescription() const {
        std::string desc = "Input Assembly State: ";

        // 拓扑类型
        desc += "Topology=";
        switch (mCreateInfo.topology) {
        case VK_PRIMITIVE_TOPOLOGY_POINT_LIST: desc += "POINT_LIST"; break;
        case VK_PRIMITIVE_TOPOLOGY_LINE_LIST: desc += "LINE_LIST"; break;
        case VK_PRIMITIVE_TOPOLOGY_LINE_STRIP: desc += "LINE_STRIP"; break;
        case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST: desc += "TRIANGLE_LIST"; break;
        case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP: desc += "TRIANGLE_STRIP"; break;
        case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN: desc += "TRIANGLE_FAN"; break;
        case VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY: desc += "LINE_LIST_WITH_ADJACENCY"; break;
        case VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY: desc += "LINE_STRIP_WITH_ADJACENCY"; break;
        case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY: desc += "TRIANGLE_LIST_WITH_ADJACENCY"; break;
        case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY: desc += "TRIANGLE_STRIP_WITH_ADJACENCY"; break;
        case VK_PRIMITIVE_TOPOLOGY_PATCH_LIST: desc += "PATCH_LIST"; break;
        default: desc += "UNKNOWN"; break;
        }

        // 图元重启
        if (mCreateInfo.primitiveRestartEnable) {
            desc += ", PrimitiveRestart=ENABLED";
        }

        return desc;
    }

    bool InputAssemblyComponent::isValid() const {
        // 检查基础结构类型
        if (mCreateInfo.sType != VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO) {
            return false;
        }

        // 检查拓扑类型是否有效
        switch (mCreateInfo.topology) {
        case VK_PRIMITIVE_TOPOLOGY_POINT_LIST:
        case VK_PRIMITIVE_TOPOLOGY_LINE_LIST:
        case VK_PRIMITIVE_TOPOLOGY_LINE_STRIP:
        case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST:
        case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP:
        case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN:
        case VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY:
        case VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY:
        case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY:
        case VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY:
        case VK_PRIMITIVE_TOPOLOGY_PATCH_LIST:
            break;  // 有效拓扑
        default:
            return false;  // 无效拓扑
        }

        // 某些拓扑类型需要特定的图元重启设置
        // 例如，对于带状的拓扑，图元重启通常应该启用
        VkPrimitiveTopology stripTopologies[] = {
            VK_PRIMITIVE_TOPOLOGY_LINE_STRIP,
            VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
            VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY,
            VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY
        };

        bool isStrip = false;
        for (auto topology : stripTopologies) {
            if (mCreateInfo.topology == topology) {
                isStrip = true;
                break;
            }
        }

        // 如果是带状拓扑，图元重启应该启用，但这不是强制要求
        // 我们只是记录，不强制返回false
        if (isStrip && !mCreateInfo.primitiveRestartEnable) {
            // 可以记录警告，但仍然是有效的
        }

        return true;
    }

} // namespace StarryEngine
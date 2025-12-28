#pragma once
#include "RHI_ENUMS.hpp"
#include <string>

namespace StarryEngine::RHI {

    // ==================== 光线追踪结构体 ====================

    /**
     * @brief 加速结构描述结构体
     * @details 描述光线追踪加速结构的配置
     */
    struct AccelerationStructureDesc {
        GeometryType geometryType = GeometryType::Triangles; ///< 几何类型
        uint64_t size = 0;                         ///< 大小
        BuildAccelerationStructureMode buildMode = BuildAccelerationStructureMode::Build; ///< 构建模式
        bool allowUpdate = false;                  ///< 允许更新
        bool allowCompaction = false;              ///< 允许压缩
        bool preferFastTrace = true;               ///< 优先快速追踪
        bool preferFastBuild = false;              ///< 优先快速构建
        std::string debugName;                     ///< 调试名称

        bool operator==(const AccelerationStructureDesc& other) const {
            return geometryType == other.geometryType && size == other.size &&
                buildMode == other.buildMode && allowUpdate == other.allowUpdate &&
                allowCompaction == other.allowCompaction &&
                preferFastTrace == other.preferFastTrace &&
                preferFastBuild == other.preferFastBuild;
        }

        bool operator!=(const AccelerationStructureDesc& other) const {
            return !(*this == other);
        }
    };

} // namespace StarryEngine::RHI
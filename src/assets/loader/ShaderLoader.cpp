#include "../../logging/Logger.hpp"
#include "../../utils/FileUtils.hpp"
#include "ShaderLoader.hpp"


namespace StarryEngine::Assets {

    ShaderLoader::ShaderLoader(std::shared_ptr<RHI::ResourceManager> resMgr)
        : m_resMgr(std::move(resMgr)) {
    }

    std::optional<ShaderCreateInfo> ShaderLoader::loadFromFile(const std::string& path, RHI::ShaderStage stage) {
        auto content = Utils::FileUtils::readTextFile(path);
        if (content.empty()) {
            LOG_ERROR("Failed to read shader file: {}", path);
            return std::nullopt;
        }
        return loadFromSource(content, stage, path);
    }

    std::optional<ShaderCreateInfo> ShaderLoader::loadFromSource(const std::string& source,
        RHI::ShaderStage stage,
        const std::string& name) {
        // 1. 编译
        auto spirv = compileToSpirv(source, stage, name);
        if (spirv.empty()) {
            return std::nullopt;
        }

        // 2. 准备创建信息
        ShaderCreateInfo info;
        info.spirv = spirv; // 可选保留

        // 3. 反射并创建描述符集布局
        if (!reflectAndCreateLayouts(spirv, info)) {
            // 反射失败但仍可继续（仅无布局）
            LOG_WARN("Reflection failed for shader: {}", name);
        }

        // 4. 创建模块句柄
        RHI::ShaderModuleDesc desc;
        desc.code = spirv;
        desc.debugName = name.empty() ? "shader" : name;
        desc.stage = stage;

        info.module = m_resMgr->createShader(desc);
        if (!info.module.isValid()) {
            LOG_ERROR("Failed to create shader module from SPIR-V");
            return std::nullopt;
        }

        return info;
    }

    std::vector<uint32_t> ShaderLoader::compileToSpirv(const std::string& source,
        RHI::ShaderStage stage,
        const std::string& name)
    {
        shaderc_shader_kind kind;
        switch (stage) {
        case RHI::ShaderStage::Vertex:   kind = shaderc_vertex_shader; break;
        case RHI::ShaderStage::Fragment: kind = shaderc_fragment_shader; break;
        case RHI::ShaderStage::Compute:  kind = shaderc_compute_shader; break;
        default:
            LOG_ERROR("Unsupported shader stage: {}", static_cast<int>(stage));
            return {};
        }

        try {
            return Utils::FileUtils::compileGlslToSpirv(source, name, kind);
        }
        catch (const std::exception& e) {
            LOG_ERROR("Compilation error: {}", e.what());
            return {};
        }
    }

    bool ShaderLoader::reflectAndCreateLayouts(const std::vector<uint32_t>& spirv,
        ShaderCreateInfo& outInfo) {
        try {
            spirv_cross::CompilerGLSL compiler(spirv);
            auto resources = compiler.get_shader_resources();

            // 收集每个 set 中绑定的资源，用于创建设置布局
            std::unordered_map<uint32_t, std::vector<RHI::DescriptorSetLayoutBinding>> setBindings;

            // 处理 uniform 缓冲区
            for (auto& res : resources.uniform_buffers) {
                uint32_t set = compiler.get_decoration(res.id, spv::DecorationDescriptorSet);
                uint32_t binding = compiler.get_decoration(res.id, spv::DecorationBinding);
                RHI::DescriptorSetLayoutBinding b;
                b.binding = binding;
                b.type = RHI::DescriptorType::UniformBuffer;
                b.stageFlags = static_cast<RHI::ShaderStage>(getShaderStageFromSpirv(compiler)); // 需实现
                b.count = 1;
                setBindings[set].push_back(b);
            }

            // 处理采样器/图像
            for (auto& res : resources.sampled_images) {
                uint32_t set = compiler.get_decoration(res.id, spv::DecorationDescriptorSet);
                uint32_t binding = compiler.get_decoration(res.id, spv::DecorationBinding);
                RHI::DescriptorSetLayoutBinding b;
                b.binding = binding;
                b.type = RHI::DescriptorType::CombinedImageSampler;
                b.stageFlags = static_cast<RHI::ShaderStage>(getShaderStageFromSpirv(compiler));
                b.count = 1;
                setBindings[set].push_back(b);
            }

            // 类似处理其他资源（存储缓冲、单独采样器等）

            // 为每个 set 创建布局句柄
            for (auto& [setIndex, bindings] : setBindings) {
                RHI::DescriptorSetLayoutDesc desc;
                desc.bindings = bindings;
                desc.debugName = "Set" + std::to_string(setIndex);
                auto layout = m_resMgr->createDescriptorSetLayout(desc);
                if (layout.isValid()) {
                    outInfo.setLayouts[setIndex] = layout;
                }
                else {
                    LOG_ERROR("Failed to create descriptor set layout for set {}", setIndex);
                }
            }

            // 处理顶点输入（如果是顶点着色器）
            if (!resources.stage_inputs.empty()) {
                for (auto& res : resources.stage_inputs) {
                    uint32_t location = compiler.get_decoration(res.id, spv::DecorationLocation);
                    auto type = compiler.get_type(res.type_id);
                    RHI::VertexAttribute attr;
                    attr.location = location;
                    attr.binding = 0; // 假设默认绑定0，实际应根据布局决定
                    attr.format = spirvTypeToFormat(type); // 需要实现映射
                    attr.offset = 0; // 此处仅示例，真实偏移需从顶点布局计算
                    outInfo.vertexAttributes.push_back(attr);
                }
            }

            return true;
        }
        catch (const std::exception& e) {
            LOG_ERROR("Reflection exception: {}", e.what());
            return false;
        }
    }

    RHI::ShaderStage ShaderLoader::getShaderStageFromSpirv(const spirv_cross::Compiler& compiler) const {
        auto model = compiler.get_execution_model();
        switch (model) {
        case spv::ExecutionModelVertex:
            return RHI::ShaderStage::Vertex;
        case spv::ExecutionModelFragment:
            return RHI::ShaderStage::Fragment;
        case spv::ExecutionModelGLCompute:
            return RHI::ShaderStage::Compute;
        case spv::ExecutionModelGeometry:
            return RHI::ShaderStage::Geometry;
        case spv::ExecutionModelTessellationControl:
            return RHI::ShaderStage::TessellationControl;
        case spv::ExecutionModelTessellationEvaluation:
            return RHI::ShaderStage::TessellationEvaluation;
            // 可选：Task/Mesh 扩展（如果你使用它们）
        case spv::ExecutionModelTaskNV:
            return RHI::ShaderStage::Amplification;  // 假设你使用 Amplification 对应 Task
        case spv::ExecutionModelMeshNV:
            return RHI::ShaderStage::Mesh;
            // 光线追踪阶段
        case spv::ExecutionModelRayGenerationKHR:
            return RHI::ShaderStage::RayGen;
        case spv::ExecutionModelIntersectionKHR:
            return RHI::ShaderStage::Intersection;
        case spv::ExecutionModelAnyHitKHR:
            return RHI::ShaderStage::AnyHit;
        case spv::ExecutionModelClosestHitKHR:
            return RHI::ShaderStage::ClosestHit;
        case spv::ExecutionModelMissKHR:
            return RHI::ShaderStage::Miss;
        case spv::ExecutionModelCallableKHR:
            return RHI::ShaderStage::Callable;
        default:
            throw std::runtime_error("Unsupported SPIR-V execution model: " + std::to_string(model));
        }
    }

    RHI::Format ShaderLoader::spirvTypeToFormat(const spirv_cross::SPIRType& type) const {
        // 处理矩阵：如果 columns > 1，可能需要特殊处理（例如视为向量数组）
        // 这里简化：对矩阵，我们返回其向量元素的格式（假设矩阵由列向量构成）
        uint32_t vecsize = type.vecsize;
        uint32_t columns = type.columns;
        auto basetype = type.basetype;

        // 对于矩阵，我们仍按向量大小映射，但实际顶点输入中矩阵通常展开为多个属性
        // 此函数主要用于顶点输入，因此只处理向量/标量类型
        if (columns > 1) {
            // 可根据需要记录警告或按列处理，这里先按向量处理
            // 你可以根据具体布局决定如何处理
        }

        switch (basetype) {
        case spirv_cross::SPIRType::Float:
            switch (vecsize) {
            case 1: return RHI::Format::R32_Float;
            case 2: return RHI::Format::RG32_Float;
            case 3: return RHI::Format::RGB32_Float;
            case 4: return RHI::Format::RGBA32_Float;
            default: break;
            }
            break;
        case spirv_cross::SPIRType::Int:
            switch (vecsize) {
            case 1: return RHI::Format::R32_SInt;
            case 2: return RHI::Format::RG32_SInt;
            case 3: return RHI::Format::RGB32_SInt;
            case 4: return RHI::Format::RGBA32_SInt;
            default: break;
            }
            break;
        case spirv_cross::SPIRType::UInt:
            switch (vecsize) {
            case 1: return RHI::Format::R32_UInt;
            case 2: return RHI::Format::RG32_UInt;
            case 3: return RHI::Format::RGB32_UInt;
            case 4: return RHI::Format::RGBA32_UInt;
            default: break;
            }
            break;
        case spirv_cross::SPIRType::Half:      // 16位浮点
            switch (vecsize) {
            case 1: return RHI::Format::R16_Float;
            case 2: return RHI::Format::RG16_Float;
            case 3: // R16G16B16_Float? 你的枚举中可能没有，可映射为 RGB16_Float 或自定义
            case 4: return RHI::Format::RGBA16_Float;
            default: break;
            }
            break;
        case spirv_cross::SPIRType::Double:
            switch (vecsize) {
            case 1: // R64_Float 需要你的枚举支持
            case 2:
            case 3:
            case 4: // 可能需要对应的双精度格式，若没有则 fallback
            default: break;
            }
            break;
            // 可根据需要添加其他类型（如 UShort, Short, Byte 等）
        default:
            break;
        }

        // 若未匹配，返回 Undefined 或抛出异常
        throw std::runtime_error("Unsupported SPIR-V type for vertex attribute");
    }

} // namespace StarryEngine::Assets
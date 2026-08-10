#include <logging/Logger.hpp>
#include <utils/FileUtils.hpp>
#include <utils/Hash.hpp>
#include <assets/loader/ShaderLoader.hpp>
#include <spirv_cross/spirv.hpp>   

namespace StarryEngine::Assets {

    ShaderLoader::ShaderLoader(std::shared_ptr<RHI::ResourceManager> resMgr): m_resMgr(std::move(resMgr)) {}

    std::optional<ShaderCreateInfo> ShaderLoader::loadFromFile(const std::string& path,RHI::ShaderStage stage,const std::unordered_map<std::string, std::string>& macros){
        auto content = Utils::FileUtils::readTextFile(path);
        if (content.empty()) {
            LOG_ERROR("Failed to read shader file: {}", path);
            return std::nullopt;
        }
        return loadFromSource(content, stage, path, macros);
    }

    std::optional<ShaderCreateInfo> ShaderLoader::loadFromSource(const std::string& source,RHI::ShaderStage stage,const std::string& name,const std::unordered_map<std::string, std::string>& macros){

        std::string combined = source;
        for (const auto& [k, v] : macros) {
            combined += k + "=" + v + ";";
        }
        size_t hash = computeHash(combined, stage);

        {
            std::lock_guard<std::mutex> lock(m_cacheMutex);
            auto it = m_cache.find(hash);
            if (it != m_cache.end()) {
                return it->second;
            }
        }

        auto spirv = compileToSpirv(source, stage, name, macros);
        if (spirv.empty()) {
            return std::nullopt;
        }

        ShaderCreateInfo info;
        info.spirv = spirv;
        if (!reflectAndCreateLayouts(spirv, info)) {
            LOG_WARN("Reflection failed for shader: {}", name);
        }

        RHI::ShaderModuleDesc desc;
        desc.code = spirv;
        desc.debugName = name.empty() ? "shader" : name;
        desc.stage = stage;
        info.module = m_resMgr->createShader(desc);
        if (!info.module.isValid()) {
            LOG_ERROR("Failed to create shader module from SPIR-V");
            return std::nullopt;
        }

        {
            std::lock_guard<std::mutex> lock(m_cacheMutex);
            m_cache[hash] = info;
        }
        return info;
    }

    std::vector<uint32_t> ShaderLoader::compileToSpirv(const std::string& source,RHI::ShaderStage stage,const std::string& name,const std::unordered_map<std::string, std::string>& macros){
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
            return Utils::FileUtils::compileGlslToSpirv(source, name, kind, macros);
        }
        catch (const std::exception& e) {
            LOG_ERROR("Compilation error: {}", e.what());
            return {};
        }
    }

    bool ShaderLoader::reflectAndCreateLayouts(const std::vector<uint32_t>& spirv,ShaderCreateInfo& outInfo){
        try {
            auto compiler = std::make_unique<spirv_cross::CompilerGLSL>(spirv);
            auto resources = std::move(compiler->get_shader_resources());

            RHI::ShaderReflectionInfo& refl = outInfo.reflection;
            refl.shaderStage = getShaderStageFromSpirv(*compiler);

            auto setBindings = std::make_unique<std::unordered_map<uint32_t, std::vector<RHI::DescriptorSetLayoutBinding>>>();

            auto processBuffer = [&](const spirv_cross::Resource& res,
                RHI::DescriptorType descType,
                RHI::ResourceBinding& resBind) {
                    uint32_t set = compiler->get_decoration(res.id, spv::DecorationDescriptorSet);
                    uint32_t binding = compiler->get_decoration(res.id, spv::DecorationBinding);
                    RHI::ShaderStage stage = getShaderStageFromSpirv(*compiler);

                    resBind.name = res.name;
                    resBind.set = set;
                    resBind.binding = binding;
                    resBind.type = descType;
                    resBind.count = 1;
                    resBind.stageFlags = static_cast<RHI::ShaderStageFlags>(stage);

                    RHI::DescriptorSetLayoutBinding lb;
                    lb.binding = binding;
                    lb.type = descType;
                    lb.stageFlags = stage;
                    lb.count = 1;
                    (*setBindings)[set].push_back(lb);

                    if (descType == RHI::DescriptorType::UniformBuffer ||
                        descType == RHI::DescriptorType::StorageBuffer) {
                        const spirv_cross::SPIRType& type = compiler->get_type(res.base_type_id);
                        if (type.basetype == spirv_cross::SPIRType::Struct) {
                            flattenUBOMembers(*compiler, type, 0, "", resBind.members,descType == RHI::DescriptorType::StorageBuffer);
                        }
                    }else if (descType == RHI::DescriptorType::CombinedImageSampler ||
                        descType == RHI::DescriptorType::SampledImage ||
                        descType == RHI::DescriptorType::StorageImage) {
                        const spirv_cross::SPIRType& type = compiler->get_type(res.type_id);
                        RHI::ResourceBinding::TextureInfo texInfo;
                        fillTextureInfo(type, texInfo);
                        resBind.texture = texInfo;
                    }
                };

            // Uniform Buffers
            for (auto& res : resources.uniform_buffers) {
                RHI::ResourceBinding binding;
                processBuffer(res, RHI::DescriptorType::UniformBuffer, binding);
                refl.resourceBindings.push_back(std::move(binding));
            }

            // Storage Buffers
            for (auto& res : resources.storage_buffers) {
                RHI::ResourceBinding binding;
                processBuffer(res, RHI::DescriptorType::StorageBuffer, binding);
                refl.resourceBindings.push_back(std::move(binding));
            }

            // Sampled Images
            for (auto& res : resources.sampled_images) {
                RHI::ResourceBinding binding;
                processBuffer(res, RHI::DescriptorType::CombinedImageSampler, binding);
                refl.resourceBindings.push_back(std::move(binding));
            }

            // Separate Images
            for (auto& res : resources.separate_images) {
                RHI::ResourceBinding binding;
                processBuffer(res, RHI::DescriptorType::SampledImage, binding);
                refl.resourceBindings.push_back(std::move(binding));
            }

            // Separate Samplers
            for (auto& res : resources.separate_samplers) {
                uint32_t set = compiler->get_decoration(res.id, spv::DecorationDescriptorSet);
                uint32_t binding = compiler->get_decoration(res.id, spv::DecorationBinding);
                RHI::ShaderStage stage = getShaderStageFromSpirv(*compiler);

                RHI::DescriptorSetLayoutBinding lb;
                lb.binding = binding;
                lb.type = RHI::DescriptorType::Sampler;
                lb.stageFlags = stage;
                lb.count = 1;
                (*setBindings)[set].push_back(lb);

                RHI::ResourceBinding resBind;
                resBind.name = res.name;
                resBind.set = set;
                resBind.binding = binding;
                resBind.type = RHI::DescriptorType::Sampler;
                resBind.count = 1;
                resBind.stageFlags = static_cast<RHI::ShaderStageFlags>(stage);
                refl.resourceBindings.push_back(resBind);
            }

            // Storage Images
            for (auto& res : resources.storage_images) {
                RHI::ResourceBinding binding;
                processBuffer(res, RHI::DescriptorType::StorageImage, binding);
                refl.resourceBindings.push_back(std::move(binding));
            }

            // Subpass Inputs
            for (auto& res : resources.subpass_inputs) {
                uint32_t set = compiler->get_decoration(res.id, spv::DecorationDescriptorSet);
                uint32_t binding = compiler->get_decoration(res.id, spv::DecorationBinding);
                RHI::ShaderStage stage = getShaderStageFromSpirv(*compiler);

                RHI::DescriptorSetLayoutBinding lb;
                lb.binding = binding;
                lb.type = RHI::DescriptorType::InputAttachment;
                lb.stageFlags = stage;
                lb.count = 1;
                (*setBindings)[set].push_back(lb);

                RHI::ResourceBinding resBind;
                resBind.name = res.name;
                resBind.set = set;
                resBind.binding = binding;
                resBind.type = RHI::DescriptorType::InputAttachment;
                resBind.count = 1;
                resBind.stageFlags = static_cast<RHI::ShaderStageFlags>(stage);
                refl.resourceBindings.push_back(resBind);
            }

            for (auto& [setIndex, bindings] : (*setBindings)) {
                RHI::DescriptorSetLayoutDesc desc;
                desc.bindings = bindings;
                desc.debugName = "Set" + std::to_string(setIndex);
                outInfo.layoutDescs[setIndex] = std::move(desc);
            }

            // 顶点输入属性
            for (auto& res : resources.stage_inputs) {
                uint32_t loc = compiler->get_decoration(res.id, spv::DecorationLocation);
                spirv_cross::SPIRType type = compiler->get_type(res.type_id);
                RHI::Format fmt = spirvTypeToFormat(type);

                RHI::VertexAttribute attr;
                attr.location = loc;
                attr.binding = 0;
                attr.format = fmt;
                attr.offset = 0;
                outInfo.vertexAttributes.push_back(attr);

                RHI::InputAttribute input;
                input.name = res.name;
                input.location = loc;
                input.format = fmt;
                refl.inputAttributes.push_back(input);
            }

            // 输出属性
            for (auto& res : resources.stage_outputs) {
                uint32_t loc = compiler->get_decoration(res.id, spv::DecorationLocation);
                spirv_cross::SPIRType type = compiler->get_type(res.type_id);
                RHI::Format fmt = spirvTypeToFormat(type);
                RHI::OutputAttribute output;
                output.name = res.name;
                output.location = loc;
                output.format = fmt;
                refl.outputAttributes.push_back(output);
            }

            // Push Constants
            for (auto& res : resources.push_constant_buffers) {
                const spirv_cross::SPIRType& pcType = compiler->get_type(res.base_type_id);
                RHI::PushConstant pc;
                pc.name = res.name;
                pc.offset = 0;
                pc.size = compiler->get_declared_struct_size(pcType);
                pc.stageFlags = static_cast<RHI::ShaderStageFlags>(getShaderStageFromSpirv(*compiler));

                if (pcType.basetype == spirv_cross::SPIRType::Struct) {
                    for (uint32_t i = 0; i < pcType.member_types.size(); ++i) {
                        RHI::BufferMember member;
                        member.name = compiler->get_member_name(pcType.self, i);
                        member.offset = compiler->type_struct_member_offset(pcType, i);
                        member.size = compiler->get_declared_struct_member_size(pcType, i);
                        const spirv_cross::SPIRType& memType = compiler->get_type(pcType.member_types[i]);
                        member.format = spirvTypeToFormat(memType);
                        pc.members.push_back(member);
                    }
                }
                refl.pushConstants.push_back(pc);
            }

            // Specialization Constants
            auto specConstants = compiler->get_specialization_constants();
            for (auto& sc : specConstants) {
                RHI::SpecConstant spec;
                spec.name = compiler->get_name(sc.id);
                spec.constantId = sc.constant_id;

                const spirv_cross::SPIRConstant& constant = compiler->get_constant(sc.id);
                spirv_cross::TypeID typeId = constant.constant_type;
                const spirv_cross::SPIRType& type = compiler->get_type(typeId);
                spec.size = getTypeSize(type);

                refl.specConstants.push_back(spec);
            }

            // Compute 工作组大小
            if (compiler->get_execution_model() == spv::ExecutionModelGLCompute) {
                refl.workGroupSizeX = compiler->get_execution_mode_argument(spv::ExecutionModeLocalSize, 0);
                refl.workGroupSizeY = compiler->get_execution_mode_argument(spv::ExecutionModeLocalSize, 1);
                refl.workGroupSizeZ = compiler->get_execution_mode_argument(spv::ExecutionModeLocalSize, 2);
            }

            return true;
        }
        catch (const std::exception& e) {
            LOG_ERROR("Reflection exception: {}", e.what());
            return false;
        }
    }

    void ShaderLoader::flattenUBOMembers(
        const spirv_cross::CompilerGLSL& compiler,
        const spirv_cross::SPIRType& type,
        uint32_t baseOffset,
        const std::string& baseName,
        std::vector<RHI::BufferMember>& flatMembers,
        bool isStorageBuffer){

        for (uint32_t i = 0; i < type.member_types.size(); ++i) {
            std::string memberName = compiler.get_member_name(type.self, i);
            if (memberName.empty()) memberName = "_" + std::to_string(i);
            uint32_t offset = baseOffset + compiler.type_struct_member_offset(type, i);
            const spirv_cross::SPIRType& memberType = compiler.get_type(type.member_types[i]);

            // 1. 优先处理数组
            if (!memberType.array.empty()) {
                uint32_t arraySize = memberType.array[0];
                if (arraySize == 0) {
                    // SSBO 的运行时数组（如 particles[]）是正常用法，反射无法确定大小，
                    // 跳过即可（SSBO 整体绑定，不需要逐 member 布局）。
                    // 只有 UBO 出现无界数组才是真正的错误（UBO 不允许运行时数组）。
                    if (!isStorageBuffer) {
                        LOG_WARN("Uniform buffer member '{}' has array size 0 (unsupported in UBO)", memberName);
                    }
                    continue;
                }
                uint32_t stride = compiler.get_declared_struct_member_size(type, i) / arraySize;

                for (uint32_t j = 0; j < arraySize; ++j) {
                    std::string elemName = baseName + memberName + "[" + std::to_string(j) + "]";
                    const spirv_cross::SPIRType& elemType = memberType;

                    if (elemType.basetype == spirv_cross::SPIRType::Struct) {
                        // 元素仍是结构体，递归展开
                        flattenUBOMembers(compiler, elemType, offset + j * stride,
                            elemName + ".", flatMembers, isStorageBuffer);
                    }
                    else {
                        // 元素是标量/向量/矩阵，直接添加叶子
                        RHI::BufferMember member;
                        member.name = elemName;
                        member.offset = offset + j * stride;
                        member.size = stride;
                        member.format = spirvTypeToFormat(elemType);
                        flatMembers.push_back(member);
                    }
                }
                continue;
            }

            // 2. 非数组结构体，递归展开
            if (memberType.basetype == spirv_cross::SPIRType::Struct) {
                flattenUBOMembers(compiler, memberType, offset,
                    baseName + memberName + ".", flatMembers, isStorageBuffer);
                continue;
            }

            // 3. 普通叶子成员
            RHI::BufferMember member;
            member.name = baseName + memberName;
            member.offset = offset;
            member.size = compiler.get_declared_struct_member_size(type, i);
            member.format = spirvTypeToFormat(memberType);
            flatMembers.push_back(member);
        }
    }


    RHI::ShaderStage ShaderLoader::getShaderStageFromSpirv(const spirv_cross::Compiler& compiler) const {
        auto model = compiler.get_execution_model();
        switch (model) {
        case spv::ExecutionModelVertex:                     return RHI::ShaderStage::Vertex;
        case spv::ExecutionModelFragment:                   return RHI::ShaderStage::Fragment;
        case spv::ExecutionModelGLCompute:                  return RHI::ShaderStage::Compute;
        case spv::ExecutionModelGeometry:                   return RHI::ShaderStage::Geometry;
        case spv::ExecutionModelTessellationControl:         return RHI::ShaderStage::TessellationControl;
        case spv::ExecutionModelTessellationEvaluation:      return RHI::ShaderStage::TessellationEvaluation;
        case spv::ExecutionModelTaskNV:                      return RHI::ShaderStage::Amplification;
        case spv::ExecutionModelMeshNV:                      return RHI::ShaderStage::Mesh;
        case spv::ExecutionModelRayGenerationKHR:            return RHI::ShaderStage::RayGen;
        case spv::ExecutionModelIntersectionKHR:             return RHI::ShaderStage::Intersection;
        case spv::ExecutionModelAnyHitKHR:                   return RHI::ShaderStage::AnyHit;
        case spv::ExecutionModelClosestHitKHR:               return RHI::ShaderStage::ClosestHit;
        case spv::ExecutionModelMissKHR:                    return RHI::ShaderStage::Miss;
        case spv::ExecutionModelCallableKHR:                return RHI::ShaderStage::Callable;
        default:
            throw std::runtime_error("Unsupported SPIR-V execution model: " + std::to_string(model));
        }
    }

    RHI::Format ShaderLoader::spirvTypeToFormat(const spirv_cross::SPIRType& type) const {
        if (type.columns > 1) {
            spirv_cross::SPIRType vecType = type;
            vecType.columns = 1;
            return spirvTypeToFormat(vecType);
        }
        switch (type.basetype) {
        case spirv_cross::SPIRType::Float:
            switch (type.vecsize) {
            case 1: return RHI::Format::R32_Float;
            case 2: return RHI::Format::RG32_Float;
            case 3: return RHI::Format::RGB32_Float;
            case 4: return RHI::Format::RGBA32_Float;
            }
            break;
        case spirv_cross::SPIRType::Int:
            switch (type.vecsize) {
            case 1: return RHI::Format::R32_SInt;
            case 2: return RHI::Format::RG32_SInt;
            case 3: return RHI::Format::RGB32_SInt;
            case 4: return RHI::Format::RGBA32_SInt;
            }
            break;
        case spirv_cross::SPIRType::UInt:
            switch (type.vecsize) {
            case 1: return RHI::Format::R32_UInt;
            case 2: return RHI::Format::RG32_UInt;
            case 3: return RHI::Format::RGB32_UInt;
            case 4: return RHI::Format::RGBA32_UInt;
            }
            break;
        case spirv_cross::SPIRType::Half:
            switch (type.vecsize) {
            case 1: return RHI::Format::R16_Float;
            case 2: return RHI::Format::RG16_Float;
            case 3: return RHI::Format::RGBA16_Float; 
            case 4: return RHI::Format::RGBA16_Float;
            }
            break;
        case spirv_cross::SPIRType::Double:
            switch (type.vecsize) {
            case 1: return RHI::Format::R32_Float; 
            case 2: return RHI::Format::RG32_Float;
            case 3: return RHI::Format::RGB32_Float;
            case 4: return RHI::Format::RGBA32_Float;
            }
            break;
        case spirv_cross::SPIRType::Boolean:
            return RHI::Format::R32_UInt;
        default:
            break;
        }
        return RHI::Format::Undefined;
    }

    RHI::Format ShaderLoader::spirvImageFormatToRHI(spv::ImageFormat fmt) {
        switch (fmt) {
        case spv::ImageFormatRgba32f:       return RHI::Format::RGBA32_Float;
        case spv::ImageFormatRgba16f:       return RHI::Format::RGBA16_Float;
        case spv::ImageFormatR32f:          return RHI::Format::R32_Float;
        case spv::ImageFormatRgba8:         return RHI::Format::RGBA8_UNorm;
        case spv::ImageFormatRgba8Snorm:    return RHI::Format::RGBA8_SNorm;
        case spv::ImageFormatRgba32i:       return RHI::Format::RGBA32_SInt;
        case spv::ImageFormatRgba16i:       return RHI::Format::RGBA16_SInt;
        case spv::ImageFormatRgba8i:        return RHI::Format::RGBA8_SInt;
        case spv::ImageFormatR32i:          return RHI::Format::R32_SInt;
        case spv::ImageFormatRgba32ui:      return RHI::Format::RGBA32_UInt;
        case spv::ImageFormatRgba16ui:      return RHI::Format::RGBA16_UInt;
        case spv::ImageFormatRgba8ui:       return RHI::Format::RGBA8_UInt;
        case spv::ImageFormatR32ui:         return RHI::Format::R32_UInt;
        case spv::ImageFormatUnknown:
        default:
            return RHI::Format::Undefined;
        }
    }

    void ShaderLoader::fillTextureInfo(const spirv_cross::SPIRType& type,RHI::ResourceBinding::TextureInfo& info) {
        switch (type.image.dim) {
        case spv::Dim1D:     info.dimension = RHI::TextureDimension::Tex1D; break;
        case spv::Dim2D:     info.dimension = RHI::TextureDimension::Tex2D; break;
        case spv::Dim3D:     info.dimension = RHI::TextureDimension::Tex3D; break;
        case spv::DimCube:   info.dimension = RHI::TextureDimension::Cube; break;
        default:             info.dimension = RHI::TextureDimension::Tex2D; break;
        }
        info.isArray = type.image.arrayed;
        info.isMultisample = type.image.ms;
        info.imageFormat = spirvImageFormatToRHI(type.image.format);
    }

    uint32_t ShaderLoader::getTypeSize(const spirv_cross::SPIRType& type) {
        uint32_t base = 4; // float/int/uint 默认4字节
        if (type.basetype == spirv_cross::SPIRType::Double) base = 8;
        if (type.basetype == spirv_cross::SPIRType::Half) base = 2;
        uint32_t vec = std::max(1u, (uint32_t)type.vecsize);
        uint32_t cols = std::max(1u, (uint32_t)type.columns);
        return base * vec * cols;
    }

    size_t ShaderLoader::computeHash(const std::string& source, RHI::ShaderStage stage) const {
        size_t seed = 0;
        Utils::hash_combine(seed, source);
        Utils::hash_combine(seed, stage);
        return seed;
    }

    void ShaderLoader::clearCache() {
        std::lock_guard<std::mutex> lock(m_cacheMutex);
        m_cache.clear();
    }

} // namespace StarryEngine::Assets
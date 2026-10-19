#include <assets/loader/MaterialDesc.hpp>
#include <assets/loader/ModelDesc.hpp>
#include <assets/material/MaterialInstance.hpp>
#include <assets/loader/ModelLoader.hpp>
#include <assets/loader/TextureLoader.hpp>
#include <renderer/renderPaths/LayerRegistry.hpp>
#include <logging/Logger.hpp>
#include <glm/glm.hpp>

namespace StarryEngine::Assets {

    namespace {
        void applyDefaultLighting(const std::shared_ptr<MaterialInstance>& mat) {
            if (!mat) return;
            if (auto* lightBlock = mat->getBlock("LightingUBO")) {
                lightBlock->setVec4("lights.position", glm::vec4(-0.5f, -1.0f, -0.8f, 0.0f));
                lightBlock->setVec4("lights.color",    glm::vec4(3.0f, 2.7f, 2.3f, 1.0f));
                lightBlock->setFloat("lightCount", 1.0f);
                lightBlock->setFloat("ambientStrength", 0.15f);
            }
            mat->applyAllDirtyBlocks();
        }
    }

    bool ModelFactory::loadModel(const std::shared_ptr<RHI::ResourceManager>& resMgr,
                                 const ModelDesc& desc, ModelData& out) {
        if (!resMgr || desc.file.empty()) {
            LOG_ERROR("[ModelFactory] 无效模型描述: file='{}'", desc.file);
            return false;
        }
        ModelLoader loader(resMgr);
        if (!loader.open(desc.file)) {
            LOG_ERROR("[ModelFactory] 打开模型失败: {}", desc.file);
            return false;
        }
        auto geo = loader.loadGeometry();
        if (!geo) {
            LOG_ERROR("[ModelFactory] 几何解析失败: {}", desc.file);
            return false;
        }
        out.geometry = std::make_shared<Geometry>(std::move(*geo));
        if (!out.geometry->uploadToGPU()) {
            LOG_ERROR("[ModelFactory] 几何上传 GPU 失败: {}", desc.file);
            return false;
        }
        if (desc.skeleton) {
            if (auto sk = loader.loadSkeleton()) out.skeleton = std::make_shared<Skeleton>(std::move(*sk));
            if (auto cl = loader.loadClip())     out.clip     = std::make_shared<AnimationClip>(std::move(*cl));
        }
        out.skinned = out.skeleton && !out.skeleton->bones.empty();
        out.textureDir = desc.textureDir;
        out.params  = loader.loadMaterials();
        return true;
    }

    bool ModelFactory::loadMaterials(const std::shared_ptr<RHI::ResourceManager>& resMgr,
                                     RHI::DescriptorPoolHandle pool,
                                     const ModelData& data,
                                     const MaterialSetDesc& set,
                                     ModelInstance& out) {
        if (!resMgr || !data.geometry) return false;

        for (const auto& param : data.params) {
            const SubmeshBinding* matched = nullptr;
            for (const auto& b : set.bindings) {
                for (const auto& a : b.aliases) if (param.name == a) { matched = &b; break; }
                if (matched) break;
            }
            MaterialDesc d;
            const RHI::DescriptorSetLayoutHandle baseLayout =set.bindings.empty() ? 
            RHI::DescriptorSetLayoutHandle::Null(): set.bindings.front().material.baseSetLayout;

            if (matched) {
                d = matched->material;
            } else {
                d.vertexShaderPath   = set.fallbackVertexShader;
                d.fragmentShaderPath = set.fallbackFragmentShader;
                d.renderLayer        = set.fallbackLayer;
            }
            if (!d.baseSetLayout.isValid()) d.baseSetLayout = baseLayout;
            d.name = param.name;
            // 贴图目录：材质槽的相对文件名按它解析（来自资产描述）
            if (!data.textureDir.empty()) d.textureDir = data.textureDir;
            // 顶点着色器：绑定没给就补默认（蒙皮/非蒙皮由材质策略决定，可整体替换）
            if (d.vertexShaderPath.empty()) d.vertexShaderPath = set.fallbackVertexShader;
            if (!set.vertexShaderOverride.empty()) d.vertexShaderPath = set.vertexShaderOverride;

            // 只有启用描边时，本体才改成"模板写 + Outline/0"；否则保持材质表里的设置
            if (set.outline) {
                d.renderLayer = RenderLayers::Outline;
                d.renderQueue = 0;
                d.stencilTest = true;
                RHI::StencilOpState write;
                write.failOp = RHI::StencilOp::Keep;
                write.passOp = RHI::StencilOp::Replace;
                write.depthFailOp = RHI::StencilOp::Keep;
                write.compareOp = RHI::CompareOp::Always;
                write.compareMask = 0xFF;
                write.writeMask = 0xFF;
                write.reference = 1;
                d.stencilFront = write;
                d.stencilBack = write;
            }

            auto m = MaterialFactory::create(resMgr, pool, d);
            if (m) {
                applyDefaultLighting(m);
                out.parts.emplace_back(param.name, m);
            }
        }

        // 描边：模板测 + 描边 shader（材质策略可替换）
        if (set.outline) {
            MaterialDesc d;
            d.name = "Outline";
            d.baseSetLayout = set.bindings.empty() ? RHI::DescriptorSetLayoutHandle::Null()
                                                   : set.bindings.front().material.baseSetLayout;
            d.vertexShaderPath   = set.outlineVertexShader;
            d.fragmentShaderPath = set.outlineFragmentShader;
            d.renderLayer = RenderLayers::Outline;
            d.renderQueue = 1;
            d.depthTest = true;
            d.depthWrite = false;
            d.stencilTest = true;
            RHI::StencilOpState test;
            test.failOp = RHI::StencilOp::Keep;
            test.passOp = RHI::StencilOp::Keep;
            test.depthFailOp = RHI::StencilOp::Keep;
            test.compareOp = RHI::CompareOp::NotEqual;
            test.compareMask = 0xFF;
            test.writeMask = 0x00;
            test.reference = 1;
            d.stencilFront = test;
            d.stencilBack = test;
            MaterialTextureSlot slot;
            slot.name = "texSampler";
            slot.format = RHI::Format::RGBA8_UNorm;
            if (!set.outlineTexture.empty()) {
                slot.path = set.outlineTexture;
            } else {
                slot.inlineData = { 255, 255, 255, 255 };
                slot.inlineWidth = 1;
                slot.inlineHeight = 1;
            }
            d.textures.push_back(slot);
            out.outlineMaterial = MaterialFactory::create(resMgr, pool, d);
        }

        // 蒙皮：写绑定姿态骨骼矩阵
        if (data.skinned) {
            std::vector<glm::mat4> bindPose(data.skeleton->bones.size(), glm::mat4(1.0f));
            const size_t bytes = bindPose.size() * sizeof(glm::mat4);
            for (auto& [_, m] : out.parts) if (m) m->setStorageBuffer(1, 2, bindPose.data(), bytes);
            if (out.outlineMaterial) out.outlineMaterial->setStorageBuffer(1, 2, bindPose.data(), bytes);
        }
        return !out.parts.empty();
    }

    bool ModelFactory::load(const std::shared_ptr<RHI::ResourceManager>& resMgr,
                            RHI::DescriptorPoolHandle pool,
                            const ModelDesc& desc,
                            const MaterialSetDesc& set,
                            ModelInstance& out) {
        ModelData data;
        if (!loadModel(resMgr, desc, data)) return false;
        if (!loadMaterials(resMgr, pool, data, set, out)) return false;
        out.geometry = data.geometry;
        out.skeleton = data.skeleton;
        out.clip     = data.clip;
        out.skinned  = data.skinned;
        LOG_INFO("[ModelFactory] '{}' 装配完成: {} 子网格材质 skinned={} outline={}",
                 desc.file, out.parts.size(), data.skinned, (bool)out.outlineMaterial);
        return true;
    }

} // namespace StarryEngine::Assets

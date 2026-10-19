#include <assets/material/MaterialDesc.hpp>
#include <assets/material/MaterialInstance.hpp>
#include <assets/material/DefaultMaterialTemplate.hpp>
#include <assets/loader/TextureLoader.hpp>
#include <logging/Logger.hpp>
#include <unordered_map>

namespace StarryEngine::Assets {

std::shared_ptr<MaterialInstance> MaterialFactory::create(const std::shared_ptr<RHI::ResourceManager>& resMgr,
                                                          RHI::DescriptorPoolHandle pool,
                                                          const MaterialDesc& desc) {
    if (!resMgr || !pool.isValid()) {
        LOG_ERROR("[MaterialFactory] 无效的资源管理器或描述符池 (材质 '{}')", desc.name);
        return nullptr;
    }

    // 1) 模板：布局 → 变体宏（必须在 loadShaders 之前，旧实现也是这个顺序）→ 着色器
    std::shared_ptr<DefaultMaterialTemplate> tmpl;
    if (desc.layouts.empty() && desc.baseSetLayout.isValid()) {
        // 与旧实现的构造方式一致：单一基础布局
        tmpl = std::make_shared<DefaultMaterialTemplate>(resMgr, desc.baseSetLayout);
    } else {
        tmpl = std::make_shared<DefaultMaterialTemplate>(resMgr, desc.layouts);
    }
    if (!desc.variants.empty()) {
        std::unordered_map<std::string, std::string> macros;
        for (const auto& v : desc.variants) macros[v] = "1";
        tmpl->setMacros(std::move(macros));
    }
    tmpl->setDebugName(desc.name);
    if (!desc.vertexShaderPath.empty() || !desc.fragmentShaderPath.empty()) {
        if (!tmpl->loadShaders(desc.vertexShaderPath, desc.fragmentShaderPath)) {
            LOG_ERROR("[MaterialFactory] 着色器加载失败: 材质 '{}' ({} / {})",
                      desc.name, desc.vertexShaderPath, desc.fragmentShaderPath);
            return nullptr;
        }
    }

    // 2) 实例
    auto mat = std::make_shared<MaterialInstance>(tmpl, pool, resMgr.get(), desc.globalSet);
    if (!mat) return nullptr;
    mat->setDebugName(desc.name);

    // 3) 渲染状态
    mat->setCullMode(desc.cullMode);
    mat->enableDepthTest(desc.depthTest);
    mat->enableDepthWrite(desc.depthWrite);
    mat->setDepthCompareOp(desc.depthCompareOp);
    if (desc.stencilTest) {
        mat->setStencilTest(true);
        mat->setStencilOps(desc.stencilFront, desc.stencilBack);
    }
    if (!desc.attachments.empty()) mat->setAttachments(desc.attachments);
    mat->setDeferred(desc.deferred);
    if (!desc.subpassTag.empty()) mat->setSubpassTag(desc.subpassTag);
    mat->setRenderLayer(desc.renderLayer);
    mat->setRenderQueue(desc.renderQueue);
    if (desc.currentSlot != 0xFFFFFFFFu) mat->setCurrentSlot(desc.currentSlot);

    // 4) 纹理：按名字（自动加载路径）或按 (set,binding)
    TextureLoader texLoader(resMgr);
    for (const auto& slot : desc.textures) {
        // 关键字门控：仅在关键字开启时绑定（CharMatSpec 的 keyword 列）
        if (!slot.requiredKeyword.empty()) {
            bool on = false;
            for (const auto& v : desc.variants) { if (v == slot.requiredKeyword) { on = true; break; } }
            if (!on) continue;
        }
        RHI::TextureHandle tex = slot.texture;
        RHI::SamplerHandle smp = slot.sampler;
        if (!tex.isValid() && !slot.path.empty()) {
            // 相对文件名按 textureDir 解析
            std::string full = slot.path;
            if (!desc.textureDir.empty() && full.find('/') == std::string::npos) {
                full = desc.textureDir + "/" + full;
            }
            auto loaded = texLoader.loadTexture2D(full, slot.format, desc.name + ":" + slot.name);
            tex = loaded.texture;
            if (!smp.isValid()) smp = loaded.sampler;
        }
        if (!tex.isValid()) {
            LOG_WARN("[MaterialFactory] 材质 '{}' 纹理槽 '{}' 无有效纹理，跳过", desc.name, slot.name);
            continue;
        }
        if (!slot.name.empty()) {
            mat->setTexture(slot.name, tex, smp);
        }
        else if (slot.set != 0xFFFFFFFFu && slot.binding != 0xFFFFFFFFu) {
            if (slot.asInputAttachment) mat->setInputAttachment(slot.set, slot.binding, tex, slot.layout);
            else                         mat->setTexture(slot.set, slot.binding, tex, smp);
        }
    }

    // 5) UBO / SSBO
    for (const auto& b : desc.buffers) {
        if (!b.data || b.size == 0) continue;
        if (!b.name.empty()) {
            if (b.isStorage) mat->setStorageBuffer(b.set, b.binding, b.data, b.size, b.slot);
            else             mat->setUniform(b.set, b.binding, b.data, b.size, b.slot);
        }
        else if (b.set != 0xFFFFFFFFu && b.binding != 0xFFFFFFFFu) {
            if (b.isStorage) mat->setStorageBuffer(b.set, b.binding, b.data, b.size, b.slot);
            else             mat->setUniform(b.set, b.binding, b.data, b.size, b.slot);
        }
    }

    LOG_INFO("[MaterialFactory] 已创建材质 '{}'（纹理槽 {} 个，缓冲 {} 个）",
             desc.name, desc.textures.size(), desc.buffers.size());
    return mat;
}

} // namespace StarryEngine::Assets

#include "Renderpass.hpp"

namespace StarryEngine::RenderGraph {
    RenderPass::RenderPass() = default;

    void RenderPass::addAttachment(const RHI::AttachmentDesc& attachment) {
        mAttachments.push_back(attachment);
    }

    void RenderPass::addSubpass(std::unique_ptr<Subpass> subpass) {
        mSubpasses.push_back(std::move(subpass));
    }

    void RenderPass::addDependency(const RHI::SubpassDependency& dependency) {
        mDependencies.push_back(dependency);
    }

    RHI::RenderPassDesc RenderPass::getRenderPassDesc() const {
        RHI::RenderPassDesc desc;
        desc.attachments = mAttachments;

        // 从 Subpass 对象构建 SubpassDesc
        for (const auto& subpass : mSubpasses) {
            desc.subpasses.push_back(subpass->build());
        }

        desc.dependencies = mDependencies;
        // 注意：RenderPassDesc 可能还有 debugName 等字段，这里暂不设置，可在外部设置
        return desc;
    }

    RenderPassBuilder::RenderPassBuilder(std::string name)
        : mName(std::move(name)) {
    }

    RenderPassBuilder& RenderPassBuilder::addAttachment(const std::string& name, const RHI::AttachmentDesc& attachment) {
        if (mAttachmentIndices.find(name) != mAttachmentIndices.end()) {
            throw std::runtime_error("Attachment with name '" + name + "' already exists");
        }

        uint32_t index = static_cast<uint32_t>(mAttachments.size());
        mAttachmentIndices[name] = index;
        mAttachmentNames.push_back(name);
        mAttachments.push_back(attachment);

        return *this;
    }

    SubpassBuilder& RenderPassBuilder::addSubpass(SubpassBuilder&& subpassBuilder) {
        mSubpassBuilders.push_back(std::move(subpassBuilder));
        return mSubpassBuilders.back();
    }

    RenderPassBuilder& RenderPassBuilder::addDependency(const RHI::SubpassDependency& dependency) {
        mManualDependencies.push_back(dependency);
        return *this;
    }

    RenderPassBuilder& RenderPassBuilder::addColorAttachment(const std::string& name,
        RHI::Format format,
        RHI::ImageLayout finalLayout,
        RHI::AttachmentLoadOp loadOp,
        RHI::AttachmentStoreOp storeOp) {
        RHI::AttachmentDesc attachment{};
        attachment.format = format;
        attachment.sampleCount = 1; // 默认非MSAA
        attachment.loadOp = loadOp;
        attachment.storeOp = storeOp;
        attachment.stencilLoadOp = RHI::AttachmentLoadOp::DontCare;
        attachment.stencilStoreOp = RHI::AttachmentStoreOp::DontCare;
        attachment.initialLayout = RHI::ImageLayout::Undefined;
        attachment.finalLayout = finalLayout;

        return addAttachment(name, attachment);
    }

    RenderPassBuilder& RenderPassBuilder::addDepthAttachment(const std::string& name,
        RHI::Format format,
        RHI::AttachmentLoadOp loadOp,
        RHI::AttachmentStoreOp storeOp) {
        RHI::AttachmentDesc attachment{};
        attachment.format = format;
        attachment.sampleCount = 1;
        attachment.loadOp = loadOp;
        attachment.storeOp = storeOp;
        attachment.stencilLoadOp = RHI::AttachmentLoadOp::Clear; // 通常深度模板也会清除模板
        attachment.stencilStoreOp = RHI::AttachmentStoreOp::DontCare;
        attachment.initialLayout = RHI::ImageLayout::Undefined;
        attachment.finalLayout = RHI::ImageLayout::DepthStencilAttachment;

        return addAttachment(name, attachment);
    }

    RenderPassBuilder& RenderPassBuilder::addResolveAttachment(const std::string& name,
        RHI::Format format,
        RHI::ImageLayout finalLayout) {
        RHI::AttachmentDesc attachment{};
        attachment.format = format;
        attachment.sampleCount = 1;
        attachment.loadOp = RHI::AttachmentLoadOp::DontCare;
        attachment.storeOp = RHI::AttachmentStoreOp::Store;
        attachment.stencilLoadOp = RHI::AttachmentLoadOp::DontCare;
        attachment.stencilStoreOp = RHI::AttachmentStoreOp::DontCare;
        attachment.initialLayout = RHI::ImageLayout::Undefined;
        attachment.finalLayout = finalLayout;

        return addAttachment(name, attachment);
    }

    RenderPassBuilder& RenderPassBuilder::addInputAttachment(const std::string& name,
        RHI::Format format,
        RHI::ImageLayout finalLayout,
        RHI::ImageLayout initialLayout,
        RHI::AttachmentLoadOp loadOp,
        RHI::AttachmentStoreOp storeOp) {
        RHI::AttachmentDesc attachment{};
        attachment.format = format;
        attachment.sampleCount = 1;
        attachment.loadOp = loadOp;
        attachment.storeOp = storeOp;
        attachment.stencilLoadOp = RHI::AttachmentLoadOp::DontCare;
        attachment.stencilStoreOp = RHI::AttachmentStoreOp::DontCare;
        attachment.initialLayout = initialLayout;  
        attachment.finalLayout = finalLayout;
        return addAttachment(name, attachment);
    }

    std::unique_ptr<RenderPassBuildResult> RenderPassBuilder::build(bool autoDependencies) {
        // 分析附件使用情况
        analyzeAttachmentUsage();

        // 自动推导依赖
        if (autoDependencies) {
            generateDependenciesFromUsage();
        }

        auto result = std::make_unique<RenderPassBuildResult>();
        result->name = mName;

        // 创建 RenderPass 对象（仅作为描述容器）
        auto renderPass = std::make_unique<RenderPass>();

        // 添加所有附件
        for (const auto& attachment : mAttachments) {
            renderPass->addAttachment(attachment);
        }

        // 构建所有子流程并收集信息
        for (uint32_t subpassIndex = 0; subpassIndex < mSubpassBuilders.size(); ++subpassIndex) {
            auto& subpassBuilder = mSubpassBuilders[subpassIndex];

            // 验证子流程中引用的附件都存在
            for (const auto& name : subpassBuilder.getColorAttachmentNames()) {
                if (mAttachmentIndices.find(name) == mAttachmentIndices.end()) {
                    throw std::runtime_error("Color attachment '" + name + "' not found in render pass");
                }
            }
            for (const auto& name : subpassBuilder.getInputAttachmentNames()) {
                if (mAttachmentIndices.find(name) == mAttachmentIndices.end()) {
                    throw std::runtime_error("Input attachment '" + name + "' not found in render pass");
                }
            }
            if (subpassBuilder.getDepthStencilAttachmentName()) {
                const auto& name = *subpassBuilder.getDepthStencilAttachmentName();
                if (mAttachmentIndices.find(name) == mAttachmentIndices.end()) {
                    throw std::runtime_error("Depth/stencil attachment '" + name + "' not found in render pass");
                }
            }

            // 构建子流程（Subpass 对象）
            auto subpass = subpassBuilder.build(mAttachmentIndices);
            renderPass->addSubpass(std::move(subpass));

            result->pipelineNameToSubpassIndexMap[subpassBuilder.getPipelineName()] = subpassIndex;
        }

        // 合并依赖（手动 + 自动，自动去重）
        auto finalDependencies = mergeDependencies();
        for (const auto& dep : finalDependencies) {
            renderPass->addDependency(dep);
        }

        std::cout << "\n=== Attachment Index Map ===" << std::endl;
        for (const auto& [name, index] : mAttachmentIndices) {
            std::cout << "Attachment: " << name << " -> Index: " << index << std::endl;
        }

        result->renderPass = std::move(renderPass);

        // 填充 Pipeline 描述和 Renderer
        for (const auto& subpassBuilder : mSubpassBuilders) {
            auto* renderer = subpassBuilder.getRenderer();
            std::cout << "[RenderPassBuilder] subpass renderer = " << renderer << std::endl;
            result->pipelineDescriptions.push_back(subpassBuilder.getPipelineDescription());
            result->subpassRenderers.push_back(renderer);
        }

        result->attachmentNames = mAttachmentNames;
        result->attachmentNameToIndex = mAttachmentIndices;

        return result;
    }

    // ========== 依赖推导私有方法 ==========

    void RenderPassBuilder::analyzeAttachmentUsage() {
        mAttachmentUsage.clear();

        // 初始化附件使用信息
        for (uint32_t i = 0; i < mAttachments.size(); ++i) {
            const auto& attachment = mAttachments[i];
            const auto& name = mAttachmentNames[i];
            mAttachmentUsage[name] = {
                {}, {},
                attachment.initialLayout,
                attachment.finalLayout
            };
        }

        // 分析每个子流程的附件使用
        for (uint32_t subpassIndex = 0; subpassIndex < mSubpassBuilders.size(); ++subpassIndex) {
            const auto& subpassBuilder = mSubpassBuilders[subpassIndex];

            // 颜色附件 = 写入
            for (const auto& name : subpassBuilder.getColorAttachmentNames()) {
                mAttachmentUsage[name].writingSubpasses.insert(subpassIndex);
            }

            // 输入附件 = 读取
            for (const auto& name : subpassBuilder.getInputAttachmentNames()) {
                mAttachmentUsage[name].readingSubpasses.insert(subpassIndex);
            }

            // 深度模板附件 = 读写
            if (subpassBuilder.getDepthStencilAttachmentName()) {
                const auto& name = *subpassBuilder.getDepthStencilAttachmentName();
                mAttachmentUsage[name].writingSubpasses.insert(subpassIndex);
                mAttachmentUsage[name].readingSubpasses.insert(subpassIndex);
            }

            // 解析附件 = 写入
            for (const auto& name : subpassBuilder.getResolveAttachmentNames()) {
                mAttachmentUsage[name].writingSubpasses.insert(subpassIndex);
            }
        }
    }

    void RenderPassBuilder::generateDependenciesFromUsage() {
        mAutoDependencies.clear();

        // 为每个附件生成依赖
        for (const auto& [attachmentName, usage] : mAttachmentUsage) {
            generateDependenciesForAttachment(attachmentName, usage);
        }

        // 添加子流程间的执行顺序依赖
        generateExecutionDependencies();
    }

    void RenderPassBuilder::generateDependenciesForAttachment(const std::string& name, const AttachmentUsage& usage) {
        const auto& writers = usage.writingSubpasses;
        const auto& readers = usage.readingSubpasses;

        // 判断附件是否为深度模板
        bool isDepthStencil = false;
        auto it = mAttachmentIndices.find(name);
        if (it != mAttachmentIndices.end()) {
            uint32_t index = it->second;
            RHI::Format format = mAttachments[index].format;
            isDepthStencil = isDepthStencilFormat(format);
        }

        // 处理写入 -> 读取依赖
        for (uint32_t writer : writers) {
            for (uint32_t reader : readers) {
                if (reader > writer) {
                    if (isDepthStencil) {
                        addDepthReadAfterWriteDependency(writer, reader);
                    }
                    else {
                        addColorReadAfterWriteDependency(writer, reader);
                    }
                }
            }
        }

        // 处理写入 -> 写入依赖（避免覆盖）
        std::vector<uint32_t> writersVec(writers.begin(), writers.end());
        for (size_t i = 0; i < writersVec.size(); ++i) {
            for (size_t j = i + 1; j < writersVec.size(); ++j) {
                if (isDepthStencil) {
                    addDepthWriteAfterWriteDependency(writersVec[i], writersVec[j]);
                }
                else {
                    addColorWriteAfterWriteDependency(writersVec[i], writersVec[j]);
                }
            }
        }

        // 外部依赖
        generateExternalDependencies(usage, isDepthStencil);
    }

    void RenderPassBuilder::addColorReadAfterWriteDependency(uint32_t src, uint32_t dst) {
        RHI::SubpassDependency dep{};
        dep.srcSubpass = src;
        dep.dstSubpass = dst;
        dep.srcStageMask = static_cast<RHI::PipelineStageFlags>(RHI::PipelineStage::ColorAttachmentOutput);
        dep.dstStageMask = static_cast<RHI::PipelineStageFlags>(RHI::PipelineStage::FragmentShader);
        dep.srcAccessMask = static_cast<RHI::AccessFlags>(RHI::AccessFlag::ColorAttachmentWrite);
        dep.dstAccessMask = static_cast<RHI::AccessFlags>(RHI::AccessFlag::InputAttachmentRead);
        dep.byRegion = true;
        mAutoDependencies.push_back(dep);
    }

    void RenderPassBuilder::addDepthReadAfterWriteDependency(uint32_t src, uint32_t dst) {
        RHI::SubpassDependency dep{};
        dep.srcSubpass = src;
        dep.dstSubpass = dst;
        dep.srcStageMask = static_cast<RHI::PipelineStageFlags>(RHI::PipelineStage::LateFragmentTests);
        dep.dstStageMask = static_cast<RHI::PipelineStageFlags>(RHI::PipelineStage::FragmentShader);
        dep.srcAccessMask = static_cast<RHI::AccessFlags>(RHI::AccessFlag::DepthStencilAttachmentWrite);
        dep.dstAccessMask = static_cast<RHI::AccessFlags>(RHI::AccessFlag::InputAttachmentRead);
        dep.byRegion = true;
        mAutoDependencies.push_back(dep);
    }

    void RenderPassBuilder::addColorWriteAfterWriteDependency(uint32_t first, uint32_t second) {
        RHI::SubpassDependency dep{};
        dep.srcSubpass = first;
        dep.dstSubpass = second;
        dep.srcStageMask = static_cast<RHI::PipelineStageFlags>(RHI::PipelineStage::ColorAttachmentOutput);
        dep.dstStageMask = static_cast<RHI::PipelineStageFlags>(RHI::PipelineStage::ColorAttachmentOutput);
        dep.srcAccessMask = static_cast<RHI::AccessFlags>(RHI::AccessFlag::ColorAttachmentWrite);
        dep.dstAccessMask = static_cast<RHI::AccessFlags>(RHI::AccessFlag::ColorAttachmentRead) |
            static_cast<RHI::AccessFlags>(RHI::AccessFlag::ColorAttachmentWrite);
        dep.byRegion = true;
        mAutoDependencies.push_back(dep);
    }

    void RenderPassBuilder::addDepthWriteAfterWriteDependency(uint32_t first, uint32_t second) {
        RHI::SubpassDependency dep{};
        dep.srcSubpass = first;
        dep.dstSubpass = second;
        dep.srcStageMask = static_cast<RHI::PipelineStageFlags>(RHI::PipelineStage::LateFragmentTests);
        dep.dstStageMask = static_cast<RHI::PipelineStageFlags>(RHI::PipelineStage::LateFragmentTests);
        dep.srcAccessMask = static_cast<RHI::AccessFlags>(RHI::AccessFlag::DepthStencilAttachmentWrite);
        dep.dstAccessMask = static_cast<RHI::AccessFlags>(RHI::AccessFlag::DepthStencilAttachmentRead) |
            static_cast<RHI::AccessFlags>(RHI::AccessFlag::DepthStencilAttachmentWrite);
        dep.byRegion = true;
        mAutoDependencies.push_back(dep);
    }

    void RenderPassBuilder::generateExternalDependencies(const AttachmentUsage& usage, bool isDepthStencil) {
        // 找出第一个使用者
        uint32_t firstUser = SUBPASS_EXTERNAL;
        if (!usage.writingSubpasses.empty()) {
            firstUser = *usage.writingSubpasses.begin();
        }
        else if (!usage.readingSubpasses.empty()) {
            firstUser = *usage.readingSubpasses.begin();
        }

        // 从外部到第一个使用者
        if (firstUser != SUBPASS_EXTERNAL) {
            RHI::SubpassDependency dep{};
            dep.srcSubpass = SUBPASS_EXTERNAL;
            dep.dstSubpass = firstUser;
            dep.byRegion = true;
            if (isDepthStencil) {
                dep.srcStageMask = static_cast<RHI::PipelineStageFlags>(RHI::PipelineStage::BottomOfPipe);
                dep.dstStageMask = static_cast<RHI::PipelineStageFlags>(RHI::PipelineStage::EarlyFragmentTests);
                dep.srcAccessMask = static_cast<RHI::AccessFlags>(RHI::AccessFlag::None);
                dep.dstAccessMask = static_cast<RHI::AccessFlags>(RHI::AccessFlag::DepthStencilAttachmentRead) |
                    static_cast<RHI::AccessFlags>(RHI::AccessFlag::DepthStencilAttachmentWrite);
            }
            else {
                dep.srcStageMask = static_cast<RHI::PipelineStageFlags>(RHI::PipelineStage::BottomOfPipe);
                dep.dstStageMask = static_cast<RHI::PipelineStageFlags>(RHI::PipelineStage::ColorAttachmentOutput);
                dep.srcAccessMask = static_cast<RHI::AccessFlags>(RHI::AccessFlag::None);
                dep.dstAccessMask = static_cast<RHI::AccessFlags>(RHI::AccessFlag::ColorAttachmentRead) |
                    static_cast<RHI::AccessFlags>(RHI::AccessFlag::ColorAttachmentWrite);
            }
            mAutoDependencies.push_back(dep);
        }

        // 找出最后一个使用者
        uint32_t lastUser = SUBPASS_EXTERNAL;
        if (!usage.readingSubpasses.empty()) {
            lastUser = *usage.readingSubpasses.rbegin();
        }
        if (!usage.writingSubpasses.empty()) {
            uint32_t lastWriter = *usage.writingSubpasses.rbegin();
            if (lastUser == SUBPASS_EXTERNAL || lastWriter > lastUser) {
                lastUser = lastWriter;
            }
        }

        // 从最后一个使用者到外部
        if (lastUser != SUBPASS_EXTERNAL) {
            RHI::SubpassDependency dep{};
            dep.srcSubpass = lastUser;
            dep.dstSubpass = SUBPASS_EXTERNAL;
            dep.byRegion = true;
            if (isDepthStencil) {
                dep.srcStageMask = static_cast<RHI::PipelineStageFlags>(RHI::PipelineStage::LateFragmentTests);
                dep.dstStageMask = static_cast<RHI::PipelineStageFlags>(RHI::PipelineStage::BottomOfPipe);
                dep.srcAccessMask = static_cast<RHI::AccessFlags>(RHI::AccessFlag::DepthStencilAttachmentRead) |
                    static_cast<RHI::AccessFlags>(RHI::AccessFlag::DepthStencilAttachmentWrite);
                dep.dstAccessMask = static_cast<RHI::AccessFlags>(RHI::AccessFlag::None);
            }
            else {
                dep.srcStageMask = static_cast<RHI::PipelineStageFlags>(RHI::PipelineStage::ColorAttachmentOutput);
                dep.dstStageMask = static_cast<RHI::PipelineStageFlags>(RHI::PipelineStage::BottomOfPipe);
                dep.srcAccessMask = static_cast<RHI::AccessFlags>(RHI::AccessFlag::ColorAttachmentRead) |
                    static_cast<RHI::AccessFlags>(RHI::AccessFlag::ColorAttachmentWrite);
                dep.dstAccessMask = static_cast<RHI::AccessFlags>(RHI::AccessFlag::None);
            }
            mAutoDependencies.push_back(dep);
        }
    }

    void RenderPassBuilder::generateExecutionDependencies() {
        // 确保子流程按顺序执行（可选，通常已有依赖）
        for (uint32_t i = 0; i < mSubpassBuilders.size() - 1; ++i) {
            RHI::SubpassDependency dep{};
            dep.srcSubpass = i;
            dep.dstSubpass = i + 1;
            dep.srcStageMask = static_cast<RHI::PipelineStageFlags>(RHI::PipelineStage::AllGraphics);
            dep.dstStageMask = static_cast<RHI::PipelineStageFlags>(RHI::PipelineStage::AllGraphics);
            dep.srcAccessMask = static_cast<RHI::AccessFlags>(RHI::AccessFlag::ColorAttachmentWrite) |
                static_cast<RHI::AccessFlags>(RHI::AccessFlag::DepthStencilAttachmentWrite);
            dep.dstAccessMask = static_cast<RHI::AccessFlags>(RHI::AccessFlag::ColorAttachmentRead) |
                static_cast<RHI::AccessFlags>(RHI::AccessFlag::DepthStencilAttachmentRead) |
                static_cast<RHI::AccessFlags>(RHI::AccessFlag::InputAttachmentRead);
            dep.byRegion = true;
            mAutoDependencies.push_back(dep);
        }
    }

    bool RenderPassBuilder::isDepthStencilFormat(RHI::Format format) const {
        switch (format) {
        case RHI::Format::D16_UNorm:
        case RHI::Format::D32_Float:
        case RHI::Format::D24_UNorm_S8_UInt:
        case RHI::Format::D32_Float_S8_UInt:
            return true;
        default:
            return false;
        }
    }

    std::vector<RHI::SubpassDependency> RenderPassBuilder::mergeDependencies() const {
        std::vector<RHI::SubpassDependency> result = mManualDependencies;

        // 添加自动推导的依赖，排除与手动依赖重复的
        for (const auto& autoDep : mAutoDependencies) {
            bool duplicate = false;
            for (const auto& manualDep : mManualDependencies) {
                if (manualDep.srcSubpass == autoDep.srcSubpass &&
                    manualDep.dstSubpass == autoDep.dstSubpass &&
                    manualDep.srcStageMask == autoDep.srcStageMask &&
                    manualDep.dstStageMask == autoDep.dstStageMask &&
                    manualDep.srcAccessMask == autoDep.srcAccessMask &&
                    manualDep.dstAccessMask == autoDep.dstAccessMask &&
                    manualDep.byRegion == autoDep.byRegion) {
                    duplicate = true;
                    break;
                }
            }
            if (!duplicate) {
                result.push_back(autoDep);
            }
        }

        return result;
    }
}
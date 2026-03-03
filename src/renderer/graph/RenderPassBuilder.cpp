#include "RenderPassBuilder.hpp"
#include "SubpassBuilder.hpp"
#include <iostream>
#include <algorithm>

namespace StarryEngine::RenderGraph {

    RenderPassBuilder::RenderPassBuilder(std::string name)
        : m_name(std::move(name)) {
    }

    RenderPassBuilder& RenderPassBuilder::addAttachment(const std::string& name, const RHI::AttachmentDesc& attachment) {
        if (m_attachmentIndices.find(name) != m_attachmentIndices.end()) {
            throw std::runtime_error("Attachment with name '" + name + "' already exists");
        }

        uint32_t index = static_cast<uint32_t>(m_attachments.size());
        m_attachmentIndices[name] = index;
        m_attachmentNames.push_back(name);
        m_attachments.push_back(attachment);
        return *this;
    }

    SubpassBuilder& RenderPassBuilder::addSubpass(SubpassBuilder&& subpassBuilder) {
        m_subpassBuilders.push_back(std::move(subpassBuilder));
        return m_subpassBuilders.back();
    }

    RenderPassBuilder& RenderPassBuilder::addDependency(const RHI::SubpassDependency& dependency) {
        m_manualDependencies.push_back(dependency);
        return *this;
    }

    RenderPassBuilder& RenderPassBuilder::registerColorAttachment(const std::string& name,
        RHI::Format format,
        RHI::ImageLayout finalLayout,
        RHI::AttachmentLoadOp loadOp,
        RHI::AttachmentStoreOp storeOp,
        RHI::ImageLayout initialLayout) {
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

    RenderPassBuilder& RenderPassBuilder::registerDepthAttachment(const std::string& name,
        RHI::Format format,
        RHI::AttachmentLoadOp loadOp,
        RHI::AttachmentStoreOp storeOp,
        RHI::ImageLayout initialLayout,
        RHI::ImageLayout finalLayout) {
        RHI::AttachmentDesc attachment{};
        attachment.format = format;
        attachment.sampleCount = 1;
        attachment.loadOp = loadOp;
        attachment.storeOp = storeOp;
        attachment.stencilLoadOp = RHI::AttachmentLoadOp::Clear;
        attachment.stencilStoreOp = RHI::AttachmentStoreOp::DontCare;
        attachment.initialLayout = initialLayout;
        attachment.finalLayout = finalLayout;
        return addAttachment(name, attachment);
    }

    RenderPassBuilder& RenderPassBuilder::registerResolveAttachment(const std::string& name,
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

    RenderPassBuilder& RenderPassBuilder::registerInputAttachment(const std::string& name,
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

        // 自动推导依赖（不再包含执行依赖）
        if (autoDependencies) {
            generateDependenciesFromUsage();
        }

        auto result = std::make_unique<RenderPassBuildResult>();
        result->name = m_name;
        result->attachmentNames = m_attachmentNames;
        result->attachmentNameToIndex = m_attachmentIndices;

        // 构建 RenderPassDesc
        result->renderPassDesc.attachments = m_attachments;

        // 构建所有子流程描述
        for (uint32_t subpassIndex = 0; subpassIndex < m_subpassBuilders.size(); ++subpassIndex) {
            auto& subpassBuilder = m_subpassBuilders[subpassIndex];

            // 验证子流程中引用的附件都存在
            for (const auto& name : subpassBuilder.getColorAttachmentNames()) {
                if (m_attachmentIndices.find(name) == m_attachmentIndices.end()) {
                    throw std::runtime_error("Color attachment '" + name + "' not found in render pass");
                }
            }
            for (const auto& name : subpassBuilder.getInputAttachmentNames()) {
                if (m_attachmentIndices.find(name) == m_attachmentIndices.end()) {
                    throw std::runtime_error("Input attachment '" + name + "' not found in render pass");
                }
            }
            if (subpassBuilder.getDepthStencilAttachmentName()) {
                const auto& name = *subpassBuilder.getDepthStencilAttachmentName();
                if (m_attachmentIndices.find(name) == m_attachmentIndices.end()) {
                    throw std::runtime_error("Depth/stencil attachment '" + name + "' not found in render pass");
                }
            }

            for (size_t i = 0; i < m_attachments.size(); ++i) {
                const auto& att = m_attachments[i];
                std::cout << "  Attachment[" << i << "] initialLayout=" << static_cast<int>(att.initialLayout)
                    << ", finalLayout=" << static_cast<int>(att.finalLayout) << std::endl;
            }

            // 构建子流程描述并添加到 renderPassDesc
            result->renderPassDesc.subpasses.push_back(subpassBuilder.buildSubpassDesc(m_attachmentIndices));

            // 在 result->renderPassDesc.subpasses.push_back(...) 之后立即添加
            for (size_t i = 0; i < result->renderPassDesc.subpasses.size(); ++i) {
                const auto& subpass = result->renderPassDesc.subpasses[i];
                const auto& builder = m_subpassBuilders[i];
                std::cout << "Subpass " << i << " (" << builder.getSubpassName() << "):\n";
                std::cout << "  Color attachments: ";
                for (const auto& ref : subpass.colorAttachments)
                    std::cout << ref.attachment << " ";
                std::cout << "\n  Input attachments: ";
                for (const auto& ref : subpass.inputAttachments)
                    std::cout << ref.attachment << " ";
                if (subpass.depthStencilAttachment.attachment != ATTACHMENT_UNUSED)
                    std::cout << "\n  Depth attachment: " << subpass.depthStencilAttachment.attachment
                    << " (layout: " << (int)subpass.depthStencilAttachment.layout << ")";
                else
                    std::cout << "\n  Depth attachment: UNUSED";
                std::cout << std::endl;
            }

            result->pipelineNameToSubpassIndexMap[subpassBuilder.getPipelineName()] = subpassIndex;
        }

        // 合并依赖
        result->renderPassDesc.dependencies = mergeDependencies();

        std::cout << "Manual dependencies count: " << m_manualDependencies.size() << std::endl;
        for (const auto& dep : m_manualDependencies) {
            std::cout << "  src=" << dep.srcSubpass << " dst=" << dep.dstSubpass << std::endl;
        }
        // 填充 Pipeline 描述和 Renderer
        for (const auto& subpassBuilder : m_subpassBuilders) {
            result->pipelineDescriptions.push_back(subpassBuilder.getPipelineDescription());
            result->subpassRenderers.push_back(subpassBuilder.getRenderer());
        }

        for (size_t i = 0; i < result->renderPassDesc.subpasses.size(); ++i) {
            const auto& subpass = result->renderPassDesc.subpasses[i];
            std::cout << "[build] Final subpass " << i << " depth layout: "
                << static_cast<int>(subpass.depthStencilAttachment.layout) << std::endl;
        }

        return result;
    }

    // ========== 依赖推导私有方法 ==========

    void RenderPassBuilder::analyzeAttachmentUsage() {
        m_attachmentUsage.clear();

        // 初始化附件使用信息
        for (uint32_t i = 0; i < m_attachments.size(); ++i) {
            const auto& attachment = m_attachments[i];
            const auto& name = m_attachmentNames[i];
            m_attachmentUsage[name] = {
                {}, {},
                attachment.initialLayout,
                attachment.finalLayout
            };
        }

        // 分析每个子流程的附件使用
        for (uint32_t subpassIndex = 0; subpassIndex < m_subpassBuilders.size(); ++subpassIndex) {
            const auto& subpassBuilder = m_subpassBuilders[subpassIndex];

            // 颜色附件 = 写入
            for (const auto& name : subpassBuilder.getColorAttachmentNames()) {
                m_attachmentUsage[name].writingSubpasses.insert(subpassIndex);
            }

            // 输入附件 = 读取
            for (const auto& name : subpassBuilder.getInputAttachmentNames()) {
                m_attachmentUsage[name].readingSubpasses.insert(subpassIndex);
            }

            // 深度模板附件 = 读写
            if (subpassBuilder.getDepthStencilAttachmentName()) {
                const auto& name = *subpassBuilder.getDepthStencilAttachmentName();
                m_attachmentUsage[name].writingSubpasses.insert(subpassIndex);
                m_attachmentUsage[name].readingSubpasses.insert(subpassIndex);
            }

            // 解析附件 = 写入
            for (const auto& name : subpassBuilder.getResolveAttachmentNames()) {
                m_attachmentUsage[name].writingSubpasses.insert(subpassIndex);
            }
        }
    }

    void RenderPassBuilder::generateDependenciesFromUsage() {
        m_autoDependencies.clear();

        // 为每个附件生成依赖
        for (const auto& [attachmentName, usage] : m_attachmentUsage) {
            generateDependenciesForAttachment(attachmentName, usage);
        }
    }

    void RenderPassBuilder::generateDependenciesForAttachment(const std::string& name, const AttachmentUsage& usage) {
        const auto& writers = usage.writingSubpasses;
        const auto& readers = usage.readingSubpasses;

        // 判断附件是否为深度模板
        bool isDepthStencil = false;
        auto it = m_attachmentIndices.find(name);
        if (it != m_attachmentIndices.end()) {
            uint32_t index = it->second;
            RHI::Format format = m_attachments[index].format;
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

        // 处理写入 -> 写入依赖（避免覆盖，如果写入区域可能重叠，则必须序列化）
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
        m_autoDependencies.push_back(dep);
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
        m_autoDependencies.push_back(dep);
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
        m_autoDependencies.push_back(dep);
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
        m_autoDependencies.push_back(dep);
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
            m_autoDependencies.push_back(dep);
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
            m_autoDependencies.push_back(dep);
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
        std::vector<RHI::SubpassDependency> result = m_manualDependencies;

        for (const auto& autoDep : m_autoDependencies) {
            bool duplicate = false;
            for (const auto& manualDep : m_manualDependencies) {
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

} // namespace StarryEngine::RenderGraph
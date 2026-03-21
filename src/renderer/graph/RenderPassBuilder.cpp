#include "RenderPassBuilder.hpp"
#include "SubpassBuilder.hpp"
#include <iostream>
#include <algorithm>

namespace StarryEngine::RenderGraph {

    RenderPassBuilder::RenderPassBuilder(std::string name)
        : m_name(std::move(name)) {
    }

    RenderPassBuilder& RenderPassBuilder::addAttachment(const std::string& key, const RHI::AttachmentDesc& attachment) {
        if (m_attachmentIndices.find(key) != m_attachmentIndices.end()) {
            throw std::runtime_error("Attachment with key '" + key + "' already exists");
        }

        uint32_t index = static_cast<uint32_t>(m_attachments.size());
        m_attachmentIndices[key] = index;
        m_attachmentNames.push_back(key);
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

    RenderPassBuilder& RenderPassBuilder::registerColorAttachment(const std::string& key,
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
        return addAttachment(key, attachment);
    }

    RenderPassBuilder& RenderPassBuilder::registerDepthAttachment(const std::string& key,
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
        return addAttachment(key, attachment);
    }

    RenderPassBuilder& RenderPassBuilder::registerResolveAttachment(const std::string& key,
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
        return addAttachment(key, attachment);
    }

    RenderPassBuilder& RenderPassBuilder::registerInputAttachment(const std::string& key,
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
        return addAttachment(key, attachment);
    }

    void RenderPassBuilder::updateAttachmentFormat(uint32_t index, RHI::Format newFormat) {
        if (index < m_attachments.size()) {
            m_attachments[index].format = newFormat;
        }
        else {
            throw std::runtime_error("Invalid attachment index");
        }
    }

    std::unique_ptr<RenderPassBuildResult> RenderPassBuilder::build(bool autoDependencies) {
        analyzeAttachmentUsage();

        if (autoDependencies) {
            generateDependenciesFromUsage();
        }

        auto result = std::make_unique<RenderPassBuildResult>();
        result->name = m_name;
        result->attachmentNames = m_attachmentNames;
        result->attachmentNameToIndex = m_attachmentIndices;
        result->renderPassDesc.attachments = m_attachments;

        for (uint32_t subpassIndex = 0; subpassIndex < m_subpassBuilders.size(); ++subpassIndex) {
            auto& subpassBuilder = m_subpassBuilders[subpassIndex];

            for (const auto& key : subpassBuilder.getColorAttachmentNames()) {
                if (m_attachmentIndices.find(key) == m_attachmentIndices.end()) {
                    throw std::runtime_error("Color attachment key '" + key + "' not found");
                }
            }
            for (const auto& key : subpassBuilder.getInputAttachmentNames()) {
                if (m_attachmentIndices.find(key) == m_attachmentIndices.end()) {
                    throw std::runtime_error("Input attachment key '" + key + "' not found");
                }
            }
            if (subpassBuilder.getDepthStencilAttachmentName()) {
                const auto& key = *subpassBuilder.getDepthStencilAttachmentName();
                if (m_attachmentIndices.find(key) == m_attachmentIndices.end()) {
                    throw std::runtime_error("Depth/stencil attachment key '" + key + "' not found");
                }
            }

            result->renderPassDesc.subpasses.push_back(subpassBuilder.buildSubpassDesc(m_attachmentIndices));
        }

        result->renderPassDesc.dependencies = mergeDependencies();

        for (const auto& subpassBuilder : m_subpassBuilders) {
            result->subpassRecorders.push_back(subpassBuilder.getRecorder());
        }

        return result;
    }

    void RenderPassBuilder::analyzeAttachmentUsage() {
        m_attachmentUsage.clear();

        for (uint32_t i = 0; i < m_attachments.size(); ++i) {
            const auto& attachment = m_attachments[i];
            const auto& key = m_attachmentNames[i];
            m_attachmentUsage[key] = { {}, {}, attachment.initialLayout, attachment.finalLayout };
        }

        for (uint32_t subpassIndex = 0; subpassIndex < m_subpassBuilders.size(); ++subpassIndex) {
            const auto& subpassBuilder = m_subpassBuilders[subpassIndex];

            for (const auto& key : subpassBuilder.getColorAttachmentNames()) {
                m_attachmentUsage[key].writingSubpasses.insert(subpassIndex);
            }
            for (const auto& key : subpassBuilder.getInputAttachmentNames()) {
                m_attachmentUsage[key].readingSubpasses.insert(subpassIndex);
            }
            if (subpassBuilder.getDepthStencilAttachmentName()) {
                const auto& key = *subpassBuilder.getDepthStencilAttachmentName();
                m_attachmentUsage[key].writingSubpasses.insert(subpassIndex);
                m_attachmentUsage[key].readingSubpasses.insert(subpassIndex);
            }
            for (const auto& key : subpassBuilder.getResolveAttachmentNames()) {
                m_attachmentUsage[key].writingSubpasses.insert(subpassIndex);
            }
        }
    }

    void RenderPassBuilder::generateDependenciesFromUsage() {
        m_autoDependencies.clear();
        for (const auto& [key, usage] : m_attachmentUsage) {
            generateDependenciesForAttachment(key, usage);
        }
    }

    void RenderPassBuilder::generateDependenciesForAttachment(const std::string& key, const AttachmentUsage& usage) {
        const auto& writers = usage.writingSubpasses;
        const auto& readers = usage.readingSubpasses;

        bool isDepthStencil = false;
        auto it = m_attachmentIndices.find(key);
        if (it != m_attachmentIndices.end()) {
            uint32_t index = it->second;
            RHI::Format format = m_attachments[index].format;
            isDepthStencil = isDepthStencilFormat(format);
        }

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
            if (!duplicate) result.push_back(autoDep);
        }
        return result;
    }

} // namespace StarryEngine::RenderGraph
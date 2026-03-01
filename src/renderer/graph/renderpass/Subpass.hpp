#pragma once
#include<iostream>
#include <memory>
#include <vector>
#include <set>
#include <optional>
#include <stdexcept>
#include <limits>
#include "../../interface/RHI_ENUMS.hpp"
#include "../../interface/RHI_HANDLES_SYSTEM.hpp"
#include "../../interface/RHI_STRUCTS_DESC.hpp"
#include "../../subpassRenderer/ISubpassRenderer.hpp"

namespace StarryEngine::RenderGraph {
    static constexpr uint32_t SUBPASS_EXTERNAL = ~0U;
    static constexpr uint32_t SUBPASS_MAX_ENUM = 0x7FFFFFFF;
    constexpr uint32_t ATTACHMENT_UNUSED = std::numeric_limits<uint32_t>::max();

    class Subpass {
    public:
        Subpass();
        ~Subpass();

        // 添加附件引用（直接传入 RHI::AttachmentReference）
        Subpass& addInputAttachmentRef(const RHI::AttachmentReference& ref);
        Subpass& addColorAttachmentRef(const RHI::AttachmentReference& ref);
        Subpass& addResolveAttachmentRef(const RHI::AttachmentReference& ref);
        Subpass& setDepthStencilAttachmentRef(const RHI::AttachmentReference& ref);

        // 通过索引和布局添加附件引用
        Subpass& addColorAttachmentRef(uint32_t attachmentIndex,
            RHI::ImageLayout layout = RHI::ImageLayout::ColorAttachment);
        Subpass& addInputAttachmentRef(uint32_t attachmentIndex,
            RHI::ImageLayout layout = RHI::ImageLayout::ShaderReadOnly);
        Subpass& addPreserveAttachmentRef(uint32_t attachmentIndex);

        RHI::SubpassDesc build() const;

    private:
        std::vector<RHI::AttachmentReference> mInputAttachments;
        std::vector<RHI::AttachmentReference> mColorAttachments;
        std::vector<RHI::AttachmentReference> mResolveAttachments;
        std::vector<uint32_t> mPreserveAttachments;
        RHI::AttachmentReference mDepthStencilAttachment;
    };

    class SubpassBuilder {
    public:
        explicit SubpassBuilder(std::string name);
        ~SubpassBuilder() = default;

        // 使用字符串名称添加附件
        SubpassBuilder& addColorAttachment(const std::string& name,
            RHI::ImageLayout layout = RHI::ImageLayout::ColorAttachment);
        SubpassBuilder& addInputAttachment(const std::string& name,
            RHI::ImageLayout layout = RHI::ImageLayout::ShaderReadOnly);
        SubpassBuilder& addResolveAttachment(const std::string& name,
            RHI::ImageLayout layout = RHI::ImageLayout::ColorAttachment);
        SubpassBuilder& setDepthStencilAttachment(const std::string& name,
            RHI::ImageLayout layout = RHI::ImageLayout::DepthStencilAttachment);
        SubpassBuilder& setPipelineName(const std::string& pipelineName);
        SubpassBuilder& addPreserveAttachment(const std::string& name);

        SubpassBuilder& setPipelineDescription(const RHI::GraphicsPipelineDesc& desc) {
            m_pipelineDesc = desc;
            return *this;
        }

        SubpassBuilder& setRenderer(ISubpassRenderer* renderer) {
            std::cout << "[SubpassBuilder::setRenderer] renderer = " << renderer << std::endl;
            m_renderer = renderer;
            return *this;
        }

        ISubpassRenderer* getRenderer() const{ return m_renderer; }

        const RHI::GraphicsPipelineDesc& getPipelineDescription() const { return m_pipelineDesc; }

        // 构建 Subpass 对象（需要名称到索引的映射）
        std::unique_ptr<Subpass> build(const std::unordered_map<std::string, uint32_t>& nameToIndexMap) const;

        // 获取附件名称列表（用于验证）
        const std::vector<std::string>& getColorAttachmentNames() const { return mColorAttachmentNames; }
        const std::vector<std::string>& getInputAttachmentNames() const { return mInputAttachmentNames; }
        const std::vector<std::string>& getResolveAttachmentNames() const { return mResolveAttachmentNames; }
        const std::optional<std::string>& getDepthStencilAttachmentName() const { return mDepthStencilAttachmentName; }
        const std::vector<std::string>& getPreserveAttachmentNames() const { return mPreserveAttachmentNames; }
        const std::string& getPipelineName() const { return mPipelineName; }
        const std::string& getSubpassName() const { return mSubpassName; }

    private:
        std::string mSubpassName;
        std::string mPipelineName;

        std::vector<std::string> mColorAttachmentNames;
        std::vector<std::string> mInputAttachmentNames;
        std::vector<std::string> mResolveAttachmentNames;
        std::vector<std::string> mPreserveAttachmentNames;
        std::optional<std::string> mDepthStencilAttachmentName;
        RHI::GraphicsPipelineDesc m_pipelineDesc;
        ISubpassRenderer* m_renderer = nullptr;

        std::unordered_map<std::string, RHI::ImageLayout> mAttachmentLayouts;
    };

}
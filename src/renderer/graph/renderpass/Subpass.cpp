#include"Subpass.hpp"

namespace StarryEngine::RenderGraph {
    Subpass::Subpass() : mDepthStencilAttachment(ATTACHMENT_UNUSED, RHI::ImageLayout::Undefined) {}

    Subpass::~Subpass() {}

    Subpass& Subpass::addInputAttachmentRef(const RHI::AttachmentReference& ref) {
        mInputAttachments.push_back(ref);
        return *this;
    }

    Subpass& Subpass::addColorAttachmentRef(const RHI::AttachmentReference& ref) {
        mColorAttachments.push_back(ref);
        return *this;
    }

    Subpass& Subpass::addResolveAttachmentRef(const RHI::AttachmentReference& ref) {
        mResolveAttachments.push_back(ref);
        return *this;
    }

    Subpass& Subpass::setDepthStencilAttachmentRef(const RHI::AttachmentReference& ref) {
        mDepthStencilAttachment = ref;
        return *this;
    }

    Subpass& Subpass::addColorAttachmentRef(uint32_t attachmentIndex, RHI::ImageLayout layout) {
        RHI::AttachmentReference ref{ attachmentIndex, layout };
        return addColorAttachmentRef(ref);
    }

    Subpass& Subpass::addInputAttachmentRef(uint32_t attachmentIndex, RHI::ImageLayout layout) {
        RHI::AttachmentReference ref{ attachmentIndex, layout };
        return addInputAttachmentRef(ref);
    }

    Subpass& Subpass::addPreserveAttachmentRef(uint32_t attachmentIndex) {
        mPreserveAttachments.push_back(attachmentIndex);
        return *this;
    }

    RHI::SubpassDesc Subpass::build() const {
        if (mColorAttachments.empty() && mInputAttachments.empty()) {
            throw std::runtime_error("Subpass must have at least one color or input attachment.");
        }

        RHI::SubpassDesc desc;
        desc.inputAttachments = mInputAttachments;
        desc.colorAttachments = mColorAttachments;
        desc.resolveAttachments = mResolveAttachments;
        desc.depthStencilAttachment = mDepthStencilAttachment; // 直接赋值
        desc.preserveAttachments = mPreserveAttachments;
        return desc;
    }

    SubpassBuilder::SubpassBuilder(std::string name)
        : mSubpassName(std::move(name)) {
    }

    SubpassBuilder& SubpassBuilder::addColorAttachment(const std::string& name, RHI::ImageLayout layout) {
        mColorAttachmentNames.push_back(name);
        mAttachmentLayouts[name] = layout;
        return *this;
    }

    SubpassBuilder& SubpassBuilder::addInputAttachment(const std::string& name, RHI::ImageLayout layout) {
        mInputAttachmentNames.push_back(name);
        mAttachmentLayouts[name] = layout;
        return *this;
    }

    SubpassBuilder& SubpassBuilder::addResolveAttachment(const std::string& name, RHI::ImageLayout layout) {
        mResolveAttachmentNames.push_back(name);
        mAttachmentLayouts[name] = layout;
        return *this;
    }

    SubpassBuilder& SubpassBuilder::addPreserveAttachment(const std::string& name) {
        mPreserveAttachmentNames.push_back(name);
        return *this;
    }

    SubpassBuilder& SubpassBuilder::setDepthStencilAttachment(const std::string& name, RHI::ImageLayout layout) {
        mDepthStencilAttachmentName = name;
        mAttachmentLayouts[name] = layout;
        return *this;
    }

    SubpassBuilder& SubpassBuilder::setPipelineName(const std::string& pipelineName) {
        mPipelineName = pipelineName;
        return *this;
    }

    std::unique_ptr<Subpass> SubpassBuilder::build(const std::unordered_map<std::string, uint32_t>& nameToIndexMap) const {

        auto subpass = std::make_unique<Subpass>();

        // 处理颜色附件
        for (const auto& name : mColorAttachmentNames) {
            auto it = nameToIndexMap.find(name);
            if (it == nameToIndexMap.end()) {
                throw std::runtime_error("Color attachment not found in index map: " + name);
            }
            auto layoutIt = mAttachmentLayouts.find(name);
            if (layoutIt == mAttachmentLayouts.end()) {
                throw std::runtime_error("Layout missing for color attachment: " + name);
            }
            subpass->addColorAttachmentRef(it->second, layoutIt->second);
        }

        // 处理输入附件
        for (const auto& name : mInputAttachmentNames) {
            auto it = nameToIndexMap.find(name);
            if (it == nameToIndexMap.end()) {
                throw std::runtime_error("Input attachment not found in index map: " + name);
            }
            auto layoutIt = mAttachmentLayouts.find(name);
            if (layoutIt == mAttachmentLayouts.end()) {
                throw std::runtime_error("Layout missing for input attachment: " + name);
            }
            subpass->addInputAttachmentRef(it->second, layoutIt->second);
        }

        // 处理解析附件
        for (const auto& name : mResolveAttachmentNames) {
            auto it = nameToIndexMap.find(name);
            if (it == nameToIndexMap.end()) {
                throw std::runtime_error("Resolve attachment not found in index map: " + name);
            }
            auto layoutIt = mAttachmentLayouts.find(name);
            if (layoutIt == mAttachmentLayouts.end()) {
                throw std::runtime_error("Layout missing for resolve attachment: " + name);
            }
            RHI::AttachmentReference ref{ it->second, layoutIt->second };
            subpass->addResolveAttachmentRef(ref);
        }

        // 处理深度模板附件
        if (mDepthStencilAttachmentName) {
            auto it = nameToIndexMap.find(*mDepthStencilAttachmentName);
            if (it == nameToIndexMap.end()) {
                throw std::runtime_error("Depth/stencil attachment not found in index map: " + *mDepthStencilAttachmentName);
            }
            auto layoutIt = mAttachmentLayouts.find(*mDepthStencilAttachmentName);
            if (layoutIt == mAttachmentLayouts.end()) {
                throw std::runtime_error("Layout missing for depth/stencil attachment: " + *mDepthStencilAttachmentName);
            }
            RHI::AttachmentReference ref{ it->second, layoutIt->second };
            subpass->setDepthStencilAttachmentRef(ref);
        }

        // 处理保留附件
        for (const auto& name : mPreserveAttachmentNames) {
            auto it = nameToIndexMap.find(name);
            if (it == nameToIndexMap.end()) {
                throw std::runtime_error("Preserve attachment not found in index map: " + name);
            }
            subpass->addPreserveAttachmentRef(it->second);
        }

        return subpass;
    }
}
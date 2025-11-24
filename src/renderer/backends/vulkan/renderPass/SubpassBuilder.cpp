#include "SubpassBuilder.hpp"
#include <stdexcept>

namespace StarryEngine::Builder {

    SubpassBuilder& SubpassBuilder::addColorAttachment(const std::string& name, SubpassAttachFormat format) {
        SubpassAttachmentInfo info;
        info.name = name;
        info.format = format;
        info.layout = getDefaultLayout(format);
        mColorAttachments.push_back(info);
        return *this;
    }

    SubpassBuilder& SubpassBuilder::addResolveAttachment(const std::string& name, SubpassAttachFormat format) {
        SubpassAttachmentInfo info;
        info.name = name;
        info.format = format;
        // Resolve 附件通常与颜色附件使用相同的布局
        info.layout = getDefaultLayout(format);
        mResolveAttachments.push_back(info);
        return *this;
    }

    SubpassBuilder& SubpassBuilder::setDepthStencilAttachment(const std::string& name, SubpassAttachFormat format) {
        SubpassAttachmentInfo info;
        info.name = name;
        info.format = format;
        // 深度模板附件由 getDefaultLayout 识别为深度布局
        info.layout = getDefaultLayout(format);
        mDepthStencilAttachment = info;
        return *this;
    }

    SubpassBuilder& SubpassBuilder::addPreserveAttachment(const std::string& name) {
        mPreserveAttachments.push_back(name);
        return *this;
    }

    SubpassBuilder& SubpassBuilder::addInputAttachment(const std::string& name, SubpassAttachFormat format) {
        SubpassAttachmentInfo info;
        info.name = name;
        info.format = format;
        info.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        mInputAttachments.push_back(info);
        return *this;
    }

    SubpassBuilder& SubpassBuilder::setAttachmentIndex(const std::string& name, uint32_t index) {
        mNameToIndexMap[name] = index;
        return *this;
    }

    std::unique_ptr<Subpass> SubpassBuilder::build(VkPipelineBindPoint bindPoint) {
        auto subpass = std::make_unique<Subpass>();

        // 构建颜色附件
        for (const auto& color : mColorAttachments) {
            auto it = mNameToIndexMap.find(color.name);
            if (it != mNameToIndexMap.end()) {
                subpass->addColorAttachmentRef(it->second, color.layout);
            }
        }

        // 构建输入附件
        for (const auto& input : mInputAttachments) {
            auto it = mNameToIndexMap.find(input.name);
            if (it != mNameToIndexMap.end()) {
                subpass->addInputAttachmentRef(it->second, input.layout);
            }
        }

        // 构建深度模板附件
        if (mDepthStencilAttachment) {
            auto it = mNameToIndexMap.find(mDepthStencilAttachment->name);
            if (it != mNameToIndexMap.end()) {
                VkAttachmentReference ref{};
                ref.attachment = it->second;
                ref.layout = mDepthStencilAttachment->layout;
                subpass->setDepthStencilAttachmentRef(ref);
            }
        }

        // 构建保留附件
        for (const auto& preserve : mPreserveAttachments) {
            auto it = mNameToIndexMap.find(preserve);
            if (it != mNameToIndexMap.end()) {
                subpass->addPreserveAttachmentRef(it->second);
            }
        }

        subpass->biuldSubpassDescription(bindPoint);
        return subpass;
    }

    VkImageLayout SubpassBuilder::getDefaultLayout(SubpassAttachFormat format, bool isInput) {
        // 根据格式和用途返回默认布局
        switch (format) {
        case SubpassAttachFormat::DEPTH16:
        case SubpassAttachFormat::DEPTH32F:
        case SubpassAttachFormat::DEPTH24STENCIL8:
        case SubpassAttachFormat::DEPTH32FSTENCIL8:
            return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        default:
            return isInput ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        }
    }

} // namespace StarryEngine::Builder
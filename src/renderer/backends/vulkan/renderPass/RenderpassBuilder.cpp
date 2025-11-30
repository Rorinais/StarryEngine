#include "RenderPassBuilder.hpp"
#include <stdexcept>

namespace StarryEngine {

    RenderPassBuilder::RenderPassBuilder(std::shared_ptr<LogicalDevice> logicalDevice)
        : mLogicalDevice(logicalDevice) {
    }

    RenderPassBuilder& RenderPassBuilder::addAttachment(const std::string& name, const VkAttachmentDescription& attachment) {
        // 检查名称是否已存在
        if (mAttachmentIndices.find(name) != mAttachmentIndices.end()) {
            throw std::runtime_error("Attachment with name '" + name + "' already exists");
        }

        // 分配新索引
        uint32_t index = static_cast<uint32_t>(mAttachments.size());
        mAttachmentIndices[name] = index;
        mAttachmentNames.push_back(name);
        mAttachments.push_back(attachment);

        return *this;
    }

    RenderPassBuilder& RenderPassBuilder::addSubpass(SubpassBuilder& subpassBuilder) {
        mSubpassBuilders.push_back(std::move(subpassBuilder));
        return *this;
    }

    RenderPassBuilder& RenderPassBuilder::addDependency(const VkSubpassDependency& dependency) {
        mManualDependencies.push_back(dependency);
        return *this;
    }

    RenderPassBuilder& RenderPassBuilder::addColorAttachment(const std::string& name,
        VkFormat format,
        VkImageLayout finalLayout,
        VkAttachmentLoadOp loadOp,
        VkAttachmentStoreOp storeOp) {

        VkAttachmentDescription attachment{};
        attachment.format = format;
        attachment.samples = VK_SAMPLE_COUNT_1_BIT;
        attachment.loadOp = loadOp;
        attachment.storeOp = storeOp;
        attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attachment.finalLayout = finalLayout;

        return addAttachment(name, attachment);
    }

    RenderPassBuilder& RenderPassBuilder::addDepthAttachment(const std::string& name,
        VkFormat format,
        VkAttachmentLoadOp loadOp,
        VkAttachmentStoreOp storeOp) {

        VkAttachmentDescription attachment{};
        attachment.format = format;
        attachment.samples = VK_SAMPLE_COUNT_1_BIT;
        attachment.loadOp = loadOp;
        attachment.storeOp = storeOp;
        attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        return addAttachment(name, attachment);
    }

    RenderPassBuilder& RenderPassBuilder::addResolveAttachment(const std::string& name,
        VkFormat format,
        VkImageLayout finalLayout) {

        VkAttachmentDescription attachment{};
        attachment.format = format;
        attachment.samples = VK_SAMPLE_COUNT_1_BIT;
        attachment.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
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

        // 创建 RenderPass 对象
        auto renderPass = std::make_unique<RenderPass>(mLogicalDevice);

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
            if (subpassBuilder.getDepthStencilAttachmentName() &&
                mAttachmentIndices.find(*subpassBuilder.getDepthStencilAttachmentName()) == mAttachmentIndices.end()) {
                throw std::runtime_error("Depth/stencil attachment '" + *subpassBuilder.getDepthStencilAttachmentName() + "' not found in render pass");
            }

            // 构建子流程
            auto subpass = subpassBuilder.build(mAttachmentIndices);
            renderPass->addSubpass(*subpass);

            result->pipelineNameToSubpassIndexMap[subpassBuilder.getPipelineName()] = subpassIndex;
        }

        // 合并依赖（手动 + 自动，自动去重）
        auto finalDependencies = mergeDependencies();
        for (const auto& dependency : finalDependencies) {
            renderPass->addDependency(dependency);
        }

        // 构建 RenderPass
        renderPass->buildRenderPass();

        // 设置结果
        result->renderPass = std::move(renderPass);
        return result;
    }

    // 新的附件使用分析 - 基于字符串名称
    void RenderPassBuilder::analyzeAttachmentUsage() {
        mAttachmentUsage.clear();

        // 初始化附件使用信息
        for (uint32_t i = 0; i < mAttachments.size(); ++i) {
            const auto& attachment = mAttachments[i];
            const auto& name = mAttachmentNames[i];
            mAttachmentUsage[name] = {
                {}, {},  // writingSubpasses, readingSubpasses
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

    // 新的依赖推导方法 - 基于字符串名称和读写关系
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

        // 判断附件类型
        bool isDepthStencil = false;
        auto it = mAttachmentIndices.find(name);
        if (it != mAttachmentIndices.end()) {
            uint32_t index = it->second;
            VkFormat format = mAttachments[index].format;
            // 简单的深度格式判断
            isDepthStencil = (format == VK_FORMAT_D16_UNORM ||
                format == VK_FORMAT_D32_SFLOAT ||
                format == VK_FORMAT_D24_UNORM_S8_UINT ||
                format == VK_FORMAT_D32_SFLOAT_S8_UINT);
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
        VkSubpassDependency dep{};
        dep.srcSubpass = src;
        dep.dstSubpass = dst;
        dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dep.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        dep.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        dep.dstAccessMask = VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
        dep.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
        mAutoDependencies.push_back(dep);
    }

    void RenderPassBuilder::addDepthReadAfterWriteDependency(uint32_t src, uint32_t dst) {
        VkSubpassDependency dep{};
        dep.srcSubpass = src;
        dep.dstSubpass = dst;
        dep.srcStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        dep.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        dep.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        dep.dstAccessMask = VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
        dep.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
        mAutoDependencies.push_back(dep);
    }

    void RenderPassBuilder::addColorWriteAfterWriteDependency(uint32_t first, uint32_t second) {
        VkSubpassDependency dep{};
        dep.srcSubpass = first;
        dep.dstSubpass = second;
        dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dep.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        dep.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
        mAutoDependencies.push_back(dep);
    }

    void RenderPassBuilder::addDepthWriteAfterWriteDependency(uint32_t first, uint32_t second) {
        VkSubpassDependency dep{};
        dep.srcSubpass = first;
        dep.dstSubpass = second;
        dep.srcStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        dep.dstStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        dep.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        dep.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        dep.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
        mAutoDependencies.push_back(dep);
    }

    void RenderPassBuilder::generateExternalDependencies(const AttachmentUsage& usage, bool isDepthStencil) {
        // 找出第一个使用者
        uint32_t firstUser = VK_SUBPASS_EXTERNAL;
        if (!usage.writingSubpasses.empty()) {
            firstUser = *usage.writingSubpasses.begin();
        }
        else if (!usage.readingSubpasses.empty()) {
            firstUser = *usage.readingSubpasses.begin();
        }

        // 从外部到第一个使用者
        if (firstUser != VK_SUBPASS_EXTERNAL && firstUser > 0) {
            VkSubpassDependency dep{};
            dep.srcSubpass = VK_SUBPASS_EXTERNAL;
            dep.dstSubpass = firstUser;
            if (isDepthStencil) {
                dep.srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
                dep.dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
                dep.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
                dep.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            }
            else {
                dep.srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
                dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
                dep.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
                dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            }
            dep.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
            mAutoDependencies.push_back(dep);
        }

        // 找出最后一个使用者
        uint32_t lastUser = VK_SUBPASS_EXTERNAL;
        if (!usage.readingSubpasses.empty()) {
            lastUser = *usage.readingSubpasses.rbegin();
        }
        if (!usage.writingSubpasses.empty()) {
            uint32_t lastWriter = *usage.writingSubpasses.rbegin();
            if (lastUser == VK_SUBPASS_EXTERNAL || lastWriter > lastUser) {
                lastUser = lastWriter;
            }
        }

        // 从最后一个使用者到外部
        if (lastUser != VK_SUBPASS_EXTERNAL && lastUser < mSubpassBuilders.size() - 1) {
            VkSubpassDependency dep{};
            dep.srcSubpass = lastUser;
            dep.dstSubpass = VK_SUBPASS_EXTERNAL;
            if (isDepthStencil) {
                dep.srcStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
                dep.dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
                dep.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
                dep.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
            }
            else {
                dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
                dep.dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
                dep.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
                dep.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
            }
            dep.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
            mAutoDependencies.push_back(dep);
        }
    }

    void RenderPassBuilder::generateExecutionDependencies() {
        // 确保子流程按顺序执行
        for (uint32_t i = 0; i < mSubpassBuilders.size() - 1; ++i) {
            VkSubpassDependency dep{};
            dep.srcSubpass = i;
            dep.dstSubpass = i + 1;
            dep.srcStageMask = VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT;
            dep.dstStageMask = VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT;
            dep.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
            dep.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
            mAutoDependencies.push_back(dep);
        }
    }

    // 依赖合并方法（避免重复）
    std::vector<VkSubpassDependency> RenderPassBuilder::mergeDependencies() {
        std::vector<VkSubpassDependency> result = mManualDependencies;

        // 添加自动推导的依赖，但排除与手动依赖重复的
        for (const auto& autoDep : mAutoDependencies) {
            bool isDuplicate = false;
            for (const auto& manualDep : mManualDependencies) {
                if (manualDep.srcSubpass == autoDep.srcSubpass &&
                    manualDep.dstSubpass == autoDep.dstSubpass &&
                    manualDep.srcStageMask == autoDep.srcStageMask &&
                    manualDep.dstStageMask == autoDep.dstStageMask) {
                    isDuplicate = true;
                    break;
                }
            }
            if (!isDuplicate) {
                result.push_back(autoDep);
            }
        }

        return result;
    }

} // namespace StarryEngine
#include <interface/vulkan/VulkanConversion.hpp>
#include <interface/vulkan/VulkanResources.hpp>
#include <logging/Logger.hpp>

namespace StarryEngine::RHI {
    VulkanShaderModule::VulkanShaderModule(std::shared_ptr<VulkanDevice> device, ShaderModuleDesc desc)
        : mDevice(device), mDesc(desc), mShaderModule(VK_NULL_HANDLE) {

        try {
            mShaderModule = mDevice->createShaderModule(mDesc.code, mDesc.debugName);
        }
        catch (const std::exception& e) {
            std::cerr << "[VulkanShaderModule] ERROR: Failed to create shader: "<< mDesc.debugName << " - " << e.what() << std::endl;

            mShaderModule = VK_NULL_HANDLE;
            throw;
        }
    }

    void VulkanShaderModule::release() {
        mDevice->destroyShaderModule(mShaderModule);
    }

    //VulkanBuffer
    // ==================== 构造函数和析构函数 ====================
    VulkanBuffer::VulkanBuffer(std::shared_ptr<VulkanDevice>  device, const BufferDesc& desc)
        : mDevice(device)
        , mDesc(desc)
        , mBuffer(VK_NULL_HANDLE)
        , mVmaAllocation(VK_NULL_HANDLE)  // VMA分配
        , mTraditionalMemory(VK_NULL_HANDLE)  // 传统内存
        , mUsingVMA(device->isVMAEnabled())
        , mMappedPointer(nullptr)
        , mIsMapped(false)
        , mPersistentlyMapped(false)
        , mNextViewKey(1)
        , mCurrentAccess(AccessFlag::None)
        , mCurrentStage(PipelineStage::TopOfPipe) {
        createBuffer();
    }

    VulkanBuffer::~VulkanBuffer() {
        release();
    }

    // ==================== 缓冲区创建和销毁 ====================
    void VulkanBuffer::createBuffer() {
        VkBufferUsageFlags usage = getBufferUsageFlags();
        
        try {
            if (mUsingVMA) {
                // VMA方式
                VmaMemoryUsage vmaUsage = func::RHI_TO_VK_VmaMemoryUsage(mDesc.memoryType);
                VmaAllocationCreateFlags flags = 0;
                
                // 持久映射标志
                if (mDesc.persistentMapped && isCPUVisible()) {
                    flags |= VMA_ALLOCATION_CREATE_MAPPED_BIT;
                    mPersistentlyMapped = true;
                }
                
                // 使用Device的统一接口
                VMABuffer vmaBuffer = mDevice->createBufferWithVMA(
                    mDesc.size,
                    usage,
                    vmaUsage,
                    flags,
                    mDesc.initialData,
                    mDesc.initialDataSize
                );
                
                mBuffer = vmaBuffer.buffer;
                mVmaAllocation = vmaBuffer.allocation;
            } else {
                // 传统方式
                VkMemoryPropertyFlags memoryProperties = func::RHI_TO_VK_MemoryProperties(mDesc.memoryType);
                
                // 获取传输命令池（如果需要）
                VkCommandPool commandPool = VK_NULL_HANDLE;
                if (mDesc.initialData && mDesc.initialDataSize > 0 && !isCPUVisible()) {
                    commandPool = mDevice->getTransferCommandPool();
                }
                
                // 使用更新后的createBufferTraditional，自动处理初始数据
                VMATraditionalBuffer traditionalBuffer = mDevice->createBufferTraditional(
                    mDesc.size,
                    usage,
                    memoryProperties,
                    mDesc.initialData,
                    mDesc.initialDataSize,
                    commandPool  
                );
                
                mBuffer = traditionalBuffer.buffer;
                mTraditionalMemory = traditionalBuffer.memory;
                
                // 设置持久映射
                if (mDesc.persistentMapped && isCPUVisible()) {
                    mPersistentlyMapped = true;
                    map(); // 立即映射
                }
            }
            
            // 设置调试名称
            if (!mDesc.debugName.empty()) {
                mDevice->setBufferName(mBuffer, mDesc.debugName.c_str());
            }
            
        } catch (const std::exception& e) {
            std::cerr << "[VulkanBuffer] Failed to create buffer: " << e.what() << std::endl;
            throw;
        }
    }

    // ==================== 数据更新优化 ====================
    bool VulkanBuffer::update(const void* data, uint64_t size, uint64_t offset) {
        if (!data || size == 0) {
            //LOG_WARN("Buffer update called with null data or zero size");
            return false;
        }

        // 检查边界
        if (offset + size > mDesc.size) {
            //LOG_ERROR("Buffer update exceeds buffer size: offset={}, size={}, total={}", offset, size, mDesc.size);
            return false;
        }

        // 根据内存类型选择合适的更新方式
        if (isCPUVisible()) {
            return updateDataViaDirectMapping(data, size, offset);
        }
        else {
            return updateDataViaStagingBuffer(data, size, offset);
        }
    }

    bool  VulkanBuffer::updateDataViaDirectMapping(const void* data, uint64_t size, uint64_t offset) {
        if (mPersistentlyMapped && mIsMapped) {
            memcpy(static_cast<uint8_t*>(mMappedPointer) + offset, data, size);
            flush(offset, size);
            return true;
        }
        
        // 否则使用RAII包装器进行临时映射
        if (mUsingVMA) {
            mDevice->uploadDataToVmaBuffer(mBuffer, mVmaAllocation, data, size, offset);
        } else {
            bool hostCoherent = (func::RHI_TO_VK_MemoryProperties(mDesc.memoryType) & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) != 0;
            mDevice->uploadDataToTraditionalMemory(mTraditionalMemory, data, size, offset, hostCoherent);
        }

        return true;
    }

    bool VulkanBuffer::updateDataViaStagingBuffer(const void* data, uint64_t size, uint64_t offset) {
        // 创建暂存缓冲区
        VMATraditionalBuffer stagingBuffer = mDevice->createBufferTraditional(
            size,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
        );

        if (stagingBuffer.buffer == VK_NULL_HANDLE || stagingBuffer.memory == VK_NULL_HANDLE) {
            //LOG_ERROR("Failed to create staging buffer for update");
            return false;
        }
        
        // 上传数据到暂存缓冲区
        mDevice->uploadDataToTraditionalMemory(stagingBuffer.memory, data, size, 0, true);
        
        // 获取传输命令池并复制
        VkCommandPool transferPool = mDevice->getTransferCommandPool();
        mDevice->copyBuffer(transferPool, stagingBuffer.buffer, mBuffer, size);
        
        // 清理
        mDevice->destroyBufferTraditional(stagingBuffer);

        return true;
    }

    // ==================== 内存映射优化 ====================
    void* VulkanBuffer::map(uint64_t offset, uint64_t size) {
        if (mIsMapped) {
            // 已经映射，返回偏移后的指针
            return static_cast<uint8_t*>(mMappedPointer) + offset;
        }
        
        if (size == 0) {
            size = mDesc.size - offset;
        }
        
        if (mUsingVMA) {
            // VMA映射
            VkResult result = vmaMapMemory(mDevice->getVmaAllocator(), mVmaAllocation, &mMappedPointer);
            if (result != VK_SUCCESS) {
                std::cerr << "[VulkanBuffer] Failed to map VMA memory: " << result << std::endl;
                return nullptr;
            }
        } else {
            // 传统映射
            VkResult result = vkMapMemory(mDevice->getLogicalDevice(), mTraditionalMemory, 
                                        offset, size, 0, &mMappedPointer);
            if (result != VK_SUCCESS) {
                std::cerr << "[VulkanBuffer] Failed to map traditional memory: " << result << std::endl;
                return nullptr;
            }
        }
        
        mIsMapped = true;
        return mMappedPointer;
    }

    void VulkanBuffer::unmap() {
        if (!mIsMapped) return;
        
        if (mUsingVMA) {
            vmaUnmapMemory(mDevice->getVmaAllocator(), mVmaAllocation);
        } else {
            vkUnmapMemory(mDevice->getLogicalDevice(), mTraditionalMemory);
        }
        
        mMappedPointer = nullptr;
        mIsMapped = false;
    }

    // ==================== 简化辅助函数 ====================
    VkBufferUsageFlags VulkanBuffer::getBufferUsageFlags() const {
        VkBufferUsageFlags usage = 0;
        
        // 根据BufferType设置基础用途
        static const std::unordered_map<BufferType, VkBufferUsageFlags> typeFlags = {
            {BufferType::Vertex, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT},
            {BufferType::Index, VK_BUFFER_USAGE_INDEX_BUFFER_BIT},
            {BufferType::Uniform, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT},
            {BufferType::Storage, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT},
            {BufferType::Indirect, VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT},
        };
        
        if (auto it = typeFlags.find(mDesc.type); it != typeFlags.end()) {
            usage |= it->second;
        }
        if (mDesc.memoryType == MemoryType::GPU_Only) {
            usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        }

        // 根据描述符添加额外标志
        if (mDesc.allowRawViews) {
            usage |= VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT | 
                    VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT;
        }
        
        if (mDesc.allowShaderAtomics) {
            usage |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        }
        
        // 如果需要传输，添加传输标志
        if (mDesc.memoryType == MemoryType::CPU_To_GPU || 
            mDesc.memoryType == MemoryType::GPU_To_CPU) {
            usage |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        }
        
        return usage;
    }

    bool VulkanBuffer::isCPUVisible() const {
        switch (mDesc.memoryType) {
            case MemoryType::CPU_To_GPU:
            case MemoryType::CPU_Only:
            case MemoryType::GPU_To_CPU:
                return true;
            case MemoryType::GPU_Only:
            default:
                return false;
        }
    }

    bool VulkanBuffer::isGPUOnly() const {
        return mDesc.memoryType == MemoryType::GPU_Only;
    }

    // ==================== 视图管理 ====================
    void* VulkanBuffer::createView(Format format, uint64_t offset, uint64_t size) {
        if (size == 0) {
            size = mDesc.size - offset;
        }
        
        VkBufferViewCreateInfo viewInfo = {};
        viewInfo.sType = VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO;
        viewInfo.buffer = mBuffer;
        viewInfo.format = func::RHI_TO_VK_Format(format);
        viewInfo.offset = offset;
        viewInfo.range = size;
        
        VkBufferView bufferView;
        if (vkCreateBufferView(mDevice->getLogicalDevice(), &viewInfo, nullptr, &bufferView) != VK_SUCCESS) {
            return nullptr;
        }
        
        uint64_t key = mNextViewKey++;
        mViews[key] = {bufferView, format, offset, size};
        
        return reinterpret_cast<void*>(key);
    }

    void VulkanBuffer::destroyView(void* view) {
        uint64_t key = reinterpret_cast<uint64_t>(view);
        if (auto it = mViews.find(key); it != mViews.end()) {
            vkDestroyBufferView(mDevice->getLogicalDevice(), it->second.view, nullptr);
            mViews.erase(it);
        }
    }
    

    // ==================== 其他函数 ====================
    void VulkanBuffer::release() {
        for (auto& [key, viewInfo] : mViews) {
            vkDestroyBufferView(mDevice->getLogicalDevice(), viewInfo.view, nullptr);
        }
        mViews.clear();

        // 取消映射
        if (mIsMapped) {
            unmap();
        }

        // 销毁缓冲区
        if (mBuffer != VK_NULL_HANDLE) {
            if (mUsingVMA && mVmaAllocation != VK_NULL_HANDLE) {
                mDevice->destroyBufferWithVMA(mBuffer, mVmaAllocation);
            }
            else if (mTraditionalMemory != VK_NULL_HANDLE) {
                mDevice->destroyBufferTraditional(mBuffer, mTraditionalMemory);
            }
            else {
                vkDestroyBuffer(mDevice->getLogicalDevice(), mBuffer, nullptr);
            }
            mBuffer = VK_NULL_HANDLE;
            mVmaAllocation = VK_NULL_HANDLE;
            mTraditionalMemory = VK_NULL_HANDLE;
        }
    }

    bool VulkanBuffer::isValid() const {
        return mBuffer != VK_NULL_HANDLE;
    }

    void* VulkanBuffer::getNativeHandle() const {
        return reinterpret_cast<void*>(mBuffer);
    }

    void VulkanBuffer::flush(uint64_t offset, uint64_t size) {
        if (!mIsMapped || !isCPUVisible()) return;
        
        if (size == 0) {
            size = mDesc.size - offset;
        }
        
        // 根据内存类型决定是否需要刷新
        VkMemoryPropertyFlags properties = func::RHI_TO_VK_MemoryProperties(mDesc.memoryType);
        bool hostCoherent = (properties & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) != 0;
        
        if (!hostCoherent) {
            if (mUsingVMA) {
                vmaFlushAllocation(mDevice->getVmaAllocator(), mVmaAllocation, offset, size);
            } else {
                VkMappedMemoryRange range = {};
                range.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
                range.memory = mTraditionalMemory;
                range.offset = offset;
                range.size = size;
                vkFlushMappedMemoryRanges(mDevice->getLogicalDevice(), 1, &range);
            }
        }
    }

    void VulkanBuffer::invalidate(uint64_t offset, uint64_t size) {
        if (!mIsMapped || !isCPUVisible()) return;
        
        if (size == 0) {
            size = mDesc.size - offset;
        }
        
        VkMemoryPropertyFlags properties = func::RHI_TO_VK_MemoryProperties(mDesc.memoryType);
        bool hostCoherent = (properties & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) != 0;
        
        if (!hostCoherent) {
            if (mUsingVMA) {
                vmaInvalidateAllocation(mDevice->getVmaAllocator(), mVmaAllocation, offset, size);
            } else {
                VkMappedMemoryRange range = {};
                range.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
                range.memory = mTraditionalMemory;
                range.offset = offset;
                range.size = size;
                vkInvalidateMappedMemoryRanges(mDevice->getLogicalDevice(), 1, &range);
            }
        }
    }

    VulkanRenderPass::VulkanRenderPass(std::shared_ptr<VulkanDevice>  device, const RenderPassDesc& desc): mDevice(device), mDesc(desc) {
        // ---------- 附件描述转换 ----------
        std::vector<VkAttachmentDescription> vkAttachments;
        vkAttachments.reserve(mDesc.attachments.size());
        for (const auto& att : mDesc.attachments) {
            VkAttachmentDescription vkAtt = {};
            vkAtt.format = func::RHI_TO_VK_Format(att.format);                      
            vkAtt.samples = static_cast<VkSampleCountFlagBits>(att.sampleCount); 
            vkAtt.loadOp = func::RHI_TO_VK_AttachmentLoadOp(att.loadOp);            
            vkAtt.storeOp = func::RHI_TO_VK_AttachmentStoreOp(att.storeOp);        
            vkAtt.stencilLoadOp = func::RHI_TO_VK_AttachmentLoadOp(att.stencilLoadOp); 
            vkAtt.stencilStoreOp = func::RHI_TO_VK_AttachmentStoreOp(att.stencilStoreOp); 
            vkAtt.initialLayout = func::RHI_TO_VK_ImageLayout(att.initialLayout); 
            vkAtt.finalLayout = func::RHI_TO_VK_ImageLayout(att.finalLayout);     
            vkAttachments.push_back(vkAtt);
        }

        // ---------- 子通道转换 ----------
        std::vector<VkSubpassDescription> vkSubpasses;
        std::vector<std::vector<VkAttachmentReference>> vkInputRefs;
        std::vector<std::vector<VkAttachmentReference>> vkColorRefs;
        std::vector<std::vector<VkAttachmentReference>> vkResolveRefs;
        std::vector<VkAttachmentReference> vkDepthStencilRefs;

        vkSubpasses.reserve(mDesc.subpasses.size());
        vkInputRefs.reserve(mDesc.subpasses.size());
        vkColorRefs.reserve(mDesc.subpasses.size());
        vkResolveRefs.reserve(mDesc.subpasses.size());
        vkDepthStencilRefs.reserve(mDesc.subpasses.size());

        for (const auto& subpass : mDesc.subpasses) {
            // 输入附件
            vkInputRefs.emplace_back();
            auto& inputRefs = vkInputRefs.back();
            inputRefs.reserve(subpass.inputAttachments.size());
            for (const auto& ref : subpass.inputAttachments) {
                VkAttachmentReference vkRef = {
                    ref.attachment,
                    func::RHI_TO_VK_ImageLayout(ref.layout)
                };
                inputRefs.push_back(vkRef);
            }

            // 颜色附件
            vkColorRefs.emplace_back();
            auto& colorRefs = vkColorRefs.back();
            colorRefs.reserve(subpass.colorAttachments.size());
            for (const auto& ref : subpass.colorAttachments) {
                VkAttachmentReference vkRef = {
                    ref.attachment,
                    func::RHI_TO_VK_ImageLayout(ref.layout)
                };
                colorRefs.push_back(vkRef);
            }

            // 解析附件
            vkResolveRefs.emplace_back();
            auto& resolveRefs = vkResolveRefs.back();
            resolveRefs.reserve(subpass.resolveAttachments.size());
            for (const auto& ref : subpass.resolveAttachments) {
                VkAttachmentReference vkRef = {
                    ref.attachment,
                    func::RHI_TO_VK_ImageLayout(ref.layout)
                };
                resolveRefs.push_back(vkRef);
            }

            // 深度模板附件
            VkAttachmentReference depthRef = {
                VK_ATTACHMENT_UNUSED,
                VK_IMAGE_LAYOUT_UNDEFINED
            };
            if (subpass.depthStencilAttachment.attachment != VK_ATTACHMENT_UNUSED) {
                depthRef.attachment = subpass.depthStencilAttachment.attachment;
                depthRef.layout = func::RHI_TO_VK_ImageLayout(subpass.depthStencilAttachment.layout);
            }
            vkDepthStencilRefs.push_back(depthRef);

            VkSubpassDescription vkSubpass = {};
            vkSubpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
            vkSubpass.inputAttachmentCount = static_cast<uint32_t>(inputRefs.size());
            vkSubpass.pInputAttachments = inputRefs.empty() ? nullptr : inputRefs.data();
            vkSubpass.colorAttachmentCount = static_cast<uint32_t>(colorRefs.size());
            vkSubpass.pColorAttachments = colorRefs.empty() ? nullptr : colorRefs.data();
            vkSubpass.pResolveAttachments = resolveRefs.empty() ? nullptr : resolveRefs.data();
            vkSubpass.pDepthStencilAttachment = &vkDepthStencilRefs.back();
            vkSubpass.preserveAttachmentCount = static_cast<uint32_t>(subpass.preserveAttachments.size());
            vkSubpass.pPreserveAttachments = subpass.preserveAttachments.empty() ? nullptr : subpass.preserveAttachments.data();
            vkSubpasses.push_back(vkSubpass);
        }

        // ---------- 依赖关系转换 ----------
        std::vector<VkSubpassDependency> vkDependencies;
        vkDependencies.reserve(mDesc.dependencies.size());
        for (const auto& dep : mDesc.dependencies) {
            VkSubpassDependency vkDep = {};
            vkDep.srcSubpass = dep.srcSubpass;
            vkDep.dstSubpass = dep.dstSubpass;
            vkDep.srcStageMask = func::RHI_TO_VK_PipelineStageFlags(dep.srcStageMask);
            vkDep.dstStageMask = func::RHI_TO_VK_PipelineStageFlags(dep.dstStageMask); 
            vkDep.srcAccessMask = func::RHI_TO_VK_AccessFlags(dep.srcAccessMask);      
            vkDep.dstAccessMask = func::RHI_TO_VK_AccessFlags(dep.dstAccessMask);      
            vkDep.dependencyFlags = dep.byRegion ? VK_DEPENDENCY_BY_REGION_BIT : 0;
            vkDependencies.push_back(vkDep);
        }

        mVkRenderPass = mDevice->createRenderPass(vkAttachments, vkSubpasses, vkDependencies);
    }

    void VulkanRenderPass::release() {
		mDevice->destroyRenderPass(mVkRenderPass);
    }

    //VulkanPipelineLayout
    VulkanPipelineLayout::VulkanPipelineLayout(
        std::shared_ptr<VulkanDevice>  device,
        const PipelineLayoutDesc& desc,
        std::vector<VkDescriptorSetLayout> vkDescriptorSetLayouts 
    ) :mDevice(device), mDesc(desc), mDescriptorSetLayouts(vkDescriptorSetLayouts),
        mLayoutHandles(desc.descriptorSetLayouts) {
        std::vector<VkPushConstantRange> vkPushConstants;
        vkPushConstants.reserve(mDesc.pushConstants.size());

        for (const auto& range : mDesc.pushConstants) {
            VkPushConstantRange vkRange{};
            vkRange.stageFlags = static_cast<VkShaderStageFlags>(range.stageFlags);
            vkRange.offset = range.offset;
            vkRange.size = range.size;
            vkPushConstants.push_back(vkRange);
        }
        mPipelineLayout = mDevice->createPipelineLayout(vkDescriptorSetLayouts, vkPushConstants);
    }

    void VulkanPipelineLayout::release() {
        mDevice->destroyPipelineLayout(mPipelineLayout);
    }

    size_t VulkanPipelineLayout::getMemoryUsage() const  {
        size_t size = sizeof(*this);

        return size;
    }

    uint32_t VulkanPipelineLayout::getBindingPoint(uint32_t set, uint32_t binding) const {
        // 反射定位绑定：引擎当前未用反射动态绑定，简化直接返回输入 binding。
        (void)set;
        return binding;
    }

    //VulkanGraphicPipeline
    VulkanGraphicPipeline::VulkanGraphicPipeline(
        std::shared_ptr<VulkanDevice>  device,
        const GraphicsPipelineDesc& desc,
        std::vector<VkPipelineShaderStageCreateInfo> shaderStages,
        VkPipelineLayout pipelineLayout,
        VkRenderPass renderPass
    ) : mDevice(device), mDesc(desc), mShaderStages(shaderStages),
        mPipelineLayout(pipelineLayout), mRenderPass(renderPass) {

        // 2. 顶点输入状态
        VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

        std::vector<VkVertexInputBindingDescription> bindingDescs;
        std::vector<VkVertexInputAttributeDescription> attributeDescs;
        // 从 mDesc.vertexInput 转换（注意 createPipeline 中需取消注释）
        for (const auto& binding : mDesc.vertexInput.bindings) {
            VkVertexInputBindingDescription b{};
            b.binding = binding.binding;
            b.stride = binding.stride;
            b.inputRate = (binding.inputRate == RHI::VertexInputRate::PerVertex)
                ? VK_VERTEX_INPUT_RATE_VERTEX : VK_VERTEX_INPUT_RATE_INSTANCE;
            bindingDescs.push_back(b);
        }
        for (const auto& attr : mDesc.vertexInput.attributes) {
            VkVertexInputAttributeDescription a{};
            a.location = attr.location;
            a.binding = attr.binding;
            a.format = func::RHI_TO_VK_Format(attr.format);
            a.offset = attr.offset;
            attributeDescs.push_back(a);
        }
        vertexInputInfo.vertexBindingDescriptionCount = static_cast<uint32_t>(bindingDescs.size());
        vertexInputInfo.pVertexBindingDescriptions = bindingDescs.data();
        vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescs.size());
        vertexInputInfo.pVertexAttributeDescriptions = attributeDescs.data();

        // 3. 输入装配状态
        VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology = static_cast<VkPrimitiveTopology>(mDesc.topology);
        inputAssembly.primitiveRestartEnable = mDesc.primitiveRestartEnable ? VK_TRUE : VK_FALSE;

        // 4. 动态状态（可选）
        std::vector<VkDynamicState> dynamicStates;
        for (auto ds : mDesc.dynamicStates) {
            dynamicStates.push_back(static_cast<VkDynamicState>(ds));
        }
        VkPipelineDynamicStateCreateInfo dynamicState{};
        dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
        dynamicState.pDynamicStates = dynamicStates.data();

        // 5. 视口和剪刀（若动态则无需指定具体值，但需提供数量）
        VkPipelineViewportStateCreateInfo viewportState{};
        viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        // 如果视口是动态的，这里只需设置数量，实际值在绘制命令中提供
        viewportState.viewportCount = static_cast<uint32_t>(mDesc.viewport.viewports.size());
        viewportState.scissorCount = static_cast<uint32_t>(mDesc.viewport.scissors.size());
        // 如果非动态，则需要提供具体数据，此处简化处理（假设动态）

        // 6. 光栅化状态
        VkPipelineRasterizationStateCreateInfo rasterizer{};
        rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizer.depthClampEnable = mDesc.rasterizer.depthClampEnable ? VK_TRUE : VK_FALSE;
        rasterizer.rasterizerDiscardEnable = mDesc.rasterizer.discardEnable ? VK_TRUE : VK_FALSE;
        rasterizer.polygonMode = static_cast<VkPolygonMode>(mDesc.rasterizer.polygonMode);
        rasterizer.cullMode = static_cast<VkCullModeFlags>(mDesc.rasterizer.cullMode);
        rasterizer.frontFace = static_cast<VkFrontFace>(mDesc.rasterizer.frontFace);
        rasterizer.depthBiasEnable = mDesc.rasterizer.depthBiasEnable ? VK_TRUE : VK_FALSE;
        rasterizer.depthBiasConstantFactor = mDesc.rasterizer.depthBiasConstantFactor;
        rasterizer.depthBiasClamp = mDesc.rasterizer.depthBiasClamp;
        rasterizer.depthBiasSlopeFactor = mDesc.rasterizer.depthBiasSlopeFactor;
        rasterizer.lineWidth = mDesc.rasterizer.lineWidth;

        // 7. 多重采样
        VkPipelineMultisampleStateCreateInfo multisampling{};
        multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.rasterizationSamples = static_cast<VkSampleCountFlagBits>(mDesc.multisample.rasterizationSamples);
        multisampling.sampleShadingEnable = mDesc.multisample.sampleShadingEnable ? VK_TRUE : VK_FALSE;
        multisampling.minSampleShading = mDesc.multisample.minSampleShading;
        multisampling.pSampleMask = mDesc.multisample.sampleMask.data();
        multisampling.alphaToCoverageEnable = mDesc.multisample.alphaToCoverageEnable ? VK_TRUE : VK_FALSE;
        multisampling.alphaToOneEnable = mDesc.multisample.alphaToOneEnable ? VK_TRUE : VK_FALSE;

        // 8. 深度模板状态
        VkPipelineDepthStencilStateCreateInfo depthStencil{};
        depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depthStencil.depthTestEnable = mDesc.depthStencil.depthTestEnable ? VK_TRUE : VK_FALSE;
        depthStencil.depthWriteEnable = mDesc.depthStencil.depthWriteEnable ? VK_TRUE : VK_FALSE;
        depthStencil.depthCompareOp = static_cast<VkCompareOp>(mDesc.depthStencil.depthCompareOp);
        depthStencil.depthBoundsTestEnable = mDesc.depthStencil.depthBoundsTestEnable ? VK_TRUE : VK_FALSE;
        depthStencil.stencilTestEnable = mDesc.depthStencil.stencilTestEnable ? VK_TRUE : VK_FALSE;
        depthStencil.front = convertStencilOpState(mDesc.depthStencil.front);
        depthStencil.back = convertStencilOpState(mDesc.depthStencil.back);
        depthStencil.minDepthBounds = mDesc.depthStencil.minDepthBounds;
        depthStencil.maxDepthBounds = mDesc.depthStencil.maxDepthBounds;

        // 9. 颜色混合状态
        std::vector<VkPipelineColorBlendAttachmentState> blendAttachments;
        for (const auto& att : mDesc.colorBlend.attachments) {
            VkPipelineColorBlendAttachmentState blendAtt{};
            blendAtt.blendEnable = att.blendEnable ? VK_TRUE : VK_FALSE;
            blendAtt.srcColorBlendFactor = static_cast<VkBlendFactor>(att.srcColorBlendFactor);
            blendAtt.dstColorBlendFactor = static_cast<VkBlendFactor>(att.dstColorBlendFactor);
            blendAtt.colorBlendOp = static_cast<VkBlendOp>(att.colorBlendOp);
            blendAtt.srcAlphaBlendFactor = static_cast<VkBlendFactor>(att.srcAlphaBlendFactor);
            blendAtt.dstAlphaBlendFactor = static_cast<VkBlendFactor>(att.dstAlphaBlendFactor);
            blendAtt.alphaBlendOp = static_cast<VkBlendOp>(att.alphaBlendOp);
            blendAtt.colorWriteMask = static_cast<VkColorComponentFlags>(att.colorWriteMask);
            blendAttachments.push_back(blendAtt);
        }

        VkPipelineColorBlendStateCreateInfo colorBlending{};
        colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlending.logicOpEnable = mDesc.colorBlend.logicOpEnable ? VK_TRUE : VK_FALSE;
        colorBlending.logicOp = static_cast<VkLogicOp>(mDesc.colorBlend.logicOp);
        colorBlending.attachmentCount = static_cast<uint32_t>(blendAttachments.size());
        colorBlending.pAttachments = blendAttachments.data();
        memcpy(colorBlending.blendConstants, mDesc.colorBlend.blendConstants.data(), 4 * sizeof(float));

        // 12. 创建图形管线
        VkGraphicsPipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
        pipelineInfo.pStages = shaderStages.data();
        pipelineInfo.pVertexInputState = &vertexInputInfo;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState = &multisampling;
        pipelineInfo.pDepthStencilState = &depthStencil;
        pipelineInfo.pColorBlendState = &colorBlending;
        pipelineInfo.pDynamicState = dynamicStates.empty() ? nullptr : &dynamicState;
        pipelineInfo.layout = pipelineLayout;
        pipelineInfo.renderPass = renderPass;
        pipelineInfo.subpass = mDesc.subpass;

        // 调用设备创建管线
        mPipeline = mDevice->createGraphicsPipeline(pipelineInfo);
    }

    void VulkanGraphicPipeline::release() {
        mDevice->destroyPipeline(mPipeline);
    }

    // 辅助函数：转换模板操作状态
    VkStencilOpState VulkanGraphicPipeline::convertStencilOpState(const StencilOpState& state) {
        VkStencilOpState vkState{};
        vkState.failOp = static_cast<VkStencilOp>(state.failOp);
        vkState.passOp = static_cast<VkStencilOp>(state.passOp);
        vkState.depthFailOp = static_cast<VkStencilOp>(state.depthFailOp);
        vkState.compareOp = static_cast<VkCompareOp>(state.compareOp);
        vkState.compareMask = state.compareMask;
        vkState.writeMask = state.writeMask;
        vkState.reference = state.reference;
        return vkState;
    }

    // 构造实现
    VulkanComputePipeline::VulkanComputePipeline(
        std::shared_ptr<VulkanDevice>  device,
        const ComputePipelineDesc& desc,
        VkPipelineShaderStageCreateInfo shaderStage,
        VkPipelineLayout pipelineLayout
    ) : mDevice(device), mDesc(desc), mPipelineLayout(pipelineLayout) {
        VkComputePipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        pipelineInfo.stage = shaderStage;         
        pipelineInfo.layout = pipelineLayout;
        pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
        pipelineInfo.basePipelineIndex = -1;

        mPipeline = mDevice->createComputePipeline(pipelineInfo);
    }

    void VulkanComputePipeline::release() {
        if (mPipeline != VK_NULL_HANDLE) {
            mDevice->destroyPipeline(mPipeline);
            mPipeline = VK_NULL_HANDLE;
        }
    }
    

    VulkanCommandPool::VulkanCommandPool(std::shared_ptr<VulkanDevice>  device, CommandPoolDesc desc):mDevice(device),mDesc(desc) {
		uint32_t queueFamilyIndex = mDevice->getQueueFamilyIndex(static_cast<uint32_t>(mDesc.queueType));   
		VkCommandPoolCreateFlags flags = 0;
        if (mDesc.transient) {
			flags |= VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
        }
        if (mDesc.resetCommandBuffer) {
            flags |= VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        }
		if (mDesc.protectedMemory) {
			flags |= VK_COMMAND_POOL_CREATE_PROTECTED_BIT;
		}
		if (queueFamilyIndex == INT_MAX) {
			throw std::runtime_error("Failed to find suitable queue family for command pool");
		}

		mCommandPool = mDevice->createCommandPool(queueFamilyIndex, flags);
    }
    void VulkanCommandPool::release() {
		mDevice->destroyCommandPool(mCommandPool);
    }

    std::vector<VkCommandBuffer> VulkanCommandPool::allocateCommandBuffers(uint32_t count, CommandBufferLevel level) {
        if (!isValid() || count == 0) return {};
        VkCommandBufferLevel vkLevel = (level == CommandBufferLevel::Primary) ? VK_COMMAND_BUFFER_LEVEL_PRIMARY : VK_COMMAND_BUFFER_LEVEL_SECONDARY;
        return mDevice->allocateCommandBuffers(mCommandPool, count, vkLevel);
    }

    void VulkanCommandPool::freeCommandBuffers(const std::vector<VkCommandBuffer>& commandBuffers) {
        if (!isValid() || commandBuffers.empty()) return;
        mDevice->freeCommandBuffers(mCommandPool, commandBuffers);
    }

    VkCommandBuffer VulkanCommandPool::allocateCommandBuffer(CommandBufferLevel level) {
		return allocateCommandBuffers(0,level)[0];
    }
    void VulkanCommandPool::freeCommandBuffer(const VkCommandBuffer& commandBuffer) {
		freeCommandBuffers({ commandBuffer });
    }

    void VulkanCommandPool::reset(bool releaseResources) {
        if (!isValid()) return;
        VkCommandPoolResetFlags flags = releaseResources ? VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT : static_cast<VkCommandPoolResetFlags>(0);
        vkResetCommandPool(mDevice->getLogicalDevice(), mCommandPool, flags);
    }

	VulkanCommandBuffer::VulkanCommandBuffer(std::shared_ptr<VulkanDevice>  device, CommandBufferDesc desc, VulkanCommandPool* cmdPool)
		: mDevice(device), mDesc(desc), mCommandPool(cmdPool) {
		mCommandBuffer = mCommandPool->allocateCommandBuffer(desc.level);
	}

    void VulkanCommandBuffer::release() {
        mCommandPool->freeCommandBuffer(mCommandBuffer); 
    }

    // 生命周期
    void VulkanCommandBuffer::begin() {
		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = 0; 
		if (mDesc.oneTimeSubmit) beginInfo.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		if (mDesc.simultaneousUse) beginInfo.flags |= VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;

		if (mDesc.level == CommandBufferLevel::Primary) {
			beginInfo.pInheritanceInfo = nullptr;
        }else{
			throw std::runtime_error("Secondary command buffers are not supported in this implementation");
        }
		if (vkBeginCommandBuffer(mCommandBuffer, &beginInfo) != VK_SUCCESS) {
			throw std::runtime_error("Failed to begin recording command buffer");
		}

    }
    void VulkanCommandBuffer::end() {
        if (vkEndCommandBuffer(mCommandBuffer) != VK_SUCCESS) {
            throw std::runtime_error("Failed to end recording command buffer");
        }
    }
    void VulkanCommandBuffer::reset(bool releaseResources) {
		VkCommandBufferResetFlags flags = releaseResources 
            ? VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT : static_cast<VkCommandBufferResetFlags>(0);
		if (vkResetCommandBuffer(mCommandBuffer, flags) != VK_SUCCESS) {
			throw std::runtime_error("Failed to reset command buffer");
		}
    }


    VulkanFramebuffer::VulkanFramebuffer(std::shared_ptr<VulkanDevice> device,
                                         RHIRenderPass* renderPass,
                                         const std::vector<RHITexture*>& attachments,
                                         const FramebufferDesc& desc)
        : mDevice(device), mDesc(desc) {
        mAttachments.reserve(attachments.size() + desc.nativeAttachments.size());
        for (void* nativeView : desc.nativeAttachments) {
            mAttachments.push_back(static_cast<VkImageView>(nativeView));
        }
        for (const auto* att : attachments) {
            mAttachments.push_back(static_cast<VkImageView>(att->getDefaultView()));
        }
        mAttachmentCount = static_cast<uint32_t>(mAttachments.size());

        mFramebuffer = mDevice->createFramebuffer(
            static_cast<VkRenderPass>(renderPass->getNativeHandle()),
            mAttachments, desc.extent.width, desc.extent.height, desc.layers);
    }
    void VulkanFramebuffer::release(){
		mDevice->destroyFramebuffer(mFramebuffer);
    }

    namespace {
        VkImageUsageFlags convertUsage(const TextureDesc& desc) {
            VkImageUsageFlags usage = 0;
            if (desc.allowRenderTarget) usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
            if (desc.allowDepthStencil) usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
            if (desc.allowUnorderedAccess) usage |= VK_IMAGE_USAGE_STORAGE_BIT;
            if (desc.allowInputAttachment) usage |= VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
            usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
            usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
            return usage;
        }

        VmaMemoryUsage convertMemoryUsage(const TextureDesc& desc) {
            if (desc.memoryless) return VMA_MEMORY_USAGE_GPU_LAZILY_ALLOCATED;
            return VMA_MEMORY_USAGE_GPU_ONLY;
        }

        VkMemoryPropertyFlags convertMemoryProperties(const TextureDesc& desc) {
            if (desc.memoryless) return VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT;
            return VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        }
    }

    // ==================== VulkanTexture 实现 ====================
    VulkanTexture::VulkanTexture(std::shared_ptr<VulkanDevice>  device, const TextureDesc& desc)
        : mDevice(device), mDesc(desc), mUsingVMA(device->isVMAEnabled()) {
        createTexture();
    }

    VulkanTexture::~VulkanTexture() {
        release();
    }

    void VulkanTexture::release() {
        for (auto& [key, viewInfo] : mViews) {
            mDevice->destroyImageView(viewInfo.view);
        }
        mViews.clear();
        mDefaultView = nullptr;

        if (mDepthAspectView != VK_NULL_HANDLE) {
            mDevice->destroyImageView(mDepthAspectView);
            mDepthAspectView = VK_NULL_HANDLE;
        }

        if (mUsingVMA) {
            if (vmaImage.image != VK_NULL_HANDLE) {
                mDevice->destroyImageWithVMAFull(vmaImage);
                vmaImage = {}; // 重置为默认值
            }
        }
        else {
            if (traditionalImage.image != VK_NULL_HANDLE) {
                mDevice->destroyImageTraditionalFull(traditionalImage);
                traditionalImage = {}; // 重置
            }
        }
    }

    bool VulkanTexture::isValid() const {
        bool valid = false;
        if (mUsingVMA) {
            valid = vmaImage.image != VK_NULL_HANDLE && vmaImage.view != VK_NULL_HANDLE;
            if (!valid) {
                std::cerr << "[VulkanTexture] isValid false: vmaImage.image=" << vmaImage.image
                    << ", vmaImage.view=" << vmaImage.view << " for " << mDesc.debugName << std::endl;
            }
        }
        else {
            valid = traditionalImage.image != VK_NULL_HANDLE && traditionalImage.view != VK_NULL_HANDLE;
            if (!valid) {
                std::cerr << "[VulkanTexture] isValid false: traditionalImage.image=" << traditionalImage.image
                    << ", traditionalImage.view=" << traditionalImage.view << " for " << mDesc.debugName << std::endl;
            }
        }
        return valid;
    }

    void* VulkanTexture::getNativeHandle() const {
        return mUsingVMA ? (void*)vmaImage.image : (void*)traditionalImage.image;
    }

    size_t VulkanTexture::getMemoryUsage() const {
        if (!isValid()) return 0;
        if (mUsingVMA) {
            VmaAllocationInfo allocInfo;
            vmaGetAllocationInfo(mDevice->getVmaAllocator(), vmaImage.allocation, &allocInfo);
            return allocInfo.size;
        }
        else {
            VkMemoryRequirements memReq;
            vkGetImageMemoryRequirements(mDevice->getLogicalDevice(), traditionalImage.image, &memReq);
            return memReq.size;
        }
    }

    void* VulkanTexture::createView(const ImageSubresourceRange& range, ImageViewType viewType) {
        VkImageViewType vkViewType = func::RHI_TO_VK_ImageViewType(viewType, mDesc.type);
        VkImageView vkView = createVkImageView(range, vkViewType);
        if (vkView == VK_NULL_HANDLE) return nullptr;

        uint64_t key = mNextViewKey++;
        mViews[key] = { vkView, range };
        return reinterpret_cast<void*>(key);
    }

    void VulkanTexture::destroyView(void* view) {
        uint64_t key = reinterpret_cast<uint64_t>(view);
        auto it = mViews.find(key);
        if (it != mViews.end()) {
            mDevice->destroyImageView(it->second.view);
            mViews.erase(it);
        }
    }

    void* VulkanTexture::getDefaultView() const {
        return mDefaultView;
    }

    void* VulkanTexture::getSamplingView() const {
        if (!mDesc.allowDepthStencil) return mDefaultView;
        if (mDepthAspectView == VK_NULL_HANDLE) {
            VkImage image = mUsingVMA ? vmaImage.image : traditionalImage.image;
            VkFormat format = func::RHI_TO_VK_Format(mDesc.format);
            VkImageViewType viewType = func::RHI_TO_VK_ImageViewType(ImageViewType::Auto, mDesc.type);
            mDepthAspectView = mDevice->createImageView(image, format, VK_IMAGE_ASPECT_DEPTH_BIT, viewType,0, mDesc.mipLevels, 0, mDesc.arrayLayers);
        }
        return reinterpret_cast<void*>(mDepthAspectView);
    }

    void VulkanTexture::transitionLayout(ImageLayout newLayout,
        PipelineStage srcStage,
        PipelineStage dstStage,
        AccessFlag srcAccess,
        AccessFlag dstAccess,
        const ImageSubresourceRange& range) {
        if (range.levelCount == 0 || range.layerCount == 0) return;

        uint32_t firstMip = range.baseMipLevel;
        uint32_t firstLayer = range.baseArrayLayer;
        ImageLayout expectedOldLayout = getSubresourceLayout(firstMip, firstLayer);

#ifndef NDEBUG
        bool consistent = true;
        forEachSubresource(range, [&](uint32_t mip, uint32_t layer) {
            if (getSubresourceLayout(mip, layer) != expectedOldLayout) {
                consistent = false;
            }
            });
        if (!consistent) {
            LOG_WARN("transitionLayout called on range with mixed layouts");
        }
#endif

        VkCommandPool cmdPool = mDevice->getTransferCommandPool();
        VkCommandBuffer cmdBuf = mDevice->beginSingleTimeCommands(cmdPool);

        VkImageMemoryBarrier barrier = {};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = func::RHI_TO_VK_ImageLayout(expectedOldLayout);
        barrier.newLayout = func::RHI_TO_VK_ImageLayout(newLayout);
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = mUsingVMA ? vmaImage.image : traditionalImage.image;
        barrier.subresourceRange.aspectMask = func::RHI_TO_VK_ImageAspect(range.aspectMask);
        barrier.subresourceRange.baseMipLevel = range.baseMipLevel;
        barrier.subresourceRange.levelCount = range.levelCount;
        barrier.subresourceRange.baseArrayLayer = range.baseArrayLayer;
        barrier.subresourceRange.layerCount = range.layerCount;
        barrier.srcAccessMask = func::RHI_TO_VK_AccessFlags(srcAccess);
        barrier.dstAccessMask = func::RHI_TO_VK_AccessFlags(dstAccess);

        vkCmdPipelineBarrier(cmdBuf,
            func::RHI_TO_VK_PipelineStageFlags(srcStage),
            func::RHI_TO_VK_PipelineStageFlags(dstStage),
            0, 0, nullptr, 0, nullptr, 1, &barrier);

        mDevice->endSingleTimeCommands(cmdPool, cmdBuf);

        forEachSubresource(range, [&](uint32_t mip, uint32_t layer) {
            setSubresourceLayout(mip, layer, newLayout);
            });
    }

    void VulkanTexture::copyFromBuffer(RHIBuffer* srcBuffer, const std::vector<BufferImageCopyRegion>& regions) {
        VkBuffer vkBuffer = static_cast<VkBuffer>(srcBuffer->getNativeHandle());
        copyFromBuffer(vkBuffer, regions);
    }
    void VulkanTexture::copyFromBuffer(VkBuffer srcBuffer, const std::vector<BufferImageCopyRegion>& regions) {
        VkCommandPool cmdPool = mDevice->getTransferCommandPool();
        VkCommandBuffer cmdBuf = mDevice->beginSingleTimeCommands(cmdPool);

        std::vector<VkBufferImageCopy> vkRegions;
        vkRegions.reserve(regions.size());
        for (const auto& region : regions) {
            VkBufferImageCopy vkRegion = {};
            vkRegion.bufferOffset = region.bufferOffset;
            vkRegion.bufferRowLength = region.bufferRowLength;
            vkRegion.bufferImageHeight = region.bufferImageHeight;
            vkRegion.imageSubresource.aspectMask = func::RHI_TO_VK_ImageAspect(region.imageSubresource.aspectMask);
            vkRegion.imageSubresource.mipLevel = region.imageSubresource.baseMipLevel;
            vkRegion.imageSubresource.baseArrayLayer = region.imageSubresource.baseArrayLayer;
            vkRegion.imageSubresource.layerCount = region.imageSubresource.layerCount;
            vkRegion.imageOffset = { region.imageOffset.x, region.imageOffset.y, region.imageOffset.z };
            vkRegion.imageExtent = { region.imageExtent.width, region.imageExtent.height, region.imageExtent.depth };
            vkRegions.push_back(vkRegion);
        }

        vkCmdCopyBufferToImage(cmdBuf,
            srcBuffer,
            mUsingVMA ? vmaImage.image : traditionalImage.image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            static_cast<uint32_t>(vkRegions.size()),
            vkRegions.data());

        mDevice->endSingleTimeCommands(cmdPool, cmdBuf);
    }

    void VulkanTexture::copyToBuffer(RHIBuffer* dstBuffer, const std::vector<BufferImageCopyRegion>& regions) {
        VulkanBuffer* vkDstBuffer = dynamic_cast<VulkanBuffer*>(dstBuffer);
        if (!vkDstBuffer) return;

        VkCommandPool cmdPool = mDevice->getTransferCommandPool();
        VkCommandBuffer cmdBuf = mDevice->beginSingleTimeCommands(cmdPool);

        std::vector<VkBufferImageCopy> vkRegions;
        vkRegions.reserve(regions.size());
        for (const auto& region : regions) {
            VkBufferImageCopy vkRegion = {};
            vkRegion.bufferOffset = region.bufferOffset;
            vkRegion.bufferRowLength = region.bufferRowLength;
            vkRegion.bufferImageHeight = region.bufferImageHeight;
            vkRegion.imageSubresource.aspectMask = func::RHI_TO_VK_ImageAspect(region.imageSubresource.aspectMask);
            vkRegion.imageSubresource.mipLevel = region.imageSubresource.baseMipLevel;
            vkRegion.imageSubresource.baseArrayLayer = region.imageSubresource.baseArrayLayer;
            vkRegion.imageSubresource.layerCount = region.imageSubresource.layerCount;
            vkRegion.imageOffset = { region.imageOffset.x, region.imageOffset.y, region.imageOffset.z };
            vkRegion.imageExtent = { region.imageExtent.width, region.imageExtent.height, region.imageExtent.depth };
            vkRegions.push_back(vkRegion);
        }

        vkCmdCopyImageToBuffer(cmdBuf,
            mUsingVMA ? vmaImage.image : traditionalImage.image,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            static_cast<VkBuffer>(vkDstBuffer->getNativeHandle()),
            static_cast<uint32_t>(vkRegions.size()),
            vkRegions.data());

        mDevice->endSingleTimeCommands(cmdPool, cmdBuf);
    }

    void VulkanTexture::readbackAsync(RHIBuffer* dstBuffer, const std::vector<BufferImageCopyRegion>& regions,
                                       VkCommandBuffer cmdBuf, VkFence fence) {
        VulkanBuffer* vkDstBuffer = dynamic_cast<VulkanBuffer*>(dstBuffer);
        if (!vkDstBuffer || cmdBuf == VK_NULL_HANDLE || fence == VK_NULL_HANDLE) return;

        vkResetCommandBuffer(cmdBuf, 0);
        VkCommandBufferBeginInfo beginInfo = {};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(cmdBuf, &beginInfo);

        VkImage image = mUsingVMA ? vmaImage.image : traditionalImage.image;
        ImageLayout oldLayout = getSubresourceLayout(0, 0);   // 首帧为 Undefined，barrier 同样合法

        auto recordBarrier = [&](ImageLayout from, ImageLayout to, PipelineStage srcStage, PipelineStage dstStage,
                                 AccessFlag srcAccess, AccessFlag dstAccess) {
            VkImageMemoryBarrier barrier = {};
            barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            barrier.oldLayout = func::RHI_TO_VK_ImageLayout(from);
            barrier.newLayout = func::RHI_TO_VK_ImageLayout(to);
            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.image = image;
            barrier.subresourceRange.aspectMask = func::RHI_TO_VK_ImageAspect(RHI::ImageAspect::Color);
            barrier.subresourceRange.baseMipLevel = 0;
            barrier.subresourceRange.levelCount = mDesc.mipLevels;
            barrier.subresourceRange.baseArrayLayer = 0;
            barrier.subresourceRange.layerCount = mDesc.arrayLayers;
            barrier.srcAccessMask = func::RHI_TO_VK_AccessFlags(srcAccess);
            barrier.dstAccessMask = func::RHI_TO_VK_AccessFlags(dstAccess);
            vkCmdPipelineBarrier(cmdBuf,
                func::RHI_TO_VK_PipelineStageFlags(srcStage),
                func::RHI_TO_VK_PipelineStageFlags(dstStage),
                0, 0, nullptr, 0, nullptr, 1, &barrier);
        };

        // ShaderReadOnly → TransferSrc，拷贝，TransferSrc → ShaderReadOnly，一个命令缓冲原子提交。
        recordBarrier(oldLayout, ImageLayout::TransferSrc,
                      PipelineStage::FragmentShader, PipelineStage::Transfer,
                      AccessFlag::ShaderRead,
                      AccessFlag::TransferRead);

        std::vector<VkBufferImageCopy> vkRegions;
        vkRegions.reserve(regions.size());
        for (const auto& region : regions) {
            VkBufferImageCopy vkRegion = {};
            vkRegion.bufferOffset = region.bufferOffset;
            vkRegion.bufferRowLength = region.bufferRowLength;
            vkRegion.bufferImageHeight = region.bufferImageHeight;
            vkRegion.imageSubresource.aspectMask = func::RHI_TO_VK_ImageAspect(region.imageSubresource.aspectMask);
            vkRegion.imageSubresource.mipLevel = region.imageSubresource.baseMipLevel;
            vkRegion.imageSubresource.baseArrayLayer = region.imageSubresource.baseArrayLayer;
            vkRegion.imageSubresource.layerCount = region.imageSubresource.layerCount;
            vkRegion.imageOffset = { region.imageOffset.x, region.imageOffset.y, region.imageOffset.z };
            vkRegion.imageExtent = { region.imageExtent.width, region.imageExtent.height, region.imageExtent.depth };
            vkRegions.push_back(vkRegion);
        }

        vkCmdCopyImageToBuffer(cmdBuf, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            static_cast<VkBuffer>(vkDstBuffer->getNativeHandle()),
            static_cast<uint32_t>(vkRegions.size()), vkRegions.data());

        recordBarrier(ImageLayout::TransferSrc, ImageLayout::ShaderReadOnly,
                      PipelineStage::Transfer, PipelineStage::FragmentShader,
                      AccessFlag::TransferRead,
                      AccessFlag::ShaderRead);

        mDevice->submitAsync(cmdBuf, fence); 

        forEachSubresource({ ImageAspect::Color, 0, mDesc.mipLevels, 0, mDesc.arrayLayers },
            [&](uint32_t mip, uint32_t layer) { setSubresourceLayout(mip, layer, ImageLayout::ShaderReadOnly); });
    }

    void VulkanTexture::copyFromTexture(RHITexture* srcTexture, const std::vector<ImageCopyRegion>& regions) {
        VulkanTexture* vkSrcTexture = dynamic_cast<VulkanTexture*>(srcTexture);
        if (!vkSrcTexture) return;

        VkCommandPool cmdPool = mDevice->getTransferCommandPool();
        VkCommandBuffer cmdBuf = mDevice->beginSingleTimeCommands(cmdPool);

        std::vector<VkImageCopy> vkRegions;
        vkRegions.reserve(regions.size());
        for (const auto& region : regions) {
            VkImageCopy vkRegion = {};
            vkRegion.srcSubresource.aspectMask = func::RHI_TO_VK_ImageAspect(region.srcSubresource.aspectMask);
            vkRegion.srcSubresource.mipLevel = region.srcSubresource.baseMipLevel;
            vkRegion.srcSubresource.baseArrayLayer = region.srcSubresource.baseArrayLayer;
            vkRegion.srcSubresource.layerCount = region.srcSubresource.layerCount;
            vkRegion.srcOffset = { region.srcOffset.x, region.srcOffset.y, region.srcOffset.z };
            vkRegion.dstSubresource.aspectMask = func::RHI_TO_VK_ImageAspect(region.dstSubresource.aspectMask);
            vkRegion.dstSubresource.mipLevel = region.dstSubresource.baseMipLevel;
            vkRegion.dstSubresource.baseArrayLayer = region.dstSubresource.baseArrayLayer;
            vkRegion.dstSubresource.layerCount = region.dstSubresource.layerCount;
            vkRegion.dstOffset = { region.dstOffset.x, region.dstOffset.y, region.dstOffset.z };
            vkRegion.extent = { region.extent.width, region.extent.height, region.extent.depth };
            vkRegions.push_back(vkRegion);
        }

        vkCmdCopyImage(cmdBuf,
            vkSrcTexture->mUsingVMA ? vkSrcTexture->vmaImage.image : vkSrcTexture->traditionalImage.image,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            mUsingVMA ? vmaImage.image : traditionalImage.image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            static_cast<uint32_t>(vkRegions.size()),
            vkRegions.data());

        mDevice->endSingleTimeCommands(cmdPool, cmdBuf);
    }

    void VulkanTexture::update(const void* data, size_t size, const ImageSubresourceRange& range) {
        bool needTransition = false;
        forEachSubresource(range, [&](uint32_t mip, uint32_t layer) {
            if (getSubresourceLayout(mip, layer) != ImageLayout::TransferDst) {
                needTransition = true;
            }
            });

        if (needTransition) {
            transitionLayout(
                ImageLayout::TransferDst,
                PipelineStage::TopOfPipe,
                PipelineStage::Transfer,
                AccessFlag::None,
                AccessFlag::TransferWrite,
                range
            );
        }

        VMABuffer staging = mDevice->createBufferWithVMA(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VMA_MEMORY_USAGE_CPU_TO_GPU, 0, data, size);

        BufferImageCopyRegion region;
        region.imageSubresource = range;
        region.imageExtent = mDesc.extent;
        copyFromBuffer(staging.buffer, { region });

        mDevice->destroyBufferWithVMA(staging.buffer, staging.allocation);
    }

    void VulkanTexture::generateMipmaps() {
        VkCommandPool cmdPool = mDevice->getTransferCommandPool();
        mDevice->generateMipmaps(cmdPool,
            mUsingVMA ? vmaImage.image : traditionalImage.image,
            func::RHI_TO_VK_Format(mDesc.format),
            mDesc.extent.width, mDesc.extent.height,
            mDesc.mipLevels);
    }
    void* VulkanTexture::getNativeHandleFromView(void* viewKey){
        uint64_t key = reinterpret_cast<uint64_t>(viewKey);
        auto it = mViews.find(key);
        if (it != mViews.end()) {
            return reinterpret_cast<void*>(it->second.view);
        }
        return nullptr;
    }

    void VulkanTexture::createTexture() {
        VkFormat vkFormat;
        if (mDesc.allowDepthStencil) {
            vkFormat = func::RHI_TO_VK_Format(mDesc.format);
            if (vkFormat == VK_FORMAT_UNDEFINED) {
                vkFormat = mDevice->findDepthFormat();
            }
            mActualFormat = func::VK_TO_RHI_Format(vkFormat);
        }
        else {
            vkFormat = func::RHI_TO_VK_Format(mDesc.format);
            mActualFormat = mDesc.format;
        }

        VkImageUsageFlags usage = convertUsage(mDesc);
        VkImageAspectFlags aspect;
        if (mDesc.allowDepthStencil) {
            // 按实际格式决定 aspect：带 stencil 的深度模板格式 → DEPTH|STENCIL（视图才可访问模板）
            aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
            if (vkFormat == VK_FORMAT_D24_UNORM_S8_UINT || vkFormat == VK_FORMAT_D32_SFLOAT_S8_UINT) {
                aspect |= VK_IMAGE_ASPECT_STENCIL_BIT;
            }
        }
        else {
            aspect = VK_IMAGE_ASPECT_COLOR_BIT;
        }
        VkImageViewType viewType = func::RHI_TO_VK_ImageViewType(ImageViewType::Auto, mDesc.type);
        VkImageTiling tiling = VK_IMAGE_TILING_OPTIMAL;

        VkImageCreateFlags imageFlags = func::RHI_TO_VK_ImageCreateFlags(static_cast<ImageCreateFlags>(mDesc.flags));

        if (mUsingVMA) {
            vmaImage = mDevice->createImageWithVMAFull(
                mDesc.extent.width, mDesc.extent.height, vkFormat,
                tiling, usage, 
                convertMemoryUsage(mDesc), aspect,
                0, imageFlags,
                mDesc.mipLevels, mDesc.arrayLayers, 
                viewType
            );
            if (vmaImage.image == VK_NULL_HANDLE) {
                std::cerr << "[VulkanTexture] Failed to create VMA image for: " << mDesc.debugName << std::endl;
                return;
            }
            mDefaultView = reinterpret_cast<void*>(vmaImage.view);
        }
        else {
            traditionalImage = mDevice->createImageTraditionalFull(
                mDesc.extent.width, mDesc.extent.height, vkFormat,
                tiling, usage, 
                convertMemoryProperties(mDesc), aspect, imageFlags,
                mDesc.mipLevels, mDesc.arrayLayers, viewType
            );
            if (traditionalImage.image == VK_NULL_HANDLE) {
                std::cerr << "[VulkanTexture] Failed to create traditional image for: " << mDesc.debugName << std::endl;
                return;
            }
            mDefaultView = reinterpret_cast<void*>(traditionalImage.view);
        }

        if (!mDesc.debugName.empty()) {
            VkImage image = mUsingVMA ? vmaImage.image : traditionalImage.image;
            mDevice->setImageName(image, mDesc.debugName.c_str());
        }

        for (uint32_t mip = 0; mip < mDesc.mipLevels; ++mip) {
            for (uint32_t layer = 0; layer < mDesc.arrayLayers; ++layer) {
                setSubresourceLayout(mip, layer, ImageLayout::Undefined);
            }
        }
    }

    VkImageView VulkanTexture::createVkImageView(const ImageSubresourceRange& range, VkImageViewType viewType) {
        VkImage image = mUsingVMA ? vmaImage.image : traditionalImage.image;
        VkFormat format = func::RHI_TO_VK_Format(mDesc.format);
        VkImageAspectFlags aspect = func::RHI_TO_VK_ImageAspect(range.aspectMask);
        return mDevice->createImageView(
            image, format, aspect, viewType,
            range.baseMipLevel,     // ← 第二行开始是：baseMipLevel
            range.levelCount,       // ← levelCount
            range.baseArrayLayer,   // ← baseArrayLayer
            range.layerCount,       // ← layerCount
            "");
    }

    ImageLayout VulkanTexture::getSubresourceLayout(uint32_t mipLevel, uint32_t arrayLayer) const {
        SubresourceKey key{ mipLevel, arrayLayer };
        auto it = m_subresourceLayouts.find(key);
        if (it != m_subresourceLayouts.end()) {
            return it->second;
        }
        return ImageLayout::Undefined;
    }

    void VulkanTexture::setSubresourceLayout(uint32_t mipLevel, uint32_t arrayLayer, ImageLayout layout) {
        SubresourceKey key{ mipLevel, arrayLayer };
        m_subresourceLayouts[key] = layout;
    }

    void VulkanTexture::forEachSubresource(const ImageSubresourceRange& range,
        std::function<void(uint32_t, uint32_t)> func) {
        for (uint32_t mip = range.baseMipLevel; mip < range.baseMipLevel + range.levelCount; ++mip) {
            for (uint32_t layer = range.baseArrayLayer; layer < range.baseArrayLayer + range.layerCount; ++layer) {
                func(mip, layer);
            }
        }
    }

    // ==================== VulkanSampler 实现 ====================
    VulkanSampler::VulkanSampler(std::shared_ptr<VulkanDevice>  device, const SamplerDesc& desc)
        : mDevice(device), mDesc(desc) {
        createSampler();
    }

    VulkanSampler::~VulkanSampler() {
        release();
    }

    void VulkanSampler::release() {
        mDevice->destroySampler(mSampler);
    }

    bool VulkanSampler::isValid() const {
        return mSampler != VK_NULL_HANDLE;
    }

    void* VulkanSampler::getNativeHandle() const {
        return reinterpret_cast<void*>(mSampler);
    }

    size_t VulkanSampler::getMemoryUsage() const {
        return sizeof(*this);
    }

    void VulkanSampler::createSampler() {
        mSampler = mDevice->createSampler(
            func::RHI_TO_VK_Filter(mDesc.magFilter),
            func::RHI_TO_VK_Filter(mDesc.minFilter),
            func::RHI_TO_VK_AddressMode(mDesc.addressU),
            func::RHI_TO_VK_AddressMode(mDesc.addressV),
            func::RHI_TO_VK_AddressMode(mDesc.addressW),
            mDesc.maxAnisotropy > 1.0f,
            mDesc.maxAnisotropy,
            mDesc.compareEnable ? VK_TRUE : VK_FALSE,
            func::RHI_TO_VK_CompareOp(mDesc.compareOp),
            mDesc.mipLodBias,
            mDesc.minLod,
            mDesc.maxLod,
            func::RHI_TO_VK_BorderColor(mDesc.borderColor)
        );

        if (!mDesc.debugName.empty()) {
            mDevice->setObjectName(reinterpret_cast<uint64_t>(mSampler),
                VK_OBJECT_TYPE_SAMPLER,
                mDesc.debugName.c_str());
        }
    }

    VulkanDescriptorSetLayout::VulkanDescriptorSetLayout(std::shared_ptr<VulkanDevice>  device, const DescriptorSetLayoutDesc& desc)
        : mDevice(device), mDesc(desc) {

        std::vector<VkDescriptorSetLayoutBinding> vkBindings;
        vkBindings.reserve(mDesc.bindings.size());

        // 临时存储每个绑定对应的采样器指针数组（确保 pImmutableSamplers 指向有效内存）
        std::vector<std::vector<VkSampler>> perBindingSamplers;

        for (const auto& binding : mDesc.bindings) {
            VkDescriptorSetLayoutBinding vkBinding{};
            vkBinding.binding = binding.binding;
            vkBinding.descriptorType = func::RHI_TO_VK_DescriptorType(binding.type);
            vkBinding.descriptorCount = binding.count;
            vkBinding.stageFlags = static_cast<VkShaderStageFlags>(binding.stageFlags);
            vkBinding.pImmutableSamplers = nullptr;

            if (binding.immutableSamplers) {
                if (binding.type != DescriptorType::Sampler &&
                    binding.type != DescriptorType::CombinedImageSampler) {
                    throw std::runtime_error("Immutable samplers only allowed for Sampler or CombinedImageSampler");
                }

                std::vector<VkSampler> samplers;
                samplers.reserve(binding.count);
                for (uint32_t i = 0; i < binding.count; ++i) {
                    SamplerDesc samplerDesc = (i < binding.samplerDescs.size()) ? binding.samplerDescs[i] : SamplerDesc{};
                    VkSampler sampler = mDevice->createSampler(
                        func::RHI_TO_VK_Filter(samplerDesc.magFilter),
                        func::RHI_TO_VK_Filter(samplerDesc.minFilter),
                        func::RHI_TO_VK_AddressMode(samplerDesc.addressU),
                        func::RHI_TO_VK_AddressMode(samplerDesc.addressV),
                        func::RHI_TO_VK_AddressMode(samplerDesc.addressW),
                        samplerDesc.maxAnisotropy > 1.0f ? VK_TRUE : VK_FALSE,
                        samplerDesc.maxAnisotropy,
                        samplerDesc.compareEnable ? VK_TRUE : VK_FALSE,
                        func::RHI_TO_VK_CompareOp(samplerDesc.compareOp),
                        samplerDesc.mipLodBias,
                        samplerDesc.minLod,
                        samplerDesc.maxLod,
                        func::RHI_TO_VK_BorderColor(samplerDesc.borderColor)
                    );
                    samplers.push_back(sampler);
                    mImmutableSamplers.push_back(sampler);
                }
                perBindingSamplers.push_back(std::move(samplers));
                vkBinding.pImmutableSamplers = perBindingSamplers.back().data();
            }
            else {
                perBindingSamplers.emplace_back(); // 占位，保持索引对齐
            }

            vkBindings.push_back(vkBinding);
        }

        try {
            mLayout = mDevice->createDescriptorSetLayout(vkBindings);
        }
        catch (...) {
            for (auto sampler : mImmutableSamplers) {
                mDevice->destroySampler(sampler);
            }
            throw;
        }
    }

    VulkanDescriptorSetLayout::~VulkanDescriptorSetLayout() {
        release();
    }

    void VulkanDescriptorSetLayout::release() {
        mDevice->destroyDescriptorSetLayout(mLayout);

        for (auto sampler : mImmutableSamplers) {
            mDevice->destroySampler(sampler);
        }
        mImmutableSamplers.clear();
    }

    bool VulkanDescriptorSetLayout::isCompatibleWith(const RHIDescriptorSetLayout* other) const {
        auto* vkOther = dynamic_cast<const VulkanDescriptorSetLayout*>(other);
        if (!vkOther) return false;
        return mDesc.bindings == vkOther->mDesc.bindings;
    }

    VulkanDescriptorPool::VulkanDescriptorPool(std::shared_ptr<VulkanDevice>  device, const DescriptorPoolDesc& desc)
        : mDevice(device), mDesc(desc) {
        std::vector<VkDescriptorPoolSize> vkPoolSizes;
        for (const auto& size : desc.poolSizes) {
            VkDescriptorPoolSize vkSize{};
            vkSize.type = func::RHI_TO_VK_DescriptorType(size.first);
            vkSize.descriptorCount = size.second;
            vkPoolSizes.push_back(vkSize);
        }

        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.flags = desc.freeDescriptorSet ? VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT : 0;
        poolInfo.maxSets = desc.maxSets;
        poolInfo.poolSizeCount = static_cast<uint32_t>(vkPoolSizes.size());
        poolInfo.pPoolSizes = vkPoolSizes.data();

        if (vkCreateDescriptorPool(mDevice->getLogicalDevice(), &poolInfo, nullptr, &mPool) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create descriptor pool");
        }

        if (!desc.debugName.empty()) {
            mDevice->setObjectName(reinterpret_cast<uint64_t>(mPool),
                VK_OBJECT_TYPE_DESCRIPTOR_POOL,
                desc.debugName.c_str());
        }
    }

    void VulkanDescriptorPool::release() {
        mDevice->destroyDescriptorPool(mPool);
    }

    std::vector<std::unique_ptr<RHIDescriptorSet>> VulkanDescriptorPool::allocateDescriptorSets(
        const std::vector<RHIDescriptorSetLayout*>& layouts) {

        std::vector<VkDescriptorSetLayout> vkLayouts;
        for (auto* layout : layouts) {
            auto* vkLayout = dynamic_cast<VulkanDescriptorSetLayout*>(layout);
            if (!vkLayout) throw std::runtime_error("Invalid layout type");
            vkLayouts.push_back(static_cast<VkDescriptorSetLayout>(vkLayout->getNativeHandle()));
        }

        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = mPool;
        allocInfo.descriptorSetCount = static_cast<uint32_t>(vkLayouts.size());
        allocInfo.pSetLayouts = vkLayouts.data();

        std::vector<VkDescriptorSet> vkSets(vkLayouts.size());
        if (vkAllocateDescriptorSets(mDevice->getLogicalDevice(), &allocInfo, vkSets.data()) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate descriptor sets");
        }

        mAllocatedSets += static_cast<uint32_t>(vkSets.size());

        std::vector<std::unique_ptr<RHIDescriptorSet>> result;
        for (size_t i = 0; i < vkSets.size(); ++i) {
            result.push_back(std::make_unique<VulkanDescriptorSet>(mDevice, vkSets[i], this, layouts[i]));
        }
        return result;
    }

    void VulkanDescriptorPool::reset() {
        vkResetDescriptorPool(mDevice->getLogicalDevice(), mPool, 0);
        mAllocatedSets = 0;
    }

    uint32_t VulkanDescriptorPool::getRemainingSets() const {
        return mDesc.maxSets - mAllocatedSets.load();
    }

    VulkanDescriptorSet::VulkanDescriptorSet(std::shared_ptr<VulkanDevice>  device, VkDescriptorSet set,
        VulkanDescriptorPool* pool, RHIDescriptorSetLayout* layout)
        : mDevice(device), mSet(set), mPool(pool), mLayout(layout) {
    }

    VulkanDescriptorSet::~VulkanDescriptorSet() { release(); }

    void VulkanDescriptorSet::release() {
        mSet = VK_NULL_HANDLE;
    }

    void VulkanDescriptorSet::writeBuffer(uint32_t binding, uint32_t arrayElement,
        RHIBuffer* buffer, uint64_t offset, uint64_t range) {
        if (!buffer) return;

        VkDescriptorBufferInfo info{};
        info.buffer = static_cast<VkBuffer>(buffer->getNativeHandle());
        info.offset = offset;
        info.range = (range == 0) ? VK_WHOLE_SIZE : range;

        uint32_t idx = static_cast<uint32_t>(mBufferInfos.size());
        mBufferInfos.push_back(info);

        mPendingBufferWrites.push_back({ binding, arrayElement, idx });
    }

    void VulkanDescriptorSet::writeTexture(uint32_t binding, uint32_t arrayElement,
        RHITexture* texture, RHISampler* sampler, ImageLayout layout)
    {
        if (!texture) return;

        VkImageView imageView = static_cast<VkImageView>(texture->getSamplingView());
        if (imageView == VK_NULL_HANDLE) {
            std::cerr << "[DescriptorSet] Texture default view is null, skip write\n";
            return;
        }

        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageView = imageView;
        if (sampler) {
            imageInfo.sampler = static_cast<VkSampler>(sampler->getNativeHandle());
        }
        imageInfo.imageLayout = func::RHI_TO_VK_ImageLayout(layout);
        mImageInfos.push_back(imageInfo);

        PendingTextureWrite pending;
        pending.binding = binding;
        pending.arrayElement = arrayElement;
        pending.imageInfoIndex = static_cast<uint32_t>(mImageInfos.size() - 1);
        mPendingTextureWrites.push_back(pending);
    }

    void VulkanDescriptorSet::writeSampler(uint32_t binding, uint32_t arrayElement,
        RHISampler* sampler) {
        if (!sampler) return;

        VkDescriptorImageInfo info{};
        info.sampler = static_cast<VkSampler>(sampler->getNativeHandle());
        uint32_t idx = static_cast<uint32_t>(mImageInfos.size());
        mImageInfos.push_back(info);

        mPendingTextureWrites.push_back({ binding, arrayElement, idx });
    }

    void VulkanDescriptorSet::update() {
        if (mPendingBufferWrites.empty() && mPendingTextureWrites.empty()) return;

        std::vector<VkWriteDescriptorSet> vkWrites;
        vkWrites.reserve(mPendingBufferWrites.size() + mPendingTextureWrites.size());

        for (const auto& pw : mPendingBufferWrites) {
            VkWriteDescriptorSet write{};
            write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            write.dstSet = mSet;
            write.dstBinding = pw.binding;
            write.dstArrayElement = pw.arrayElement;
            write.descriptorCount = 1;
            write.descriptorType = getBindingDescriptorType(pw.binding);
            write.pBufferInfo = &mBufferInfos[pw.bufferInfoIndex];
            vkWrites.push_back(write);
        }

        for (const auto& pt : mPendingTextureWrites) {
            VkWriteDescriptorSet write{};
            write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            write.dstSet = mSet;
            write.dstBinding = pt.binding;
            write.dstArrayElement = pt.arrayElement;
            write.descriptorCount = 1;
            write.descriptorType = getBindingDescriptorType(pt.binding);
            write.pImageInfo = &mImageInfos[pt.imageInfoIndex];
            vkWrites.push_back(write);
        }

        mDevice->updateDescriptorSet(mSet, vkWrites);

        mBufferInfos.clear();
        mImageInfos.clear();
        mPendingBufferWrites.clear();
        mPendingTextureWrites.clear();
    }

    void VulkanDescriptorSet::writeTextureCustomView(
        uint32_t binding, uint32_t arrayElement,
        void* imageView,
        RHISampler* sampler,
        ImageLayout layout)
    {
        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageView = static_cast<VkImageView>(imageView);
        imageInfo.sampler = sampler ? static_cast<VkSampler>(sampler->getNativeHandle()) : VK_NULL_HANDLE;
        imageInfo.imageLayout = func::RHI_TO_VK_ImageLayout(layout);
        mImageInfos.push_back(imageInfo);

        PendingTextureWrite pending;
        pending.binding = binding;
        pending.arrayElement = arrayElement;
        pending.imageInfoIndex = static_cast<uint32_t>(mImageInfos.size() - 1);
        mPendingTextureWrites.push_back(pending);
    }

    void VulkanDescriptorSet::copyFrom(const RHIDescriptorSet* src, const std::vector<DescriptorCopy>& copies) {
        std::vector<VkCopyDescriptorSet> vkCopies;
        for (const auto& copy : copies) {
            auto* vkSrc = dynamic_cast<const VulkanDescriptorSet*>(src);
            if (!vkSrc) continue;

            VkCopyDescriptorSet vkCopy{};
            vkCopy.sType = VK_STRUCTURE_TYPE_COPY_DESCRIPTOR_SET;
            vkCopy.srcSet = vkSrc->mSet;
            vkCopy.srcBinding = copy.srcBinding;
            vkCopy.srcArrayElement = copy.srcArrayElement;
            vkCopy.dstSet = mSet;
            vkCopy.dstBinding = copy.dstBinding;
            vkCopy.dstArrayElement = copy.dstArrayElement;
            vkCopy.descriptorCount = copy.descriptorCount;
            vkCopies.push_back(vkCopy);
        }

        if (!vkCopies.empty()) {
            vkUpdateDescriptorSets(mDevice->getLogicalDevice(),
                0, nullptr,
                static_cast<uint32_t>(vkCopies.size()),
                vkCopies.data());
        }
    }

    void VulkanDescriptorSet::writeInputAttachment(uint32_t binding, uint32_t arrayElement,
        RHITexture* texture, ImageLayout layout) {
        if (!texture) return;

        VkImageView view = static_cast<VkImageView>(texture->getDefaultView());
        if (view == VK_NULL_HANDLE) {
            std::cerr << "[DescriptorSet] Input attachment view is null\n";
            return;
        }

        VkDescriptorImageInfo info{};
        info.imageView = view;
        info.imageLayout = func::RHI_TO_VK_ImageLayout(layout);
        info.sampler = VK_NULL_HANDLE;

        uint32_t idx = static_cast<uint32_t>(mImageInfos.size());
        mImageInfos.push_back(info);

        mPendingTextureWrites.push_back({ binding, arrayElement, idx });
    }

    VkDescriptorType VulkanDescriptorSet::getBindingDescriptorType(uint32_t binding) const {
        if (!mLayout) return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        const auto& bindings = mLayout->getBindings();
        for (const auto& b : bindings) {
            if (b.binding == binding) {
                return func::RHI_TO_VK_DescriptorType(b.type);
            }
        }
        return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    }

    // ===== 补充实现（此前缺失导致链接错误）=====

    size_t VulkanBuffer::getMemoryUsage() const {
        return mDesc.size;
    }



}
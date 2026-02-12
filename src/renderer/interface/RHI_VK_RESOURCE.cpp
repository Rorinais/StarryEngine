#include"RHI_VK_RESOURCE.hpp"

namespace StarryEngine::RHI {
    // ANSI 颜色代码定义
    namespace ANSIColor {
        const std::string RESET = "\033[0m";
        const std::string BLACK = "\033[30m";
        const std::string RED = "\033[31m";
        const std::string GREEN = "\033[32m";
        const std::string YELLOW = "\033[33m";
        const std::string BLUE = "\033[34m";
        const std::string MAGENTA = "\033[35m";
        const std::string CYAN = "\033[36m";
        const std::string WHITE = "\033[37m";

        // 背景色
        const std::string BG_BLACK = "\033[40m";
        const std::string BG_RED = "\033[41m";
        const std::string BG_GREEN = "\033[42m";
        const std::string BG_YELLOW = "\033[43m";
        const std::string BG_BLUE = "\033[44m";
        const std::string BG_MAGENTA = "\033[45m";
        const std::string BG_CYAN = "\033[46m";
        const std::string BG_WHITE = "\033[47m";

        // 样式
        const std::string BOLD = "\033[1m";
        const std::string UNDERLINE = "\033[4m";
        const std::string INVERSE = "\033[7m";
    }

    RHI_VK_ShaderModule::RHI_VK_ShaderModule(Device::Ptr device, ShaderModuleDesc desc)
        : mDevice(device), mDesc(desc), mShaderModule(VK_NULL_HANDLE) {

        try {
            auto spirv = compileGLSL(mDesc.sourcecode, FUNC::RHI_TO_Shaderc_ShaderKind(mDesc.stage), mDesc.defines, mDesc.debugName);
            mShaderModule = mDevice->createShaderModule(spirv, mDesc.debugName);
            std::cout << "[RHI_VK_ShaderModule] Created shader module: "<< mShaderModule << " for " << mDesc.debugName << std::endl;
        }
        catch (const std::exception& e) {
            std::cerr << "[RHI_VK_ShaderModule] ERROR: Failed to create shader: "<< mDesc.debugName << " - " << e.what() << std::endl;

            mShaderModule = VK_NULL_HANDLE;
            throw;
        }
    }

    void RHI_VK_ShaderModule::release() {
        mDevice->destroyShaderModule(mShaderModule);
    }

    std::vector<uint32_t> RHI_VK_ShaderModule::compileGLSL(
        const std::string& source,
        shaderc_shader_kind kind,
        const std::vector<std::pair<std::string, std::string>>& macros,
        const std::string& debugName) {
        shaderc::CompileOptions options;
        options.SetTargetEnvironment(
            shaderc_target_env_vulkan,
            shaderc_env_version_vulkan_1_2
        );
        options.SetOptimizationLevel(shaderc_optimization_level_performance);

        // 添加用户定义的宏
        for (const auto& [name, value] : macros) {
            if (value.empty()) {
                options.AddMacroDefinition(name);
            }
            else {
                options.AddMacroDefinition(name, value);
            }
        }

        shaderc::SpvCompilationResult result = mCompiler.CompileGlslToSpv(
            source, kind, debugName.c_str(), options
        );

        if (result.GetCompilationStatus() != shaderc_compilation_status_success) {
            throw std::runtime_error("Shader compile error: " + debugName + "\n" + result.GetErrorMessage());
        }

        return { result.cbegin(), result.cend() };
    }

    //RHI_VK_Buffer
    // ==================== 构造函数和析构函数 ====================
    RHI_VK_Buffer::RHI_VK_Buffer(std::shared_ptr<Device> device, const BufferDesc& desc)
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

		std::cout << ANSIColor::BG_RED<<"[RHI_VK_Buffer] Created buffer: " << ANSIColor::RESET<< mBuffer << " of size " << mDesc.size << " bytes with VMA: " << (mUsingVMA ? "Yes" : "No") << std::endl;
    }

    RHI_VK_Buffer::~RHI_VK_Buffer() {
        release();
    }

    // ==================== 缓冲区创建和销毁 ====================
    void RHI_VK_Buffer::createBuffer() {
        VkBufferUsageFlags usage = getBufferUsageFlags();
        
        try {
            if (mUsingVMA) {
                // VMA方式
                VmaMemoryUsage vmaUsage = FUNC::RHI_TO_VK_VmaMemoryUsage(mDesc.memoryType);
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
                VkMemoryPropertyFlags memoryProperties = FUNC::RHI_TO_VK_MemoryProperties(mDesc.memoryType);
                
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
            std::cerr << "[RHI_VK_Buffer] Failed to create buffer: " << e.what() << std::endl;
            throw;
        }
    }
    void RHI_VK_Buffer::destroyBuffer() {
        // 销毁所有视图
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
            } else if (mTraditionalMemory != VK_NULL_HANDLE) {
                mDevice->destroyBufferTraditional(mBuffer, mTraditionalMemory);
            } else {
                vkDestroyBuffer(mDevice->getLogicalDevice(), mBuffer, nullptr);
            }
            
            mBuffer = VK_NULL_HANDLE;
            mVmaAllocation = VK_NULL_HANDLE;
            mTraditionalMemory = VK_NULL_HANDLE;
        }
    }

    // ==================== 数据更新优化 ====================
    void RHI_VK_Buffer::update(const void* data, uint64_t size, uint64_t offset) {
        if (!data || size == 0) return;
        
        // 检查边界
        if (offset + size > mDesc.size) {
            throw std::runtime_error("Buffer update exceeds buffer size");
        }
        
        // 根据内存类型选择合适的更新方式
        if (isCPUVisible()) {
            updateDataViaDirectMapping(data, size, offset);
        } else {
            updateDataViaStagingBuffer(data, size, offset);
        }
    }

    void RHI_VK_Buffer::updateDataViaDirectMapping(const void* data, uint64_t size, uint64_t offset) {
        // 如果已经持久映射，直接使用现有指针
        if (mPersistentlyMapped && mIsMapped) {
            memcpy(static_cast<uint8_t*>(mMappedPointer) + offset, data, size);
            flush(offset, size);
            return;
        }
        
        // 否则使用RAII包装器进行临时映射
        if (mUsingVMA) {
            // 使用Device的uploadDataToVmaBuffer
            mDevice->uploadDataToVmaBuffer(mBuffer, mVmaAllocation, data, size, offset);
        } else {
            // 判断内存一致性
            bool hostCoherent = (FUNC::RHI_TO_VK_MemoryProperties(mDesc.memoryType) & 
                            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) != 0;
            
            // 使用Device的统一上传函数
            mDevice->uploadDataToTraditionalMemory(mTraditionalMemory, data, size, offset, hostCoherent);
        }
    }

    void RHI_VK_Buffer::updateDataViaStagingBuffer(const void* data, uint64_t size, uint64_t offset) {
        // 创建暂存缓冲区
        VMATraditionalBuffer stagingBuffer = mDevice->createBufferTraditional(
            size,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
        );
        
        // 上传数据到暂存缓冲区
        mDevice->uploadDataToTraditionalMemory(
            stagingBuffer.memory, data, size, 0, true);
        
        // 获取传输命令池并复制
        VkCommandPool transferPool = mDevice->getTransferCommandPool();
        mDevice->copyBuffer(transferPool, stagingBuffer.buffer, mBuffer, size);
        
        // 清理
        mDevice->destroyBufferTraditional(stagingBuffer);
    }

    // ==================== 内存映射优化 ====================
    void* RHI_VK_Buffer::map(uint64_t offset, uint64_t size) {
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
                std::cerr << "[RHI_VK_Buffer] Failed to map VMA memory: " << result << std::endl;
                return nullptr;
            }
        } else {
            // 传统映射
            VkResult result = vkMapMemory(mDevice->getLogicalDevice(), mTraditionalMemory, 
                                        offset, size, 0, &mMappedPointer);
            if (result != VK_SUCCESS) {
                std::cerr << "[RHI_VK_Buffer] Failed to map traditional memory: " << result << std::endl;
                return nullptr;
            }
        }
        
        mIsMapped = true;
        return mMappedPointer;
    }

    void RHI_VK_Buffer::unmap() {
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
    VkBufferUsageFlags RHI_VK_Buffer::getBufferUsageFlags() const {
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

    bool RHI_VK_Buffer::isCPUVisible() const {
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

    bool RHI_VK_Buffer::isGPUOnly() const {
        return mDesc.memoryType == MemoryType::GPU_Only;
    }

    // ==================== 视图管理 ====================
    void* RHI_VK_Buffer::createView(Format format, uint64_t offset, uint64_t size) {
        if (size == 0) {
            size = mDesc.size - offset;
        }
        
        VkBufferViewCreateInfo viewInfo = {};
        viewInfo.sType = VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO;
        viewInfo.buffer = mBuffer;
        viewInfo.format = FUNC::RHI_TO_VK_Format(format);
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

    void RHI_VK_Buffer::destroyView(void* view) {
        uint64_t key = reinterpret_cast<uint64_t>(view);
        if (auto it = mViews.find(key); it != mViews.end()) {
            vkDestroyBufferView(mDevice->getLogicalDevice(), it->second.view, nullptr);
            mViews.erase(it);
        }
    }
    

    // ==================== 其他函数 ====================
    void RHI_VK_Buffer::release() {
        destroyBuffer();
    }

    bool RHI_VK_Buffer::isValid() const {
        return mBuffer != VK_NULL_HANDLE;
    }

    void* RHI_VK_Buffer::getNativeHandle() const {
        return reinterpret_cast<void*>(mBuffer);
    }

    size_t RHI_VK_Buffer::getMemoryUsage() const {
        // 根据不同的内存分配方式获取内存使用量
        if (mUsingVMA && mVmaAllocation != VK_NULL_HANDLE) {
            VmaAllocationInfo allocInfo;
            vmaGetAllocationInfo(mDevice->getVmaAllocator(), mVmaAllocation, &allocInfo);
            return allocInfo.size;
        }
        return mDesc.size; // 对于传统方式，返回请求的大小
    }

    void RHI_VK_Buffer::flush(uint64_t offset, uint64_t size) {
        if (!mIsMapped || !isCPUVisible()) return;
        
        if (size == 0) {
            size = mDesc.size - offset;
        }
        
        // 根据内存类型决定是否需要刷新
        VkMemoryPropertyFlags properties = FUNC::RHI_TO_VK_MemoryProperties(mDesc.memoryType);
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

    void RHI_VK_Buffer::invalidate(uint64_t offset, uint64_t size) {
        if (!mIsMapped || !isCPUVisible()) return;
        
        if (size == 0) {
            size = mDesc.size - offset;
        }
        
        VkMemoryPropertyFlags properties = FUNC::RHI_TO_VK_MemoryProperties(mDesc.memoryType);
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

    RHI_VK_RenderPass::RHI_VK_RenderPass(Device::Ptr device, const RenderPassDesc& desc): mDevice(device), mDesc(desc) {
        // ---------- 附件描述转换 ----------
        std::vector<VkAttachmentDescription> vkAttachments;
        vkAttachments.reserve(mDesc.attachments.size());
        for (const auto& att : mDesc.attachments) {
            VkAttachmentDescription vkAtt = {};
            vkAtt.format = FUNC::RHI_TO_VK_Format(att.format);                      
            vkAtt.samples = static_cast<VkSampleCountFlagBits>(att.sampleCount); 
            vkAtt.loadOp = FUNC::RHI_TO_VK_AttachmentLoadOp(att.loadOp);            
            vkAtt.storeOp = FUNC::RHI_TO_VK_AttachmentStoreOp(att.storeOp);        
            vkAtt.stencilLoadOp = FUNC::RHI_TO_VK_AttachmentLoadOp(att.stencilLoadOp); 
            vkAtt.stencilStoreOp = FUNC::RHI_TO_VK_AttachmentStoreOp(att.stencilStoreOp); 
            vkAtt.initialLayout = FUNC::RHI_TO_VK_ImageLayout(att.initialLayout); 
            vkAtt.finalLayout = FUNC::RHI_TO_VK_ImageLayout(att.finalLayout);     
            vkAttachments.push_back(vkAtt);
        }

        // ---------- 子通道转换 ----------
        std::vector<VkSubpassDescription> vkSubpasses;
        std::vector<std::vector<VkAttachmentReference>> vkInputRefs;
        std::vector<std::vector<VkAttachmentReference>> vkColorRefs;
        std::vector<std::vector<VkAttachmentReference>> vkResolveRefs;
        std::vector<VkAttachmentReference> vkDepthStencilRefs;
        vkSubpasses.reserve(mDesc.subpasses.size());

        for (const auto& subpass : mDesc.subpasses) {
            // 输入附件
            vkInputRefs.emplace_back();
            auto& inputRefs = vkInputRefs.back();
            inputRefs.reserve(subpass.inputAttachments.size());
            for (const auto& ref : subpass.inputAttachments) {
                VkAttachmentReference vkRef = {
                    ref.attachment,
                    FUNC::RHI_TO_VK_ImageLayout(ref.layout)   
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
                    FUNC::RHI_TO_VK_ImageLayout(ref.layout)   
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
                    FUNC::RHI_TO_VK_ImageLayout(ref.layout)  
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
                depthRef.layout = FUNC::RHI_TO_VK_ImageLayout(subpass.depthStencilAttachment.layout); // 映射
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
            vkDep.srcStageMask = FUNC::RHI_TO_VK_PipelineStageFlags(dep.srcStageMask);
            vkDep.dstStageMask = FUNC::RHI_TO_VK_PipelineStageFlags(dep.dstStageMask); 
            vkDep.srcAccessMask = FUNC::RHI_TO_VK_AccessFlags(dep.srcAccessMask);      
            vkDep.dstAccessMask = FUNC::RHI_TO_VK_AccessFlags(dep.dstAccessMask);      
            vkDep.dependencyFlags = dep.byRegion ? VK_DEPENDENCY_BY_REGION_BIT : 0;
            vkDependencies.push_back(vkDep);
        }

        mVkRenderPass = mDevice->createRenderPass(vkAttachments, vkSubpasses, vkDependencies);
    }

    void RHI_VK_RenderPass::release() {
		mDevice->destroyRenderPass(mVkRenderPass);
    }

    //RHI_VK_PipelineLayout
    RHI_VK_PipelineLayout::RHI_VK_PipelineLayout(
        Device::Ptr device,
        const PipelineLayoutDesc& desc,
        std::vector<VkDescriptorSetLayout> vkDescriptorSetLayouts 
    ) :mDevice(device), mDesc(desc), mDescriptorSetLayouts(vkDescriptorSetLayouts) {
        std::vector<VkPushConstantRange> vkPushConstants;
        vkPushConstants.reserve(mDesc.pushConstants.size());

        for (const auto& range : mDesc.pushConstants) {
            VkPushConstantRange vkRange{};
            vkRange.stageFlags = static_cast<VkShaderStageFlags>(range.stage);
            vkRange.offset = range.offset;
            vkRange.size = range.size;
            vkPushConstants.push_back(vkRange);
        }
        mPipelineLayout = mDevice->createPipelineLayout(vkDescriptorSetLayouts, vkPushConstants);
    }

    void RHI_VK_PipelineLayout::release() {
        mDevice->destroyPipelineLayout(mPipelineLayout);
    }

    uint32_t RHI_VK_PipelineLayout::getBindingPoint(uint32_t set, uint32_t binding) const  {
        // 简化实现：返回绑定索引本身

        return binding;
    }

    size_t RHI_VK_PipelineLayout::getMemoryUsage() const  {
        size_t size = sizeof(*this);
        // 计算描述符集布局的内存使用

        return size;
    }

    //RHI_VK_Pipeline
    RHI_VK_Pipeline::RHI_VK_Pipeline(
        Device::Ptr device,
        const GraphicsPipelineDesc& pipelineDesc,
        PipelineType type,
        std::unique_ptr<RHI_VK_PipelineLayout> layout 
    ) : mDevice(device), mType(type), mDesc(pipelineDesc), mLayout(std::move(layout)) {

        if (!mLayout) {
            // 如果没有提供布局，使用描述符中的布局描述
            mLayout = std::make_unique<RHI_VK_PipelineLayout>(mDevice, pipelineDesc.layoutDesc);
        }
    }

    void RHI_VK_Pipeline::release(){
        mDevice->destroyPipeline(mPipeline);
        mLayout.reset();
    }

    void RHI_VK_Pipeline::createGraphicsPipeline() {
    }

    // 辅助函数：转换模板操作状态
    VkStencilOpState RHI_VK_Pipeline::convertStencilOpState(const StencilOpState& state) {
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

}
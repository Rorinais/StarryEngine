#include <renderer/utils/FrameCapture.hpp>

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/packing.hpp>

#include <logging/Logger.hpp>
#include <renderer/interface/IRHI.hpp>
#include <renderer/interface/RHIManager.hpp>
#include <renderer/interface/RHIEnums.hpp>

namespace StarryEngine {

    namespace {
        struct SrgbLut {
            uint8_t v[8192];
            SrgbLut() {
                for (int i = 0; i < 8192; ++i) {
                    float c = static_cast<float>(i) / 8191.0f;
                    float s = (c <= 0.0031308f) ? (12.92f * c)
                                                : (1.055f * std::pow(c, 1.0f / 2.4f) - 0.055f);
                    v[i] = static_cast<uint8_t>(s * 255.0f + 0.5f);
                }
            }
        };
        inline uint8_t linearToSrgb8(float c) {
            c = glm::clamp(c, 0.0f, 1.0f);
            static const SrgbLut lut;
            return lut.v[static_cast<int>(c * 8191.0f + 0.5f)];
        }

        inline uint8_t linearToUint8(float c) {
            return static_cast<uint8_t>(glm::clamp(c, 0.0f, 1.0f) * 255.0f + 0.5f);
        }

        uint32_t s_crcTable[256];
        bool s_crcInit = false;

        void initCrc() {
            for (uint32_t i = 0; i < 256; ++i) {
                uint32_t c = i;
                for (int k = 0; k < 8; ++k)
                    c = (c & 1u) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
                s_crcTable[i] = c;
            }
            s_crcInit = true;
        }

        uint32_t crc32(const uint8_t* data, size_t len) {
            if (!s_crcInit) initCrc();
            uint32_t c = 0xFFFFFFFFu;
            for (size_t i = 0; i < len; ++i)
                c = s_crcTable[(c ^ data[i]) & 0xFFu] ^ (c >> 8);
            return c ^ 0xFFFFFFFFu;
        }

        void putU32(std::vector<uint8_t>& out, uint32_t v) {
            out.push_back(static_cast<uint8_t>(v >> 24));
            out.push_back(static_cast<uint8_t>(v >> 16));
            out.push_back(static_cast<uint8_t>(v >> 8));
            out.push_back(static_cast<uint8_t>(v));
        }

        void putChunk(std::vector<uint8_t>& out, const char type[4],
                      const uint8_t* payload, size_t payloadLen) {
            putU32(out, static_cast<uint32_t>(payloadLen));
            size_t start = out.size();
            out.insert(out.end(), type, type + 4);
            if (payloadLen > 0) out.insert(out.end(), payload, payload + payloadLen);
            putU32(out, crc32(out.data() + start, out.size() - start));
        }

        bool encodePngStored(std::vector<uint8_t>& out, uint32_t w, uint32_t h, const uint8_t* rgba) {
            out.clear();
            static const uint8_t sig[8] = { 137, 80, 78, 71, 13, 10, 26, 10 };
            out.insert(out.end(), sig, sig + 8);

            uint8_t ihdr[13] = {};
            ihdr[0] = static_cast<uint8_t>(w >> 24); ihdr[1] = static_cast<uint8_t>(w >> 16);
            ihdr[2] = static_cast<uint8_t>(w >> 8);  ihdr[3] = static_cast<uint8_t>(w);
            ihdr[4] = static_cast<uint8_t>(h >> 24); ihdr[5] = static_cast<uint8_t>(h >> 16);
            ihdr[6] = static_cast<uint8_t>(h >> 8);  ihdr[7] = static_cast<uint8_t>(h);
            ihdr[8] = 8;   
            ihdr[9] = 6;   
            putChunk(out, "IHDR", ihdr, 13);

            size_t rowBytes = static_cast<size_t>(w) * 4;
            std::vector<uint8_t> filtered(rowBytes + 1);
            std::vector<uint8_t> idat;
            idat.reserve((rowBytes + 1) * h + h * 5 + 6);
            idat.push_back(0x78);  
            idat.push_back(0x01);   

            uint32_t a = 1, b = 0;  
            for (uint32_t y = 0; y < h; ++y) {
                const uint8_t* row = rgba + static_cast<size_t>(y) * rowBytes;
                filtered[0] = 1;    
                for (size_t i = 0; i < 4; ++i) filtered[1 + i] = row[i];           
                for (size_t i = 4; i < rowBytes; ++i)                               
                    filtered[1 + i] = static_cast<uint8_t>(row[i] - row[i - 4]);
                for (size_t i = 0; i <= rowBytes; ++i) {
                    a = (a + filtered[i]) % 65521u;
                    b = (b + a) % 65521u;
                }
                bool last = (y + 1 == h);
                idat.push_back(last ? 1u : 0u);   
                idat.push_back(static_cast<uint8_t>(rowBytes + 1));            
                idat.push_back(static_cast<uint8_t>((rowBytes + 1) >> 8));
                idat.push_back(static_cast<uint8_t>(~(rowBytes + 1)));              
                idat.push_back(static_cast<uint8_t>(~((rowBytes + 1) >> 8)));
                idat.insert(idat.end(), filtered.begin(), filtered.end());
            }
            putU32(idat, (b << 16) | a);  
            putChunk(out, "IDAT", idat.data(), idat.size());
            putChunk(out, "IEND", nullptr, 0);
            return true;
        }
    }

    FrameCapture::FrameCapture(std::shared_ptr<RHI::IRHI> rhi)
        : m_rhi(std::move(rhi)) {
        if (m_rhi) m_resMgr = m_rhi->getResourceManager();
    }

    FrameCapture::~FrameCapture() {
        if (m_rhi) m_rhi->waitIdle();
        {
            std::lock_guard<std::mutex> lock(m_workMutex);
            m_stop = true;
        }
        m_workCv.notify_all();
        for (auto& t : m_workers) {
            if (t.joinable()) t.join();
        }
        destroyStaging();
    }

    bool FrameCapture::initialize(const Config& cfg) {
        m_cfg = cfg;
        if (m_cfg.outputDir.empty()) {
            LOG_WARN("[FrameCapture] 输出目录为空，帧捕获未激活");
            return false;
        }
        std::error_code ec;
        if (!std::filesystem::create_directories(m_cfg.outputDir, ec) && ec) {
            LOG_ERROR("[FrameCapture] 创建输出目录失败: {} ({})", m_cfg.outputDir, ec.message());
            return false;
        }
        m_active = true;
        startWorker();
        if (m_cfg.frameIndex != UINT32_MAX) {
            LOG_INFO("[FrameCapture] 指定导出第 {} 帧 → {}（导出 worker 已启动）", m_cfg.frameIndex, m_cfg.outputDir);
        } else {
            LOG_INFO("[FrameCapture] 帧序列输出 → {}（{}，导出 worker 已启动）", m_cfg.outputDir,
                     m_cfg.frameCount == 0 ? "持续录制" : (std::to_string(m_cfg.frameCount) + " 帧"));
        }
        return true;
    }

    void FrameCapture::startWorker() {
        if (!m_workers.empty()) return;
        m_stop = false;
        for (uint32_t i = 0; i < kWorkers; ++i) {
            m_workers.emplace_back(&FrameCapture::workerMain, this);
        }
    }

    void FrameCapture::workerMain() {
        while (true) {
            PendingJob job;
            {
                std::unique_lock<std::mutex> lock(m_workMutex);
                m_workCv.wait(lock, [this] { return m_stop || !m_workQueue.empty(); });
                if (m_workQueue.empty()) break;          
                job = m_workQueue.front();
                m_workQueue.pop_front();
            }
            process(job);
        }
    }

    void FrameCapture::process(const PendingJob& job) {
        if (!m_rhi->waitAsyncReadback(job.slot, UINT64_MAX)) {
            LOG_WARN("[FrameCapture] 帧 {} 读回 fence 超时，跳过", job.frameIndex);
        } else if (auto* staging = m_resMgr->getBuffer(m_staging[job.slot])) {
            void* mapped = staging->map();
            if (mapped) {
                encodeAndWrite(job, static_cast<const uint16_t*>(mapped), m_cfg.outputDir);
                staging->unmap();
            } else {
                LOG_ERROR("[FrameCapture] 映射 staging 失败，帧 {} 丢弃", job.frameIndex);
            }
        }
        m_rhi->releaseAsyncReadback(job.slot);
        {
            std::lock_guard<std::mutex> lock(m_freeMutex);
            m_freeSlots.push_back(job.slot);
        }
        m_freeCv.notify_one();   
    }

    void FrameCapture::encodeAndWrite(const PendingJob& job, const uint16_t* half, const std::string& outputDir) {
        uint32_t w = job.width, h = job.height;
        size_t pixelCount = static_cast<size_t>(w) * h;

        std::vector<uint8_t> rgba8(pixelCount * 4);
        for (size_t i = 0; i < pixelCount; ++i) {
            const uint16_t* p = half + i * 4;
            float a = glm::unpackHalf1x16(p[3]);
            float invA = (a > 0.004f) ? (1.0f / a) : 0.0f;
            rgba8[i * 4 + 0] = linearToSrgb8(glm::unpackHalf1x16(p[0]) * invA);
            rgba8[i * 4 + 1] = linearToSrgb8(glm::unpackHalf1x16(p[1]) * invA);
            rgba8[i * 4 + 2] = linearToSrgb8(glm::unpackHalf1x16(p[2]) * invA);
            rgba8[i * 4 + 3] = linearToUint8(a);
        }

        char filename[64];
        std::snprintf(filename, sizeof(filename), "frame_%04u.png", job.frameIndex);
        std::string path = outputDir + "/" + filename;

        std::vector<uint8_t> png;
        if (!encodePngStored(png, w, h, rgba8.data())) {
            LOG_ERROR("[FrameCapture] PNG 编码失败: {}", path);
            return;
        }
        FILE* f = std::fopen(path.c_str(), "wb");
        if (!f) {
            LOG_ERROR("[FrameCapture] 打开输出文件失败: {}", path);
            return;
        }
        bool ok = std::fwrite(png.data(), 1, png.size(), f) == png.size();
        std::fclose(f);
        if (!ok) LOG_ERROR("[FrameCapture] 写文件失败: {}", path);
    }

    bool FrameCapture::ensureStaging(const RHI::Extent3D& extent) {
        uint32_t w = extent.width, h = extent.height;
        if (!m_staging.empty() && m_width == w && m_height == h) return true;

        if (!m_staging.empty()) {
            std::unique_lock<std::mutex> lock(m_freeMutex);
            m_freeCv.wait(lock, [this] { return m_stop || m_freeSlots.size() >= kSlots; });
        }
        destroyStaging();

        for (uint32_t i = 0; i < kSlots; ++i) {
            RHI::BufferDesc desc;
            desc.size = static_cast<uint64_t>(w) * h * 4 * 2;   
            desc.type = RHI::BufferType::Staging;
            desc.memoryType = RHI::MemoryType::GPU_To_CPU;
            desc.allowReadback = true;

            char name[64];
            std::snprintf(name, sizeof(name), "FrameCapture_Staging_%u", i);
            desc.debugName = name;
            auto handle = m_resMgr->createBuffer(desc, name);
            if (!handle.isValid()) {
                LOG_ERROR("[FrameCapture] 创建读回 staging buffer 失败 ({}x{})", w, h);
                destroyStaging();
                return false;
            }
            m_staging.push_back(handle);
            m_freeSlots.push_back(i);
        }
        m_width = w;
        m_height = h;
        return true;
    }

    void FrameCapture::destroyStaging() {
        for (auto& hnd : m_staging) {
            if (hnd.isValid()) m_resMgr->destroy(hnd);
        }
        m_staging.clear();
        {
            std::lock_guard<std::mutex> lock(m_freeMutex);
            m_freeSlots.clear();
        }
        m_width = m_height = 0;
    }

    bool FrameCapture::capture(RHI::RHITexture* tex) {
        if (!m_active) return false;

        uint64_t thisFrame = m_frameCounter++;
        if (m_cfg.frameIndex != UINT32_MAX) {
            if (thisFrame != m_cfg.frameIndex) return false;
            if (m_captured >= 1) return false;
        } else if (m_cfg.frameCount > 0 && m_captured >= m_cfg.frameCount) {
            return false;
        }

        if (!tex || tex->getType() != RHI::TextureType::Texture2D) return false;
        if (tex->getFormat() != RHI::Format::RGBA16_Float) {
            if (!m_formatWarned) {
                LOG_WARN("[FrameCapture] 期望 RGBA16_Float 最终纹理（引擎 SceneColor 约定），当前格式不匹配，帧捕获跳过");
                m_formatWarned = true;
            }
            return false;
        }

        auto extent = tex->getExtent();
        if (!ensureStaging(extent)) return false;

        uint32_t slot;
        {
            std::unique_lock<std::mutex> lock(m_freeMutex);
            m_freeCv.wait(lock, [this] { return m_stop || !m_freeSlots.empty(); });
            if (m_stop) return false;
            slot = m_freeSlots.back();
            m_freeSlots.pop_back();
        }

        auto* staging = m_resMgr->getBuffer(m_staging[slot]);
        if (!staging) {
            LOG_ERROR("[FrameCapture] staging buffer 丢失");
            std::lock_guard<std::mutex> lock(m_freeMutex);
            m_freeSlots.push_back(slot);
            m_freeCv.notify_one();
            return false;
        }

        RHI::ImageSubresourceRange range;
        range.aspectMask = RHI::ImageAspect::Color;
        range.baseMipLevel = 0;
        range.levelCount = 1;
        range.baseArrayLayer = 0;
        range.layerCount = 1;

        RHI::BufferImageCopyRegion region;
        region.bufferOffset = 0;
        region.bufferRowLength = 0;    
        region.bufferImageHeight = 0;
        region.imageSubresource = range;
        region.imageOffset = { 0, 0, 0 };
        region.imageExtent = extent;

        if (!m_rhi->beginAsyncReadback(slot, tex, staging, region)) {
            LOG_ERROR("[FrameCapture] beginAsyncReadback 失败，帧 {} 丢弃", thisFrame);
            std::lock_guard<std::mutex> lock(m_freeMutex);
            m_freeSlots.push_back(slot);
            m_freeCv.notify_one();
            return false;
        }

        PendingJob job;
        job.slot = slot;
        job.width = extent.width;
        job.height = extent.height;
        job.frameIndex = static_cast<uint32_t>(thisFrame);
        {
            std::lock_guard<std::mutex> lock(m_workMutex);
            m_workQueue.push_back(job);
        }
        m_workCv.notify_one();

        m_captured++;
        if (m_cfg.frameIndex == UINT32_MAX && m_cfg.frameCount > 0 && m_captured >= m_cfg.frameCount) {
            LOG_INFO("[FrameCapture] 已录满 {} 帧 → {}（后台导出中）", m_cfg.frameCount, m_cfg.outputDir);
        }
        return true;
    }

} // namespace StarryEngine

#pragma once

#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <renderer/interface/RHIHandles.hpp>
#include <renderer/interface/IRHIResources.hpp>

namespace StarryEngine::RHI {
    class IRHI;
    class ResourceManager;
}

namespace StarryEngine {

    class FrameCapture {
    public:
        struct Config {
            std::string outputDir = "frames";
            uint32_t frameCount = 1;              
            uint32_t frameIndex = UINT32_MAX;   
        };

        explicit FrameCapture(std::shared_ptr<RHI::IRHI> rhi);
        ~FrameCapture();

        FrameCapture(const FrameCapture&) = delete;
        FrameCapture& operator=(const FrameCapture&) = delete;

        bool initialize(const Config& cfg);

        bool capture(RHI::RHITexture* finalTexture);

        uint32_t capturedCount() const { return m_captured; }
        bool isActive() const { return m_active; }

    private:
        static constexpr uint32_t kSlots = 8;    
        static constexpr uint32_t kWorkers = 8;    

        struct PendingJob {
            uint32_t slot = 0;
            uint32_t width = 0;
            uint32_t height = 0;
            uint32_t frameIndex = 0;              
        };

        bool ensureStaging(const RHI::Extent3D& extent);
        void destroyStaging();
        void startWorker();
        void workerMain();
        void process(const PendingJob& job);
        static void encodeAndWrite(const PendingJob& job, const uint16_t* half, const std::string& outputDir);

        std::shared_ptr<RHI::IRHI> m_rhi;
        std::shared_ptr<RHI::ResourceManager> m_resMgr;

        Config m_cfg;
        std::vector<RHI::BufferHandle> m_staging;  
        uint32_t m_width = 0;
        uint32_t m_height = 0;
        uint64_t m_frameCounter = 0;              
        uint32_t m_captured = 0;               
        bool m_active = false;
        bool m_formatWarned = false;

        std::mutex m_freeMutex;
        std::condition_variable m_freeCv;
        std::vector<uint32_t> m_freeSlots;

        std::mutex m_workMutex;
        std::condition_variable m_workCv;
        std::deque<PendingJob> m_workQueue;
        std::vector<std::thread> m_workers;
        bool m_stop = false;
    };

} // namespace StarryEngine

#pragma once
#include <cstdint>
#include <functional>
#include <memory>

namespace StarryEngine {
    class JobSystem;

    namespace RHI {
        class RHICommandEncoder;
    }

    struct ParallelRecordingContext {
        JobSystem* jobs = nullptr;                                  // nullptr → 串行
        uint32_t workerCount = 1;                                   // 0 = 主线程内联
        std::function<std::unique_ptr<RHI::RHICommandEncoder>(uint32_t workerIndex)> allocateSecondary;
    };
} // namespace StarryEngine

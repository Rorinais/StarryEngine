#pragma once

namespace StarryEngine {

    struct ParticleParams {
        float gravity = 0.0f, speedMin = 0.8f, speedMax = 2.5f, lifetime = 3.0f;
        float spreadXZ = 0.8f, swayFreq = 2.7f, swayAmp = 0.8f;
        float emitterY = -3.0f, topDiffuse = 1.5f, topThreshold = 2.0f;
        float colorYoung[4]  = {1.0f, 0.95f, 0.5f, 0.0f};
        float colorMiddle[4] = {1.0f, 0.45f, 0.05f, 0.0f};
        float colorOld[4]    = {0.7f, 0.1f, 0.02f, 0.0f};
        float pointSizeMin = 2.0f, pointSizeMax = 8.0f;
    };

} // namespace StarryEngine

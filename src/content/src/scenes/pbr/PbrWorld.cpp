// PbrWorld.cpp — PBRDemo 的职责分片（工作区）
#include <content/scenes/pbr/PbrScene.hpp>
#include <renderer/renderPaths/LayerRegistry.hpp>
#include <assets/loader/MaterialDesc.hpp>
#include <assets/loader/TextureLoader.hpp>

    void PBRDemo::createScene() {
        initIBL();

        {        
            auto skyboxEffect = std::make_shared<Scene::ProceduralEffect>();
            skyboxEffect->material = createSkyboxMaterial();
            m_scene->addProceduralEffect(skyboxEffect);

            auto mat = createTexturedPbrMaterial();
            auto sphere = std::make_shared<Scene::RenderObject>();
            sphere->geometry = Assets::GeometryGenerator::createSphere(m_rhi->getResourceManager(), 1.0f);
            sphere->materials = { mat };
            sphere->transform = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 1.5f, 0.0f));

            {
                auto clip = std::make_shared<Assets::AnimationClip>();
                clip->name = "SpinBob";
                clip->duration = 4.0f;   
                clip->looping = true;
                for (int i = 0; i <= 8; ++i) {
                    float t = i * 0.5f;
                    Assets::TransformKeyframe kf;
                    kf.time = t;
                    kf.position = glm::vec3(0.0f, 1.5f + 0.3f * std::sin(t * glm::pi<float>() / 2.0f), 0.0f);
                    kf.rotation = glm::angleAxis(t * glm::pi<float>() / 2.0f, glm::vec3(0.0f, 1.0f, 0.0f));
                    kf.scale = glm::vec3(1.0f);
                    clip->keyframes.push_back(kf);
                }

                auto animator = std::make_shared<Scene::Animator>();
                animator->setClip(clip);
                animator->setSpeed(1.0f);
                sphere->animator = animator;
            }

            m_scene->addObject(sphere);

            auto groundMat = createTexturedPbrMaterial();
            auto ground = std::make_shared<Scene::RenderObject>();
            ground->geometry = Assets::GeometryGenerator::createCube(m_rhi->getResourceManager(), 20.0f, 0.3f, 20.0f);
            ground->materials = { groundMat };
            ground->transform = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -0.15f, 0.0f));

            m_scene->addObject(ground);
        }

        addGriseoModel();

        {
            for (auto& mat : m_skinnedMaterials) {
                if (!mat) continue;
                mat->setRenderLayer(RenderLayers::Outline);
                mat->setRenderQueue(0);
                RHI::StencilOpState write;
                write.failOp = RHI::StencilOp::Keep;
                write.passOp = RHI::StencilOp::Replace; 
                write.depthFailOp = RHI::StencilOp::Keep;
                write.compareOp = RHI::CompareOp::Always;
                write.compareMask = 0xFF;
                write.writeMask = 0xFF;
                write.reference = 1;
                mat->setStencilTest(true);
                mat->setStencilOps(write, write);
            }

            bool outlineOn = !StarryEngine::App::runtimeSettings().noOutline;
            m_outlineMaterial = outlineOn ? makeOutlineMaterial() : nullptr;
            if (m_outlineMaterial && m_modelGeometry) {
                auto outline = std::make_shared<Scene::RenderObject>();
                outline->geometry = m_modelGeometry;

                std::vector<std::shared_ptr<Assets::MaterialInstance>> outlineMats(m_modelMaterialCount, m_outlineMaterial);
                outline->materials = std::move(outlineMats);

                float footY = computeModelFootY(m_modelGeometry);
                LOG_INFO("[demo] 描边缩放中心（脚底）Y = {:.3f}", footY);
                outline->transform =
                    glm::translate(glm::mat4(1.0f), glm::vec3(2.0f, footY, 0.0f))
                    * glm::scale(glm::mat4(1.0f), glm::vec3(1.03f))
                    * glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -footY, 0.0f));
                m_scene->addObject(outline);
            }
        }

        {
            auto fire = std::make_shared<Scene::ParticleEmitter>();
            fire->name = "Fire";
            fire->particleCount = 1024;
            fire->computeShader = "assets/shaders/test/particle.comp";
            fire->material = makeParticleMaterial();
            fire->params.gravity  = -0.15f;
            fire->params.speedMin = 0.5f;
            fire->params.speedMax = 2.0f;
            fire->params.lifetime = 3.5f;
            fire->params.spreadXZ = 1.2f;
            fire->params.swayFreq = 2.7f;
            fire->params.swayAmp  = 0.6f;
            fire->params.emitterY = 0.0f;
            fire->params.topDiffuse   = 1.5f;
            fire->params.topThreshold = 2.5f;
            fire->params.colorYoung[0] = 1.0f; fire->params.colorYoung[1] = 0.9f; fire->params.colorYoung[2] = 0.2f;
            fire->params.colorMiddle[0]= 1.0f; fire->params.colorMiddle[1]= 0.4f; fire->params.colorMiddle[2]= 0.05f;
            fire->params.colorOld[0]   = 0.6f; fire->params.colorOld[1]   = 0.1f; fire->params.colorOld[2]   = 0.02f;
            fire->params.pointSizeMin  = 3.0f;
            fire->params.pointSizeMax  = 12.0f;
            m_scene->addParticleEmitter(fire);

            auto sparks = std::make_shared<Scene::ParticleEmitter>();
            sparks->name = "Sparks";
            sparks->particleCount = 256;
            sparks->computeShader = "assets/shaders/test/particle.comp";
            sparks->material = makeParticleMaterial();
            sparks->params.gravity  = -0.05f;
            sparks->params.speedMin = 0.3f;
            sparks->params.speedMax = 1.5f;
            sparks->params.lifetime = 2.0f;
            sparks->params.spreadXZ = 0.5f;
            sparks->params.swayFreq = 3.5f;
            sparks->params.swayAmp  = 0.4f;
            sparks->params.emitterY = 2.5f;
            sparks->params.topDiffuse   = 1.0f;
            sparks->params.topThreshold = 2.0f;
            sparks->params.colorYoung[0] = 0.0f; sparks->params.colorYoung[1] = 1.0f; sparks->params.colorYoung[2] = 0.0f;
            sparks->params.colorMiddle[0]= 0.0f; sparks->params.colorMiddle[1]= 0.8f; sparks->params.colorMiddle[2]= 0.0f;
            sparks->params.colorOld[0]   = 0.0f; sparks->params.colorOld[1]   = 0.4f; sparks->params.colorOld[2]   = 0.0f;
            sparks->params.pointSizeMin  = 10.0f;
            sparks->params.pointSizeMax  = 24.0f;
            m_scene->addParticleEmitter(sparks);
        }

        auto perspectiveCamera = std::make_shared<Scene::PerspectiveCamera>();
        perspectiveCamera->setPerspective(glm::radians(45.0f), (float)m_width / m_height, 0.1f, 100.0f);
        perspectiveCamera->lookAt(glm::vec3(1.57f, 1.53f, 2.77f), glm::vec3(1.65f, 1.45f, 1.78f), glm::vec3(0.01f, 1.0f, -0.07f));
        m_scene->addCamera(perspectiveCamera);
        m_scene->setActiveCamera(perspectiveCamera);

        if (m_boneProducer && m_renderer && m_renderer->getJobSystem()) {
            const auto& m0 = m_boneProducer->getFrame0Matrices();
            if (!m0.empty()) {
                size_t bytes = m0.size() * sizeof(glm::mat4);
                for (auto& mat : m_skinnedMaterials) if (mat) mat->setStorageBuffer(1, 2, m0.data(), bytes);
                if (m_outlineMaterial) m_outlineMaterial->setStorageBuffer(1, 2, m0.data(), bytes);
            }
        }
    }

#pragma once 
#include"Geometry.hpp"

namespace StarryEngine::Assets {
	class GeometryGenerator {
		public:
		static std::shared_ptr<Geometry> createSphere(std::shared_ptr<RHI::ResourceManager> resMgr, float radius, uint32_t sectorCount = 36, uint32_t stackCount = 18);
		static std::shared_ptr<Geometry> createGrid(std::shared_ptr<RHI::ResourceManager> resMgr, float size = 10.0f, uint32_t divisions = 10);
		static std::shared_ptr<Geometry> createCube(std::shared_ptr<RHI::ResourceManager> resMgr,float width = 1.0f,float height = 1.0f,float depth = 1.0f);
		static std::shared_ptr<Geometry> createQuad(std::shared_ptr<RHI::ResourceManager> resMgr, float width = 1.0f, float height = 1.0f);
		static std::shared_ptr<Geometry> createCylinder(std::shared_ptr<RHI::ResourceManager> resMgr, float bottomRadius, float topRadius, float height, uint32_t radialSegments, uint32_t heightSegments, bool topCap, bool bottomCap);
		static std::shared_ptr<Geometry> createFrustum(std::shared_ptr<RHI::ResourceManager> resMgr,float nearPlane, float farPlane, float fovDegrees, float aspectRatio);
	};

}
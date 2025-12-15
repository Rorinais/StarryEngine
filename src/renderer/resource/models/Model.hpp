#pragma once 
#include "../../../base.hpp"
#include "mesh/Mesh.hpp"
#include "boundingBox/BoundingBox.hpp"
#include "boundingBox/AxisAlignedBoundingBox.hpp"
namespace StarryEngine {

    class Model {
    public:

        const std::vector<Mesh>& getMeshes() const { return meshes; }
        std::shared_ptr<BoundingBox> getBoundingBox() const { return boundingBox; }

    private:
        std::vector<Mesh> meshes;

        std::string name = "DefaultModel";
        std::shared_ptr<BoundingBox> boundingBox;
        glm::mat4 globalTransform = glm::mat4(1.0f);
    };
}

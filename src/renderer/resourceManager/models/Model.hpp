#pragma once 
#include "../../../base.hpp"
#include "mesh/Mesh.hpp"
#include "boundingBox/BoundingBox.hpp"
namespace StarryEngine {

    class Model {
    public:

        const std::vector<Mesh>& getMeshes() const { return meshes; }
        const BoundingBox& getBoundingBox() const { return boundingBox; }

    private:
        std::vector<Mesh> meshes;

        std::string name = "DefaultModel";
        BoundingBox boundingBox;
        glm::mat4 globalTransform = glm::mat4(1.0f);
    };
}

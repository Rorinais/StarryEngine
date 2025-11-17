#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "../Geometry.hpp"
#include "../../boundingBox/BoundingBox.hpp"

namespace StarryEngine {

class Shape {
public:
	enum class Type { Cube, Sphere, Plane, Custom };
	virtual ~Shape() = default;
	virtual Geometry::Ptr generateGeometry()const = 0;
	virtual void getBoundingBox(glm::vec3& min, glm::vec3& max)const = 0;
	virtual Type getType()const = 0;
};

}
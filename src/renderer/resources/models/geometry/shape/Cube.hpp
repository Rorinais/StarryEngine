#include "Shape.hpp"
#include <array>
namespace StarryEngine {

	class Cube :public Shape {
	public:
		Cube(float length = 1.0f, float width = 1.0f, float height = 1.0f)
			:mLength(length), mWidth(width), mHeight(height) {
		}

		Geometry::Ptr generateGeometry()const override;
		void getBoundingBox(glm::vec3& min, glm::vec3& max)const override;
		Type getType()const override { return Type::Cube; }
		float getLength()const { return mLength; }
		float getWidth()const { return mWidth; }
		float getHeight()const { return mHeight; }

		void setOrigin(glm::vec3 point) { mOrigin = point; }
		glm::vec3 getOrigin() const { return mOrigin; }

	private:
		float mLength = 0.0f;
		float mWidth = 0.0f;
		float mHeight = 0.0f;

		glm::vec3 mOrigin{ 0.0f };
	};
}
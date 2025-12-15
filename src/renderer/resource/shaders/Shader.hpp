#pragma once
#include "ShaderProgram.hpp"
#include "ShaderBuilder.hpp"
namespace StarryEngine {
	class Shader {
	public:
		using Ptr = std::shared_ptr<Shader>;

		virtual ~Shader() = default;
		virtual void createShaderSource(const ShaderBuilder::Ptr& builder) = 0;
		virtual void compileShader() = 0;
		virtual void linkShaderProgram() = 0;
		virtual ShaderProgram::Ptr getShaderProgram() const = 0;
		virtual std::string getSource() const = 0;
		virtual ShaderType getType() const = 0;
		virtual std::string getName() const = 0;
	};
}

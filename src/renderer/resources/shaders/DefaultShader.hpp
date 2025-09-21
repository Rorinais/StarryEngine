#pragma once
#include "Shader.hpp"
namespace StarryEngine {
	class DefaultShader : public Shader {
	public:
		using Ptr = std::shared_ptr<DefaultShader>;
		static Ptr create(std::string n) {
			return std::make_shared<DefaultShader>(n);
		}
		DefaultShader(std::string n) : Shader(), name(n) {

		}
		~DefaultShader() override = default;

		void createShaderSource() override;
		void compileShader() override;
		void linkShaderProgram() override;

		ShaderType getType() const override {
			return type;
		}
		std::string getName() const override {
			return name;
		}
		std::string getSource() const override {
			return source;
		}
		ShaderProgram::Ptr getShaderProgram() const override {
			return shaderProgram;
		}
	private:
		LogicalDevice::Ptr mLogicalDevice;

		ShaderType type = ShaderType::Default;
		std::string name = "DefaultShader";
		std::string source = "";
		ShaderProgram::Ptr shaderProgram;
	};
}

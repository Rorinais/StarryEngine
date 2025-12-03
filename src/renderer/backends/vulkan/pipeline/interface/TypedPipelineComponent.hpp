#pragma once
#include"IPipelineStateComponent.hpp"
#include <string>
#include <memory>
namespace StarryEngine {
	template<class Derived,PipelineComponentType Type>
	class TypedPipelineComponent : public IPipelineStateComponent {
		public:
		static constexpr PipelineComponentType ComponentType = Type;

		PipelineComponentType getType() const override {
			return Type;
		}

		std::shared_ptr<IPipelineStateComponent> clone() const override {
			return std::make_shared<Derived>(static_cast<const Derived&>(*this));
		}
	protected:
		void setName(const std::string& name) { mName = name; }
		const std::string& getName() const override { return mName; }

	private:
		std::string mName;
	};
}
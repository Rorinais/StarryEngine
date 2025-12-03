#pragma once
#include"../interface/IPipelineStateComponent.hpp"
#include<memory>
#include<unordered_map>
#include<string>

namespace StarryEngine {
	class ComponentManager {
	public:
		ComponentManager() = default;
		~ComponentManager() = default;

		void registerComponent(const std::string& name, std::shared_ptr<IPipelineStateComponent> component) {
			mComponents[name] = component;
		}

		std::shared_ptr<IPipelineStateComponent> getComponent(const std::string& name) const {
			auto it = mComponents.find(name);
			if (it != mComponents.end()) {
				return it->second;
			}
			return nullptr;
		}

	private:

		std::unordered_map<std::string,std::shared_ptr<IPipelineStateComponent>> mComponents;
	};

}


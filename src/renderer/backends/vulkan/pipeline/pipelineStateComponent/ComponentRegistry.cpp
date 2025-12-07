#include "ComponentRegistry.hpp"

namespace StarryEngine {

    void ComponentRegistry::registerComponent(const std::string& name,
        std::shared_ptr<IPipelineStateComponent> component) {
        PipelineComponentType type = component->getType();
        mComponents[type][name] = component;
        mComponentNames[type].insert(name);
    }

    std::shared_ptr<IPipelineStateComponent> ComponentRegistry::getComponent(
        PipelineComponentType type,
        const std::string& name) const {

        auto typeIt = mComponents.find(type);
        if (typeIt == mComponents.end()) {
            return nullptr;
        }

        auto compIt = typeIt->second.find(name);
        if (compIt == typeIt->second.end()) {
            return nullptr;
        }

        return compIt->second;
    }

    std::vector<std::string> ComponentRegistry::getComponentNames(PipelineComponentType type) const {
        std::vector<std::string> names;
        auto it = mComponentNames.find(type);
        if (it != mComponentNames.end()) {
            names.assign(it->second.begin(), it->second.end());
        }
        return names;
    }

    std::vector<PipelineComponentType> ComponentRegistry::getRegisteredTypes() const {
        std::vector<PipelineComponentType> types;
        for (const auto& [type, _] : mComponents) {
            types.push_back(type);
        }
        return types;
    }

    void ComponentRegistry::setDefaultComponent(PipelineComponentType type, const std::string& name) {
        if (hasComponent(type, name)) {
            mDefaultComponents[type] = name;
        }
    }

    std::shared_ptr<IPipelineStateComponent> ComponentRegistry::getDefaultComponent(
        PipelineComponentType type) const {

        auto defaultIt = mDefaultComponents.find(type);
        if (defaultIt != mDefaultComponents.end()) {
            return getComponent(type, defaultIt->second);
        }
        return nullptr;
    }

    std::optional<std::string> ComponentRegistry::getDefaultComponentName(PipelineComponentType type) const {
        auto it = mDefaultComponents.find(type);
        if (it != mDefaultComponents.end()) {
            return it->second;
        }
        return std::nullopt;
    }

    bool ComponentRegistry::hasComponent(PipelineComponentType type, const std::string& name) const {
        auto typeIt = mComponents.find(type);
        if (typeIt == mComponents.end()) {
            return false;
        }
        return typeIt->second.find(name) != typeIt->second.end();
    }

    void ComponentRegistry::removeComponent(PipelineComponentType type, const std::string& name) {
        auto typeIt = mComponents.find(type);
        if (typeIt != mComponents.end()) {
            typeIt->second.erase(name);
            if (typeIt->second.empty()) {
                mComponents.erase(typeIt);
            }
        }

        auto nameIt = mComponentNames.find(type);
        if (nameIt != mComponentNames.end()) {
            nameIt->second.erase(name);
            if (nameIt->second.empty()) {
                mComponentNames.erase(nameIt);
            }
        }

        // 如果移除的组件是默认组件，则清除默认设置
        auto defaultIt = mDefaultComponents.find(type);
        if (defaultIt != mDefaultComponents.end() && defaultIt->second == name) {
            mDefaultComponents.erase(defaultIt);
        }
    }

    void ComponentRegistry::clear() {
        mComponents.clear();
        mComponentNames.clear();
        mDefaultComponents.clear();
    }

    size_t ComponentRegistry::getComponentCount() const {
        size_t count = 0;
        for (const auto& [_, components] : mComponents) {
            count += components.size();
        }
        return count;
    }

    size_t ComponentRegistry::getComponentCount(PipelineComponentType type) const {
        auto it = mComponents.find(type);
        return (it != mComponents.end()) ? it->second.size() : 0;
    }

} // namespace StarryEngine
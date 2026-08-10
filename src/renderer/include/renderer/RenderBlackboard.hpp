#pragma once
#include <any>
#include <typeindex>
#include <unordered_map>

namespace StarryEngine {

    // 类型键控的数据块（黑板）：render path 共享数据给 pass/executor 用。
    // put<T>(value) / get<T>() 按完整类型 T 索引（含指针/引用，如 put<Scene::Scene*>(scene)）。
    class RenderBlackboard {
    public:
        template<typename T>
        void put(T value) {
            m_data[std::type_index(typeid(T))] = std::make_any<T>(std::move(value));
        }

        template<typename T>
        T* get() {
            auto it = m_data.find(std::type_index(typeid(T)));
            if (it == m_data.end()) return nullptr;
            return std::any_cast<T>(&it->second);
        }

        template<typename T>
        const T* get() const {
            auto it = m_data.find(std::type_index(typeid(T)));
            if (it == m_data.end()) return nullptr;
            return std::any_cast<T>(&it->second);
        }

        void clear() { m_data.clear(); }

    private:
        std::unordered_map<std::type_index, std::any> m_data;
    };

} // namespace StarryEngine

#pragma once
#include <any>
#include <typeindex>
#include <unordered_map>

namespace StarryEngine {

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

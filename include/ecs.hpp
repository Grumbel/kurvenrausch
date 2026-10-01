#pragma once
#include "types.hpp"
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <functional>
#include <typeindex>
#include <memory>
#include <cassert>

namespace racer {

// Minimal, clean, data-oriented ECS.
// Entities are IDs. Components are plain structs stored in contiguous arrays.
// Systems are free functions or functors operating on component views.

class World {
public:
    Entity create() {
        Entity e = next_++;
        alive_.push_back(e);
        return e;
    }

    void destroy(Entity e) {
        // Lazy: mark dead, systems skip. For demo simplicity.
        dead_.insert(e);
    }

    bool alive(Entity e) const {
        return dead_.find(e) == dead_.end() && e < next_;
    }

    template <typename T>
    T& add(Entity e, T component = T{}) {
        auto& store = get_store<T>();
        store[e] = std::move(component);
        return store[e];
    }

    template <typename T>
    bool has(Entity e) const {
        auto it = stores_.find(std::type_index(typeid(T)));
        if (it == stores_.end()) return false;
        auto* map = static_cast<std::unordered_map<Entity, T>*>(it->second.get());
        return map->count(e) > 0;
    }

    template <typename T>
    T& get(Entity e) {
        return get_store<T>()[e];
    }

    template <typename T>
    const T& get(Entity e) const {
        return get_store<T>()[e];
    }

    template <typename T>
    void remove(Entity e) {
        get_store<T>().erase(e);
    }

    // Iterate all entities that have the given components.
    // Usage: world.view<Pos, Vel>([](Entity e, Pos& p, Vel& v){ ... });
    template <typename... Cs, typename Fn>
    void view(Fn&& fn) {
        // Simple: iterate all alive, check has all.
        for (Entity e : alive_) {
            if (!alive(e)) continue;
            if ((has<Cs>(e) && ...)) {
                fn(e, get<Cs>(e)...);
            }
        }
    }

    const std::vector<Entity>& entities() const { return alive_; }

private:
    template <typename T>
    std::unordered_map<Entity, T>& get_store() {
        auto key = std::type_index(typeid(T));
        auto it = stores_.find(key);
        if (it == stores_.end()) {
            auto ptr = std::make_shared<std::unordered_map<Entity, T>>();
            stores_[key] = ptr;
            return *ptr;
        }
        return *static_cast<std::unordered_map<Entity, T>*>(it->second.get());
    }

    template <typename T>
    const std::unordered_map<Entity, T>& get_store() const {
        auto key = std::type_index(typeid(T));
        auto it = stores_.find(key);
        assert(it != stores_.end());
        return *static_cast<const std::unordered_map<Entity, T>*>(it->second.get());
    }

    Entity next_ = 0;
    std::vector<Entity> alive_;
    std::unordered_set<Entity> dead_;
    // type-erased stores
    std::unordered_map<std::type_index, std::shared_ptr<void>> stores_;
};

} // namespace racer

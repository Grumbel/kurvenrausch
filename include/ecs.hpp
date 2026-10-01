#pragma once
#include "types.hpp"

#include <algorithm>
#include <cassert>
#include <memory>
#include <typeindex>
#include <unordered_map>
#include <vector>

namespace racer {

// Minimal Entity-Component-System.
// Entities are IDs. Each component type lives in its own hash map keyed by
// entity. This is not cache-optimal, but the game only has a handful of
// entities, so clarity wins.
class World {
public:
    Entity create() {
        Entity e = next_++;
        alive_.push_back(e);
        return e;
    }

    void destroy(Entity e) {
        auto it = std::find(alive_.begin(), alive_.end(), e);
        if (it == alive_.end()) return;
        alive_.erase(it);
        for (auto& [type, store] : stores_) {
            (void)type;
            store->erase(e);
        }
    }

    bool alive(Entity e) const {
        return std::find(alive_.begin(), alive_.end(), e) != alive_.end();
    }

    template <typename T>
    T& add(Entity e, T component = T{}) {
        auto& map = store<T>().map;
        map[e] = std::move(component);
        return map[e];
    }

    template <typename T>
    bool has(Entity e) const {
        const Store<T>* s = find_store<T>();
        return s && s->map.count(e) > 0;
    }

    template <typename T>
    T& get(Entity e) {
        return store<T>().map.at(e);
    }

    template <typename T>
    const T& get(Entity e) const {
        const Store<T>* s = find_store<T>();
        assert(s && "component type was never added");
        return s->map.at(e);
    }

    template <typename T>
    void remove(Entity e) {
        store<T>().map.erase(e);
    }

    // Iterate all entities that have all of the given components.
    // Usage: world.view<Pos, Vel>([](Entity e, Pos& p, Vel& v){ ... });
    template <typename... Cs, typename Fn>
    void view(Fn&& fn) {
        for (Entity e : alive_) {
            if ((has<Cs>(e) && ...)) {
                fn(e, get<Cs>(e)...);
            }
        }
    }

    const std::vector<Entity>& entities() const { return alive_; }

private:
    struct StoreBase {
        virtual ~StoreBase() = default;
        virtual void erase(Entity e) = 0;
    };

    template <typename T>
    struct Store final : StoreBase {
        std::unordered_map<Entity, T> map;
        void erase(Entity e) override { map.erase(e); }
    };

    template <typename T>
    Store<T>& store() {
        auto& slot = stores_[std::type_index(typeid(T))];
        if (!slot) slot = std::make_unique<Store<T>>();
        return static_cast<Store<T>&>(*slot);
    }

    template <typename T>
    const Store<T>* find_store() const {
        auto it = stores_.find(std::type_index(typeid(T)));
        if (it == stores_.end()) return nullptr;
        return static_cast<const Store<T>*>(it->second.get());
    }

    Entity next_ = 0;
    std::vector<Entity> alive_;
    std::unordered_map<std::type_index, std::unique_ptr<StoreBase>> stores_;
};

} // namespace racer

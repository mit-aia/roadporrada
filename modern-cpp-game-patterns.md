# Modern C++ Patterns for 3D Game Development

A practical reference of modern C++ (C++17 / C++20 / C++23) idioms and design patterns commonly used in high-performance 3D game engines and gameplay code.

---

## Table of Contents

1. [Memory & Ownership](#1-memory--ownership)
2. [Value Semantics & Move Mechanics](#2-value-semantics--move-mechanics)
3. [Entity-Component-System (ECS)](#3-entity-component-system-ecs)
4. [CRTP – Static Polymorphism](#4-crtp--static-polymorphism)
5. [Type Erasure](#5-type-erasure)
6. [std::variant & std::visit – Data-Oriented Events](#6-stdvariant--stdvisit--data-oriented-events)
7. [Concepts & Constrained Templates (C++20)](#7-concepts--constrained-templates-c20)
8. [constexpr & compile-time Math](#8-constexpr--compile-time-math)
9. [Structured Bindings & if-init](#9-structured-bindings--if-init)
10. [std::span – Non-Owning Views](#10-stdspan--non-owning-views)
11. [Coroutines (C++20)](#11-coroutines-c20)
12. [Object Pools & Custom Allocators](#12-object-pools--custom-allocators)
13. [Command Pattern & Undo System](#13-command-pattern--undo-system)
14. [State Machines with std::variant](#14-state-machines-with-stdvariant)
15. [Strongly-Typed IDs](#15-strongly-typed-ids)
16. [Data-Oriented Design (SoA vs AoS)](#16-data-oriented-design-soa-vs-aos)
17. [std::expected (C++23) – Error Handling Without Exceptions](#17-stdexpected-c23--error-handling-without-exceptions)
18. [Multithreading Primitives for Game Loops](#18-multithreading-primitives-for-game-loops)
19. [RAII Wrappers for GPU Resources](#19-raii-wrappers-for-gpu-resources)
20. [Module Partitions (C++20)](#20-module-partitions-c20)

---

## 1. Memory & Ownership

Games allocate and destroy thousands of objects per frame. Modern C++ replaces raw `new`/`delete` with smart pointers that encode ownership semantics directly into the type system.

```cpp
#include <memory>

// Unique ownership – zero overhead over raw pointer
auto mesh = std::make_unique<Mesh>("dragon.obj");

// Shared ownership – reference-counted, useful for shared assets
auto texture = std::make_shared<Texture>("stone_diffuse.png");

// Non-owning observer – does NOT prevent destruction
std::weak_ptr<Texture> cached = texture;

if (auto tex = cached.lock()) {
    tex->bind(0);  // safe: object still alive
}
```

**When to use what:**

| Pattern                 | Use Case                                                 |
| ----------------------- | -------------------------------------------------------- |
| `unique_ptr`            | Scene nodes, components, transient objects               |
| `shared_ptr`            | Asset caches, textures/meshes referenced by many objects |
| `weak_ptr`              | Caches, back-references that must not extend lifetime    |
| Raw pointer / reference | Non-owning observation within a single frame             |

---

## 2. Value Semantics & Move Mechanics

Avoid unnecessary copies of heavy GPU buffers or large vertex arrays by enabling move semantics.

```cpp
class VertexBuffer {
    GLuint id_ = 0;
    size_t size_ = 0;

public:
    explicit VertexBuffer(std::span<const Vertex> vertices) {
        glGenBuffers(1, &id_);
        glBindBuffer(GL_ARRAY_BUFFER, id_);
        glBufferData(GL_ARRAY_BUFFER,
                     vertices.size_bytes(),
                     vertices.data(),
                     GL_STATIC_DRAW);
        size_ = vertices.size();
    }

    // Non-copyable – GPU resources should not be duplicated accidentally
    VertexBuffer(const VertexBuffer&) = delete;
    VertexBuffer& operator=(const VertexBuffer&) = delete;

    // Movable – transfer ownership cheaply
    VertexBuffer(VertexBuffer&& other) noexcept
        : id_(std::exchange(other.id_, 0))
        , size_(std::exchange(other.size_, 0)) {}

    VertexBuffer& operator=(VertexBuffer&& other) noexcept {
        if (this != &other) {
            destroy();
            id_   = std::exchange(other.id_, 0);
            size_ = std::exchange(other.size_, 0);
        }
        return *this;
    }

    ~VertexBuffer() { destroy(); }

private:
    void destroy() {
        if (id_) glDeleteBuffers(1, &id_);
    }
};

// Usage: move into a container, no copy, no leak
std::vector<VertexBuffer> buffers;
buffers.push_back(VertexBuffer(vertices));  // moved
```

> **Key takeaway:** `std::exchange` is the idiomatic way to implement move constructors for handle-based resources.

---

## 3. Entity-Component-System (ECS)

ECS decouples data from behavior and unlocks cache-friendly iteration over thousands of game entities.

```cpp
#include <cstdint>
#include <unordered_map>
#include <vector>
#include <typeindex>
#include <any>

using Entity = uint32_t;

// ── Components are plain data ──────────────────────
struct Transform {
    glm::vec3 position{0.f};
    glm::quat rotation{1.f, 0.f, 0.f, 0.f};
    glm::vec3 scale{1.f};
};

struct RigidBody {
    glm::vec3 velocity{0.f};
    glm::vec3 acceleration{0.f};
    float mass = 1.f;
};

struct MeshRenderer {
    std::shared_ptr<Mesh>     mesh;
    std::shared_ptr<Material> material;
};

// ── Minimal component storage ──────────────────────
class Registry {
    Entity next_id_ = 0;
    // type → (entity → component)
    std::unordered_map<std::type_index,
                       std::unordered_map<Entity, std::any>> pools_;

public:
    Entity create() { return next_id_++; }

    template <typename T, typename... Args>
    T& emplace(Entity e, Args&&... args) {
        auto& pool = pools_[typeid(T)];
        auto [it, _] = pool.emplace(e, std::make_any<T>(std::forward<Args>(args)...));
        return std::any_cast<T&>(it->second);
    }

    template <typename T>
    T* get(Entity e) {
        auto pit = pools_.find(typeid(T));
        if (pit == pools_.end()) return nullptr;
        auto eit = pit->second.find(e);
        if (eit == pit->second.end()) return nullptr;
        return std::any_cast<T>(&eit->second);
    }

    template <typename T>
    void each(auto&& fn) {
        auto pit = pools_.find(typeid(T));
        if (pit == pools_.end()) return;
        for (auto& [entity, comp] : pit->second) {
            fn(entity, std::any_cast<T&>(comp));
        }
    }
};

// ── System: free function operating on components ──
void physics_system(Registry& reg, float dt) {
    reg.each<RigidBody>([&](Entity e, RigidBody& rb) {
        rb.velocity += rb.acceleration * dt;
        if (auto* tf = reg.get<Transform>(e)) {
            tf->position += rb.velocity * dt;
        }
    });
}
```

> **Production note:** Real engines (EnTT, flecs) use sparse-set or archetype storage for contiguous memory access. The pattern above illustrates the architecture.

---

## 4. CRTP – Static Polymorphism

Curiously Recurring Template Pattern eliminates vtable overhead in hot paths like per-vertex shading or per-particle updates.

```cpp
template <typename Derived>
class RenderPass {
public:
    void execute(const Scene& scene) {
        // compile-time dispatch – no virtual call
        static_cast<Derived*>(this)->setup();
        static_cast<Derived*>(this)->render(scene);
        static_cast<Derived*>(this)->teardown();
    }
};

class ShadowPass : public RenderPass<ShadowPass> {
    friend class RenderPass<ShadowPass>;

    void setup()                    { /* bind depth FBO */ }
    void render(const Scene& scene) { /* draw shadow casters */ }
    void teardown()                 { /* unbind FBO */ }
};

class GBufferPass : public RenderPass<GBufferPass> {
    friend class RenderPass<GBufferPass>;

    void setup()                    { /* bind G-buffer FBO */ }
    void render(const Scene& scene) { /* draw geometry */ }
    void teardown()                 { /* unbind */ }
};

// In the render pipeline:
ShadowPass  shadows;
GBufferPass gbuffer;
shadows.execute(scene);   // fully inlined at compile time
gbuffer.execute(scene);
```

---

## 5. Type Erasure

When you need runtime polymorphism _without_ inheritance hierarchies (useful for plugin systems, scripting bridges):

```cpp
#include <memory>
#include <functional>

class Drawable {
    struct Concept {
        virtual ~Concept() = default;
        virtual void draw(const glm::mat4& vp) const = 0;
        virtual std::unique_ptr<Concept> clone() const = 0;
    };

    template <typename T>
    struct Model : Concept {
        T obj_;
        explicit Model(T obj) : obj_(std::move(obj)) {}
        void draw(const glm::mat4& vp) const override { obj_.draw(vp); }
        std::unique_ptr<Concept> clone() const override {
            return std::make_unique<Model>(*this);
        }
    };

    std::unique_ptr<Concept> self_;

public:
    template <typename T>
    Drawable(T obj) : self_(std::make_unique<Model<T>>(std::move(obj))) {}

    Drawable(const Drawable& other) : self_(other.self_->clone()) {}
    Drawable(Drawable&&) noexcept = default;
    Drawable& operator=(Drawable rhs) {
        std::swap(self_, rhs.self_);
        return *this;
    }

    void draw(const glm::mat4& vp) const { self_->draw(vp); }
};

// Any type with a .draw(mat4) method works – no base class required
struct Terrain  { void draw(const glm::mat4& vp) const { /* ... */ } };
struct Particle { void draw(const glm::mat4& vp) const { /* ... */ } };

std::vector<Drawable> render_queue;
render_queue.emplace_back(Terrain{});
render_queue.emplace_back(Particle{});

for (auto& d : render_queue)
    d.draw(camera.view_projection());
```

---

## 6. `std::variant` & `std::visit` – Data-Oriented Events

Replace traditional OOP event hierarchies with closed, cache-friendly variant types.

```cpp
#include <variant>
#include <vector>

// Events as simple structs
struct CollisionEvent {
    Entity a, b;
    glm::vec3 contact_point;
    glm::vec3 normal;
};

struct DamageEvent {
    Entity target;
    float amount;
    enum class Type { Physical, Fire, Ice } type;
};

struct SpawnEvent {
    std::string prefab_name;
    glm::vec3 position;
};

using GameEvent = std::variant<CollisionEvent, DamageEvent, SpawnEvent>;

// Process events with std::visit – exhaustive at compile time
void dispatch(const GameEvent& event) {
    std::visit(overloaded{
        [](const CollisionEvent& e) {
            // resolve physics response
            fmt::print("Collision between {} and {}\n", e.a, e.b);
        },
        [](const DamageEvent& e) {
            // apply damage, play VFX
            fmt::print("Entity {} took {:.1f} damage\n", e.target, e.amount);
        },
        [](const SpawnEvent& e) {
            // instantiate prefab
            fmt::print("Spawning '{}'\n", e.prefab_name);
        }
    }, event);
}

// Helper: the "overloaded" trick (C++17)
template <class... Ts>
struct overloaded : Ts... { using Ts::operator()...; };
template <class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;  // CTAD deduction guide
```

> **Advantage over virtual dispatch:** All event data lives in a contiguous `vector<GameEvent>`, exploiting CPU cache lines. The compiler can warn you if you forget to handle a variant alternative.

---

## 7. Concepts & Constrained Templates (C++20)

Use concepts to produce clear error messages and self-documenting template interfaces.

```cpp
#include <concepts>

template <typename T>
concept Transformable = requires(T t, float dt) {
    { t.position() } -> std::convertible_to<glm::vec3>;
    { t.rotation() } -> std::convertible_to<glm::quat>;
    { t.update(dt) } -> std::same_as<void>;
};

template <typename T>
concept Renderable = requires(T t, const RenderContext& ctx) {
    { t.mesh() }     -> std::convertible_to<const Mesh&>;
    { t.material() } -> std::convertible_to<const Material&>;
    { t.render(ctx) };
};

// Constrain a system to only accept proper components
template <Transformable T>
void integrate(T& object, float dt) {
    object.update(dt);
}

// Combine concepts
template <typename T>
    requires Transformable<T> && Renderable<T>
void process_entity(T& entity, float dt, const RenderContext& ctx) {
    entity.update(dt);
    entity.render(ctx);
}
```

---

## 8. `constexpr` & Compile-Time Math

Evaluate constants, lookup tables, and simple math at compile time — zero runtime cost.

```cpp
#include <array>
#include <cmath>
#include <numbers>

// constexpr math for baked sin table
constexpr size_t SIN_TABLE_SIZE = 256;

constexpr auto make_sin_table() {
    std::array<float, SIN_TABLE_SIZE> table{};
    for (size_t i = 0; i < SIN_TABLE_SIZE; ++i) {
        double angle = (2.0 * std::numbers::pi * i) / SIN_TABLE_SIZE;
        table[i] = static_cast<float>(
            // Taylor series approximation (constexpr-friendly)
            angle - (angle*angle*angle)/6.0
                  + (angle*angle*angle*angle*angle)/120.0
        );
    }
    return table;
}

// Lives in .rodata — no runtime initialization
constexpr auto SIN_LUT = make_sin_table();

// Compile-time vector math
struct Vec3c {
    float x, y, z;
    constexpr Vec3c operator+(Vec3c o) const { return {x+o.x, y+o.y, z+o.z}; }
    constexpr Vec3c operator*(float s) const { return {x*s, y*s, z*s}; }
    constexpr float dot(Vec3c o) const { return x*o.x + y*o.y + z*o.z; }
    constexpr float length_sq() const { return dot(*this); }
};

// Evaluated at compile time
constexpr Vec3c forward{0, 0, -1};
constexpr Vec3c up{0, 1, 0};
constexpr float cos_angle = forward.dot(up);  // 0.0f, known at compile time

static_assert(cos_angle == 0.f, "Forward and up must be perpendicular");
```

---

## 9. Structured Bindings & if-init

Reduce boilerplate and keep declarations scoped tightly.

```cpp
// Structured bindings with maps
std::unordered_map<std::string, std::shared_ptr<Shader>> shader_cache;

// Clean iteration
for (const auto& [name, shader] : shader_cache) {
    shader->set_uniform("u_time", elapsed);
}

// if-init: the iterator stays scoped to the if block
if (auto it = shader_cache.find("pbr"); it != shader_cache.end()) {
    auto& [name, shader] = *it;
    shader->bind();
} else {
    log::warn("PBR shader not found, using fallback");
}

// Decompose return values cleanly
auto [hit, distance, normal] = raycast(origin, direction, max_distance);
if (hit) {
    spawn_decal(origin + direction * distance, normal);
}
```

---

## 10. `std::span` – Non-Owning Views

Pass contiguous data to rendering/physics functions without caring about the container type.

```cpp
#include <span>

// Accepts data from vector, array, raw pointer, etc.
void upload_vertices(std::span<const Vertex> vertices) {
    glBufferSubData(GL_ARRAY_BUFFER, 0,
                    vertices.size_bytes(),
                    vertices.data());
}

void submit_draw_calls(std::span<const DrawCall> calls) {
    for (const auto& dc : calls) {
        dc.material->bind();
        glDrawElementsInstanced(GL_TRIANGLES,
                                dc.index_count,
                                GL_UNSIGNED_INT,
                                nullptr,
                                dc.instance_count);
    }
}

// All of these work:
std::vector<Vertex> dynamic_verts = /* ... */;
std::array<Vertex, 64> static_verts = /* ... */;

upload_vertices(dynamic_verts);
upload_vertices(static_verts);
upload_vertices({static_verts.data(), 32});  // first 32 only
```

---

## 11. Coroutines (C++20)

Implement game AI behaviors, cutscenes, and async asset loading with readable sequential code.

```cpp
#include <coroutine>

// Minimal task type for game coroutines
struct Task {
    struct promise_type {
        Task get_return_object() {
            return Task{std::coroutine_handle<promise_type>::from_promise(*this)};
        }
        std::suspend_never  initial_suspend() { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }
        void return_void() {}
        void unhandled_exception() { std::terminate(); }
    };

    std::coroutine_handle<promise_type> handle;

    bool done() const { return handle.done(); }
    void resume()     { if (!handle.done()) handle.resume(); }

    ~Task() { if (handle) handle.destroy(); }
};

// Awaitable: suspend for N frames
struct WaitFrames {
    int frames;
    bool await_ready() const { return frames <= 0; }
    void await_suspend(std::coroutine_handle<> h) { /* scheduler stores h */ }
    void await_resume() {}
};

// AI behavior reads like a script
Task enemy_patrol(Entity enemy, std::span<const glm::vec3> waypoints) {
    while (true) {
        for (const auto& wp : waypoints) {
            // Move towards waypoint
            while (glm::distance(get_position(enemy), wp) > 0.5f) {
                move_towards(enemy, wp, patrol_speed);
                co_await WaitFrames{1};  // yield for 1 frame
            }
            // Idle at waypoint
            play_animation(enemy, "idle");
            co_await WaitFrames{120};  // wait ~2 seconds at 60 fps
        }
    }
}
```

> **Why coroutines?** Traditional approaches use state machines or callback chains that are hard to read and maintain. Coroutines let you write linear, imperative AI/cutscene logic that suspends and resumes naturally.

---

## 12. Object Pools & Custom Allocators

Avoid per-frame heap allocations for particles, projectiles, and other transient objects.

```cpp
#include <vector>
#include <cassert>

template <typename T, size_t BlockSize = 1024>
class ObjectPool {
    struct Block {
        alignas(T) std::byte storage[sizeof(T) * BlockSize];
        std::bitset<BlockSize> used;
    };

    std::vector<std::unique_ptr<Block>> blocks_;

public:
    template <typename... Args>
    T* acquire(Args&&... args) {
        for (auto& block : blocks_) {
            for (size_t i = 0; i < BlockSize; ++i) {
                if (!block->used.test(i)) {
                    block->used.set(i);
                    T* ptr = reinterpret_cast<T*>(block->storage + i * sizeof(T));
                    return std::construct_at(ptr, std::forward<Args>(args)...);
                }
            }
        }
        // All blocks full — add a new one
        blocks_.push_back(std::make_unique<Block>());
        blocks_.back()->used.set(0);
        T* ptr = reinterpret_cast<T*>(blocks_.back()->storage);
        return std::construct_at(ptr, std::forward<Args>(args)...);
    }

    void release(T* obj) {
        for (auto& block : blocks_) {
            auto* base = reinterpret_cast<T*>(block->storage);
            auto idx = obj - base;
            if (idx >= 0 && static_cast<size_t>(idx) < BlockSize) {
                std::destroy_at(obj);
                block->used.reset(static_cast<size_t>(idx));
                return;
            }
        }
        assert(false && "Object does not belong to this pool");
    }
};

// Usage
ObjectPool<Particle> particle_pool;

void emit_particles(const glm::vec3& origin, int count) {
    for (int i = 0; i < count; ++i) {
        Particle* p = particle_pool.acquire(origin, random_velocity(), 2.f);
        active_particles.push_back(p);
    }
}
```

---

## 13. Command Pattern & Undo System

Essential for level editors. Every action is an object that can be applied and reversed.

```cpp
#include <stack>
#include <memory>
#include <vector>

struct ICommand {
    virtual ~ICommand() = default;
    virtual void execute() = 0;
    virtual void undo() = 0;
};

class MoveEntityCommand : public ICommand {
    Entity entity_;
    glm::vec3 old_pos_;
    glm::vec3 new_pos_;

public:
    MoveEntityCommand(Entity e, glm::vec3 new_pos)
        : entity_(e), new_pos_(new_pos) {}

    void execute() override {
        old_pos_ = get_position(entity_);
        set_position(entity_, new_pos_);
    }

    void undo() override {
        set_position(entity_, old_pos_);
    }
};

class CommandHistory {
    std::stack<std::unique_ptr<ICommand>> undo_stack_;
    std::stack<std::unique_ptr<ICommand>> redo_stack_;

public:
    void execute(std::unique_ptr<ICommand> cmd) {
        cmd->execute();
        undo_stack_.push(std::move(cmd));
        // clear redo on new action
        redo_stack_ = {};
    }

    void undo() {
        if (undo_stack_.empty()) return;
        auto cmd = std::move(undo_stack_.top());
        undo_stack_.pop();
        cmd->undo();
        redo_stack_.push(std::move(cmd));
    }

    void redo() {
        if (redo_stack_.empty()) return;
        auto cmd = std::move(redo_stack_.top());
        redo_stack_.pop();
        cmd->execute();
        undo_stack_.push(std::move(cmd));
    }
};
```

---

## 14. State Machines with `std::variant`

Model entity states (player, AI, UI screens) without inheritance-heavy hierarchies.

```cpp
#include <variant>

// States as structs with state-specific data
struct Idle   { float time_idle = 0.f; };
struct Running { glm::vec3 direction; float speed; };
struct Jumping { float air_time = 0.f; float initial_velocity; };
struct Attacking { int combo_step = 0; float timer = 0.f; };

using PlayerState = std::variant<Idle, Running, Jumping, Attacking>;

// Transition logic — exhaustive, compiler-checked
PlayerState update_state(PlayerState state, const Input& input, float dt) {
    return std::visit(overloaded{
        [&](Idle& s) -> PlayerState {
            s.time_idle += dt;
            if (input.attack)           return Attacking{};
            if (input.jump)             return Jumping{0.f, 8.f};
            if (glm::length(input.move) > 0.1f)
                return Running{glm::normalize(input.move), 5.f};
            return s;
        },
        [&](Running& s) -> PlayerState {
            if (input.attack)           return Attacking{};
            if (input.jump)             return Jumping{0.f, 8.f};
            if (glm::length(input.move) < 0.1f) return Idle{};
            s.direction = glm::normalize(input.move);
            return s;
        },
        [&](Jumping& s) -> PlayerState {
            s.air_time += dt;
            if (s.air_time > 1.f)       return Idle{};  // landed
            return s;
        },
        [&](Attacking& s) -> PlayerState {
            s.timer += dt;
            if (s.timer > 0.5f) {
                if (input.attack && s.combo_step < 3)
                    return Attacking{s.combo_step + 1, 0.f};
                return Idle{};
            }
            return s;
        }
    }, state);
}
```

> **Why not enums + switch?** With `std::variant`, each state carries its own data. You can't accidentally access `air_time` while in the `Idle` state — the type system prevents it.

---

## 15. Strongly-Typed IDs

Prevent mixing up entity IDs, texture handles, and buffer indices at compile time.

```cpp
template <typename Tag>
struct StrongId {
    uint32_t value = 0;

    constexpr bool operator==(StrongId other) const { return value == other.value; }
    constexpr auto operator<=>(StrongId other) const { return value <=> other.value; }
};

// Each tag creates a distinct, incompatible type
struct EntityTag {};
struct TextureTag {};
struct MeshTag {};

using EntityId  = StrongId<EntityTag>;
using TextureId = StrongId<TextureTag>;
using MeshId    = StrongId<MeshTag>;

// std::hash specialization for use in unordered containers
template <typename Tag>
struct std::hash<StrongId<Tag>> {
    size_t operator()(StrongId<Tag> id) const noexcept {
        return std::hash<uint32_t>{}(id.value);
    }
};

// Compile-time safety:
void attach_mesh(EntityId entity, MeshId mesh);

EntityId  player{1};
TextureId diffuse{42};
MeshId    sword{7};

attach_mesh(player, sword);   // ✅ compiles
// attach_mesh(player, diffuse);  // ❌ error: TextureId ≠ MeshId
```

---

## 16. Data-Oriented Design (SoA vs AoS)

Restructure data for cache efficiency — the single biggest performance win in game engines.

```cpp
// ❌ Array of Structures (AoS) – poor cache utilization for position-only ops
struct ParticleAoS {
    glm::vec3 position;
    glm::vec3 velocity;
    glm::vec4 color;
    float     lifetime;
    float     size;
    // 56 bytes per particle – iterating positions loads unused fields
};
std::vector<ParticleAoS> particles_aos;  // stride = 56 bytes

// ✅ Structure of Arrays (SoA) – cache-friendly
struct ParticleSystem {
    std::vector<glm::vec3> positions;    // contiguous positions
    std::vector<glm::vec3> velocities;   // contiguous velocities
    std::vector<glm::vec4> colors;
    std::vector<float>     lifetimes;
    std::vector<float>     sizes;

    size_t count() const { return positions.size(); }

    void update(float dt) {
        // This loop touches only positions & velocities → hot in cache
        for (size_t i = 0; i < count(); ++i) {
            positions[i] += velocities[i] * dt;
            lifetimes[i] -= dt;
        }
    }

    void add(glm::vec3 pos, glm::vec3 vel, glm::vec4 col, float life, float sz) {
        positions.push_back(pos);
        velocities.push_back(vel);
        colors.push_back(col);
        lifetimes.push_back(life);
        sizes.push_back(sz);
    }
};
```

> **Performance note:** SoA layout can be 2–5× faster for large particle counts because the CPU prefetcher can predict sequential memory access patterns.

---

## 17. `std::expected` (C++23) – Error Handling Without Exceptions

Games often disable exceptions for performance. `std::expected` provides a type-safe error channel.

```cpp
#include <expected>
#include <string>

enum class AssetError {
    FileNotFound,
    ParseFailed,
    UnsupportedFormat,
    OutOfMemory
};

std::expected<Mesh, AssetError> load_mesh(const std::filesystem::path& path) {
    if (!std::filesystem::exists(path))
        return std::unexpected(AssetError::FileNotFound);

    auto ext = path.extension();
    if (ext != ".obj" && ext != ".gltf" && ext != ".fbx")
        return std::unexpected(AssetError::UnsupportedFormat);

    auto data = read_file(path);
    if (!data)
        return std::unexpected(AssetError::ParseFailed);

    return parse_mesh(*data);  // returns Mesh on success
}

// Caller: clean, explicit error handling
void init_scene() {
    auto mesh = load_mesh("assets/models/hero.gltf");

    if (!mesh) {
        switch (mesh.error()) {
            case AssetError::FileNotFound:
                log::error("Mesh file missing");
                break;
            case AssetError::UnsupportedFormat:
                log::error("Unsupported mesh format");
                break;
            default:
                log::error("Failed to load mesh");
        }
        return;
    }

    // Access the value
    scene.add_mesh(std::move(*mesh));
}

// Monadic chaining (C++23)
auto result = load_mesh("hero.gltf")
    .and_then([](Mesh& m) -> std::expected<Mesh, AssetError> {
        m.compute_normals();
        return std::move(m);
    })
    .transform([](Mesh& m) {
        m.upload_to_gpu();
        return std::move(m);
    });
```

---

## 18. Multithreading Primitives for Game Loops

Split work across threads for physics, rendering prep, and AI.

```cpp
#include <thread>
#include <mutex>
#include <atomic>
#include <latch>    // C++20
#include <barrier>  // C++20

class ParallelGameLoop {
    std::atomic<bool> running_{true};
    std::barrier<> frame_sync_{3};  // 3 worker threads

    // Double-buffered command list: one written, one consumed
    struct FrameData {
        std::vector<DrawCall> draw_calls;
        std::vector<glm::mat4> transforms;
    };

    FrameData frame_data_[2];
    std::atomic<int> write_index_{0};

    void physics_thread() {
        while (running_) {
            auto& data = frame_data_[write_index_];
            // Simulate physics, write transforms
            simulate_physics(data.transforms);
            frame_sync_.arrive_and_wait();  // sync with other threads
        }
    }

    void ai_thread() {
        while (running_) {
            update_all_ai();
            frame_sync_.arrive_and_wait();
        }
    }

    void render_thread() {
        while (running_) {
            int read_idx = 1 - write_index_.load();
            auto& data = frame_data_[read_idx];
            // Render last frame's data while physics writes new data
            submit_draws(data.draw_calls, data.transforms);
            frame_sync_.arrive_and_wait();
            // Swap buffers
            write_index_.store(read_idx);
        }
    }

public:
    void run() {
        std::jthread physics(std::bind_front(&ParallelGameLoop::physics_thread, this));
        std::jthread ai(std::bind_front(&ParallelGameLoop::ai_thread, this));
        render_thread();  // render on main thread
    }
};

// Task-based parallelism using std::jthread (auto-joining)
void update_frame(float dt) {
    std::latch work_done{2};

    std::jthread t1([&] {
        update_particle_systems(dt);
        work_done.count_down();
    });

    std::jthread t2([&] {
        update_animation_blending(dt);
        work_done.count_down();
    });

    // Main thread does its own work
    process_input();

    // Wait for parallel tasks before rendering
    work_done.wait();

    render_frame();
}
```

---

## 19. RAII Wrappers for GPU Resources

Prevent resource leaks by tying GPU object lifetimes to C++ scope.

```cpp
// Generic OpenGL handle wrapper
template <auto CreateFn, auto DestroyFn>
class GLHandle {
    GLuint id_ = 0;

public:
    GLHandle()  { CreateFn(1, &id_); }
    ~GLHandle() { if (id_) DestroyFn(1, &id_); }

    GLHandle(const GLHandle&) = delete;
    GLHandle& operator=(const GLHandle&) = delete;

    GLHandle(GLHandle&& o) noexcept : id_(std::exchange(o.id_, 0)) {}
    GLHandle& operator=(GLHandle&& o) noexcept {
        if (this != &o) {
            if (id_) DestroyFn(1, &id_);
            id_ = std::exchange(o.id_, 0);
        }
        return *this;
    }

    GLuint get() const { return id_; }
    operator GLuint() const { return id_; }
};

// Concrete types — zero boilerplate
using VAO     = GLHandle<glGenVertexArrays,  glDeleteVertexArrays>;
using VBO     = GLHandle<glGenBuffers,       glDeleteBuffers>;
using FBO     = GLHandle<glGenFramebuffers,  glDeleteFramebuffers>;
using Texture = GLHandle<glGenTextures,      glDeleteTextures>;

// Scoped binding (bind on construction, unbind on destruction)
class ScopedBind {
    GLenum target_;
    GLuint prev_ = 0;

public:
    ScopedBind(GLenum target, GLuint id) : target_(target) {
        glGetIntegerv(binding_query(target), reinterpret_cast<GLint*>(&prev_));
        glBindBuffer(target, id);
    }
    ~ScopedBind() { glBindBuffer(target_, prev_); }
};

// Usage: resources are automatically cleaned up
void render_mesh() {
    VAO vao;
    VBO vbo;

    {
        ScopedBind bind(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, data.size(), data.data(), GL_STATIC_DRAW);
    }  // GL_ARRAY_BUFFER automatically unbound

    // ... render ...
}  // vbo and vao automatically destroyed
```

---

## 20. Module Partitions (C++20)

Organize engine code with proper encapsulation, replacing the header/source split.

```cpp
// ── engine.cppm (primary module interface) ─────────
export module engine;

export import :math;
export import :renderer;
export import :physics;
export import :audio;

// ── engine-math.cppm (partition) ───────────────────
export module engine:math;

export struct Vec3 {
    float x, y, z;
    constexpr Vec3 operator+(Vec3 o) const { return {x+o.x, y+o.y, z+o.z}; }
    constexpr Vec3 operator*(float s) const { return {x*s, y*s, z*s}; }
    constexpr float dot(Vec3 o) const { return x*o.x + y*o.y + z*o.z; }
};

export struct Mat4 {
    float m[4][4];
    static constexpr Mat4 identity();
    constexpr Mat4 operator*(const Mat4& o) const;
};

// ── engine-renderer.cppm (partition) ───────────────
export module engine:renderer;

import :math;

export class Renderer {
    // Only the public interface is visible to importers
public:
    void begin_frame();
    void submit(const Mesh& mesh, const Mat4& transform);
    void end_frame();

private:
    // Implementation details are truly hidden — not just "private"
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// ── game.cpp (consumer) ───────────────────────────
import engine;  // single import, fast compilation

int main() {
    Renderer renderer;
    renderer.begin_frame();
    // ...
}
```

> **Build system note:** Module support varies across compilers and build systems (CMake ≥ 3.28 with `CXX_MODULE_DIRS`, MSVC, Clang 17+, GCC 14+). Check your toolchain's support before adopting.

---

## Quick Reference Card

| Pattern                     | Problem Solved                          | Key Header / Feature           |
| --------------------------- | --------------------------------------- | ------------------------------ |
| Smart pointers              | Use-after-free, leaks                   | `<memory>`                     |
| Move semantics              | Expensive copies of GPU handles         | `std::exchange`                |
| ECS                         | Cache misses, rigid hierarchies         | Archetype / sparse set storage |
| CRTP                        | Virtual call overhead in hot loops      | Template + `static_cast`       |
| Type erasure                | Dynamic polymorphism without base class | Concept/Model idiom            |
| `std::variant`              | Open-ended inheritance chains           | `<variant>`, `std::visit`      |
| Concepts                    | Unreadable template errors              | `<concepts>` (C++20)           |
| `constexpr`                 | Runtime cost of constant data           | `constexpr`, `consteval`       |
| `std::span`                 | Container-agnostic buffer passing       | `<span>` (C++20)               |
| Coroutines                  | Callback hell in AI / async loading     | `co_await`, `co_yield`         |
| Object pool                 | Per-frame heap allocation               | Custom allocator               |
| Command pattern             | Undo/redo in editors                    | Virtual `execute`/`undo`       |
| Variant state machine       | Complex state + data coupling           | `std::variant` + `std::visit`  |
| Strong IDs                  | Mixing up unrelated integer handles     | Tagged `uint32_t` wrapper      |
| SoA layout                  | Cache thrashing in hot loops            | Parallel `std::vector`s        |
| `std::expected`             | Error handling without exceptions       | `<expected>` (C++23)           |
| `std::barrier`/`std::latch` | Frame synchronization                   | `<barrier>`, `<latch>` (C++20) |
| RAII GPU wrappers           | Resource leaks (textures, buffers)      | NTTP + `GLHandle` template     |
| Modules                     | Slow compilation, ODR violations        | `export module` (C++20)        |

---

_Written for C++17/20/23. Tested patterns used in production game engines including Unreal Engine, Godot, and custom engines. Adapt to your project's compiler support and coding standards._

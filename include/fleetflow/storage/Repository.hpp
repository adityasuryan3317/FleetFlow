#pragma once

#include "fleetflow/storage/EntityTraits.hpp"
#include "fleetflow/core/Exceptions.hpp"

#include <unordered_map>
#include <memory>
#include <vector>
#include <string>
#include <utility>
#include <cstddef>

namespace fleetflow::storage {

/**
 * @brief Generic in-memory repository managing exclusive RAII ownership of domain entities.
 *
 * @tparam T Domain entity type (can be abstract base class e.g. Vehicle, User).
 * @tparam Traits Compile-time trait struct providing static int getId(const T&).
 *
 * @note Ownership & Pointer Lifetime Contract:
 * - This repository takes exclusive ownership of entities via std::unique_ptr<T>.
 * - Non-owning raw pointers (T* or const T*) returned by getById() and getAll()
 *   remain valid ONLY while the referenced entity remains in the repository,
 *   remove() or clear() is not called on that entity, and the repository instance is alive.
 * - Copying is explicitly deleted to prevent duplicate ownership of unique_ptr.
 * - Move semantics are supported, transferring the entire underlying table without reallocation.
 *
 * @note Complexity:
 * - add(), getById(), remove(), and exists() provide average O(1) time complexity.
 *   Worst-case complexity is O(N) in the event of hash collisions or map rehashing.
 */
template <typename T, typename Traits = EntityTraits<T>>
class Repository {
private:
    std::unordered_map<int, std::unique_ptr<T>> items;

public:
    Repository() = default;
    ~Repository() = default;

    // Disallow copy operations to prevent duplicate unique_ptr ownership
    Repository(const Repository&) = delete;
    Repository& operator=(const Repository&) = delete;

    // Support move operations
    Repository(Repository&&) noexcept = default;
    Repository& operator=(Repository&&) noexcept = default;

    /**
     * @brief Adds an entity (base or derived) to the repository, taking exclusive ownership on success.
     *
     * Validates that the entity is non-null and that its ID does not already exist.
     * If insertion fails (e.g. duplicate ID), the caller's unique_ptr is NOT moved from
     * and retains full ownership of the object.
     *
     * @tparam Derived Entity type (T or a class publicly derived from T).
     * @param item Lvalue reference to std::unique_ptr<Derived>.
     * @throws core::ValidationException if item is nullptr.
     * @throws core::DuplicateEntityException if an entity with the same ID already exists.
     */
    template <typename Derived,
              typename = std::enable_if_t<std::is_base_of_v<T, Derived>>>
    void add(std::unique_ptr<Derived>& item) {
        if (!item) {
            throw core::ValidationException("Cannot insert null entity into Repository.");
        }
        const int id = Traits::getId(*item);
        // Pre‑check for duplicate ID to keep the original validation message.
        if (items.find(id) != items.end()) {
            throw core::DuplicateEntityException(
                "Entity with ID #" + std::to_string(id) + " already exists in repository.");
        }
        // Use try_emplace to allocate map node before moving ownership.
        auto [it, inserted] = items.try_emplace(id, nullptr);
        if (!inserted) {
            // This should not happen because of the pre‑check, but guard against race‑conditions.
            throw core::DuplicateEntityException(
                "Entity with ID already exists.");
        }
        it->second = std::move(item);
    }

    /**
     * @brief Overload supporting rvalues (e.g. std::make_unique<Car>(...)).
     */
    template <typename Derived,
              typename = std::enable_if_t<std::is_base_of_v<T, Derived>>>
    void add(std::unique_ptr<Derived>&& item) {
        add(item);
    }

    /**
     * @brief Retrieves a non-owning observer pointer to an entity by ID.
     * @return T* pointer, or nullptr if no entity with the specified ID exists.
     */
    T* getById(int id) noexcept {
        auto it = items.find(id);
        return (it != items.end()) ? it->second.get() : nullptr;
    }

    /**
     * @brief Const overload of getById().
     * @return const T* pointer, or nullptr if not found.
     */
    const T* getById(int id) const noexcept {
        auto it = items.find(id);
        return (it != items.end()) ? it->second.get() : nullptr;
    }

    /**
     * @brief Retrieves non-owning observer pointers to all stored entities.
     * @note The order of elements is non-deterministic due to hash map bucketing.
     */
    std::vector<T*> getAll() {
        std::vector<T*> result;
        result.reserve(items.size());
        for (auto& [id, ptr] : items) {
            result.push_back(ptr.get());
        }
        return result;
    }

    /**
     * @brief Const overload of getAll().
     */
    std::vector<const T*> getAll() const {
        std::vector<const T*> result;
        result.reserve(items.size());
        for (const auto& [id, ptr] : items) {
            result.push_back(ptr.get());
        }
        return result;
    }

    /**
     * @brief Removes an entity by ID, deallocating the managed unique_ptr<T>.
     * @return true if an entity was removed, false if the ID was not found.
     */
    bool remove(int id) noexcept {
        return items.erase(id) > 0;
    }

    /**
     * @brief Checks if an entity exists in the repository.
     */
    bool exists(int id) const noexcept {
        return items.find(id) != items.end();
    }

    /**
     * @brief Returns the number of entities currently stored.
     */
    std::size_t size() const noexcept {
        return items.size();
    }

    /**
     * @brief Checks if the repository is empty.
     */
    bool empty() const noexcept {
        return items.empty();
    }

    /**
     * @brief Clears all entities from the repository, deallocating all owned objects.
     */
    void clear() noexcept {
        items.clear();
    }
};

} // namespace fleetflow::storage

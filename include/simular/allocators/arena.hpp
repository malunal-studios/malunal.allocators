/// @file   arena.hpp
/// @brief  Provides the arena memory resource declaration.
/// @author John Christman sorakatadzuma@gmail.com
/// @copyright 2024 Simular Technologies, LLC.
#pragma once


#if SIMULAR_ALLOCATORS_PLATFORM_POSIX
#include <sys/mman.h>
#elif SIMULAR_ALLOCATORS_PLATFORM_WIN32
#define WIN32_LEAN_AND_MEAN 1
#define NOMINMAX 1
#include <windows.h>
#endif /* Platform specific headers */


// Set maximum allocation size if not yet set.
#ifndef SIMULAR_ALLOCATORS_REGION_MAXIMUM_ALLOCATION
/// @def     SIMULAR_ALLOCATORS_REGION_MAXIMUM_ALLOCATION
/// @brief   The maximum allowed allocation size for the arena memory resource.
/// @details This is modifiable by you the developer. It configures how much the
///          arena memory resource can allocate in one allocation call. The value
///          must be between 1 and the 64-bit max.
#define SIMULAR_ALLOCATORS_REGION_MAXIMUM_ALLOCATION 0x003F'FFF8
#elif SIMULAR_ALLOCATORS_REGION_MAXIMUM_ALLOCATION < 0x1000 || \
      SIMULAR_ALLOCATORS_REGION_MAXIMUM_ALLOCATION > INT64_MAX
#  error Maximum allocation size must be > 0 and < INT64_MAX
#endif /* SIMULAR_ALLOCATORS_REGION_MAXIMUM_ALLOCATION */

// Set default arena capacity if not yet set.
#ifndef SIMULAR_ALLOCATORS_ARENA_DEFAULT_CAPACITY
/// @def     SIMULAR_ALLOCATORS_ARENA_DEFAULT_CAPACITY
/// @brief   A default capacity for virtual memory allocated by this library.
/// @details This is modifiable by you the developer. It configures how much
///          memory the arena memory resource can allocate per region it stores.
///          There is no upper bounds to how much memory can be allocated for
///          a region, but there is a lower bounds of 1.
/// @remarks This is in mebibytes.
#define SIMULAR_ALLOCATORS_ARENA_DEFAULT_CAPACITY 4
#elif SIMULAR_ALLOCATORS_ARENA_DEFAULT_CAPACITY < 1
#  error Region default capacity must be greater than 1
#endif /* SIMULAR_ALLOCATORS_ARENA_DEFAULT_CAPACITY */

// Set free list size if not yet set.
#ifndef SIMULAR_ALLOCATORS_ARENA_FREE_LIST_SIZE
/// @def     SIMULAR_ALLOCATORS_ARENA_FREE_LIST_SIZE
/// @brief   The allowed size for the arena memory resource free list.
/// @details This is modifiable by you the developer. It configures how many
///          free list nodes can be stored within the memory of the arena's
///          initial region. The lower bounds is 8 and the upper bounds is 256.
///          The upper bounds is very generous. You may never need that many if
///          the arena does it's job properly and you use the memory properly.
///          This value is a way to preallocate the arena to store the number
///          you need. Otherwise, the list will reallocate everytime it needs
///          more space and that will cause it to wander around the memory of
///          the allocator which may be undesirable.
/// @note    The free list has no ability to corrupt the arena memory! The
///          cause for it to reallocate and wander is a consequence of using
///          the standard library `vector` as it will allocate a new block
///          before deallocating the previous.
#define SIMULAR_ALLOCATORS_ARENA_FREE_LIST_SIZE 32
#elif SIMULAR_ALLOCATORS_ARENA_FREE_LIST_SIZE < 8 || \
      SIMULAR_ALLOCATORS_ARENA_FREE_LIST_SIZE > 256
#  error Default free count must be > 8 and < 256
#endif


namespace simular::allocators {

/// @brief   The maximum size of an allocation into any given region.
/// @details Regions of the vmem will be the size of this since one whole
///          allocation can fit into it.
inline static constexpr size_t
k_max_alloc_size = SIMULAR_ALLOCATORS_REGION_MAXIMUM_ALLOCATION;

/// @brief   The default capacity of the arena memory resource.
/// @details This is controlled by a macro which you, the developer, can
///          adjust to your needs.
inline static constexpr size_t
k_default_capacity = SIMULAR_ALLOCATORS_ARENA_DEFAULT_CAPACITY;

/// @brief   The default free list size for the arena memory resource.
/// @details This is controlled by a macro which you, the developer, can
///          adjust to your needs.
inline static constexpr size_t
k_free_list_size = SIMULAR_ALLOCATORS_ARENA_FREE_LIST_SIZE;


/// @brief   A memory resource for managing regions of virtual memory acquired
///          from the operating system.
/// @details Manages the acquisition, initialization, and release of virtual
///          memory regions from the operating system, as well as (de)allocation
///          of data in those regions.
struct arena_memory_resource : std::pmr::memory_resource {
    using super = std::pmr::memory_resource;
    using self  = arena_memory_resource;

    /// @brief   Initializes the arena memory resource by acquiring a linked
    ///          list of unallocated regions.
    /// @details Each unallocated region will have a small header at the start
    ///          which conveys how much of that region is used, how much is
    ///          committed to the operating system (only applicable to windows),
    ///          and a pointer to the next region.
    /// @param   capacity The initial capacity of the arena memory resource
    ///          measured in MiB (mebibytes).
    explicit
    arena_memory_resource(size_t capacity = k_default_capacity)
        : linbufres_()
        , free_list_(&linbufres_)
    {
        constexpr size_t mebibytes = 1048576;
        capacity *= mebibytes;
        vmem_acquire(capacity);
    }

    /// @brief   Releases the arena memory resource by releasing all of the
    ///          regions in the linked list of the memory managed by this
    ///          resource.
    /// @details No matter what was allocated into the regions, the arena
    ///          memory resource will completely free those regions without
    ///          calling destructors. The calling of destructors for objects
    ///          contained in the regions managed by this resource should be
    ///          handled by an extended implementation.
    virtual
    ~arena_memory_resource() noexcept {
        if (first_ != nullptr)
            vmem_release(&first_);
    }

    /// @brief   Provides the amount of memory that this resource has used.
    /// @details The arena memory resource tracks the usage of regions, both
    ///          per region and in total across all acquired regions. It does
    ///          this for diagnostic purposes.
    /// @returns The total amount of used memory from the regions acquired by
    ///          this resource.
    size_t
    total_used() const noexcept {
        return total_used_;
    }

    /// @brief   Provides the amount of memory that this resource has acquired.
    /// @details The arena memory resource tracks its total size, the size of
    ///          all its regions combined, primarily for diagnostic purposes.
    /// @returns The total amount of memory that this resource has acquired.
    size_t
    total_size() const noexcept {
        return total_size_;
    }

    /// @brief   Provides the number of regions acquired by this arena.
    /// @details The arena memory resource tracks the number of regions acquired
    ///          from the operating system as a means of diagnostics.
    /// @returns The total number of regions acquired by this resource.
    size_t
    total_regions() const noexcept {
        return total_regions_;
    }

    /// @brief   Provides the number of allocations that have taken place on
    ///          this arena memory resource.
    /// @details The arena memory resource tracks the number of allocations made
    ///          into the regions of this allocator for diagnostic purposes.
    /// @returns The number of allocations made to this resource.
    /// @remarks The number of allocations is always accurate because not only
    ///          allocations tracked but so are deallocations. So, when something
    ///          becomes deallocated the allocation count will be decreased.
    size_t
    allocations() const noexcept {
        return allocations_;
    }

protected:
    /// @brief   Defines a region of virual memory that has been acquired from
    ///          the operating system.
    /// @details This structure is really simple, it is a linked list to each of
    ///          the regions that were acquired from the operating system. The
    ///          region at the end of the list is guaranteed to be `nullptr`.
    struct region final {
        /// @brief   The next region in the list.
        /// @details This points to the next acquired region in the chain. It is
        ///          guaranteed to be `nullptr` if there is no next region.
        region* next;
    };

    /// @brief   Defines a freed block of a region.
    /// @details After an object has been allocated, it obviously can be
    ///          deallocated. When the object is deallocated, the memory is
    ///          reset and this structure will be injected into its place for
    ///          the arena to track.
    struct freed final {
        /// @brief   The size of this freed block.
        /// @details This will often be the size of the object that was
        ///          originally stored in the location this now occupies.
        ///          However, in some cases, the size will be the size of the
        ///          region or a subdivision of the region.
        size_t size{0};

        /// @brief   The next freed block in the list.
        /// @details This points to the next freed block in the chain. It is
        ///          guaranteed to be `nullptr` if there is no next block.
        uintptr_t addr;
    };

    /// @brief Functor object used to compare the sizes of freed blocks within the
    ///        arena memory resource when sorting the arena's freed list.
    struct freed_size_comparator final {
        using host = arena_memory_resource;

        /// @brief  Functor call method that performs the size comparison.
        /// @param  lhs The left hand freed block to compare.
        /// @param  rhs The right hand freed block to compare.
        /// @retval True if the left hand free block size is smaller.
        /// @retval False if the right hand free block size is smaller. 
        bool
        operator()(
            const host::freed& lhs,
            const host::freed& rhs
        ) const noexcept {
            return lhs.size < rhs.size;
        }
    };

    /// @brief Functor object used to compare the addresses of freed blocks within
    ///        the arena memory resource when sorting the arena's freed list.
    struct freed_addr_comparator final {
        using host = arena_memory_resource;

        /// @brief  Functor call method that performs the address comparison.
        /// @param  lhs The left hand freed block to comapre.
        /// @param  rhs The right hand freed block to compare.
        /// @retval True if the left hand free block address is smaller.
        /// @retval False if the right hand free block address is smaller.
        bool
        operator()(
            const host::freed& lhs,
            const host::freed& rhs
        ) const noexcept {
            return lhs.addr < rhs.addr;
        }
    };

    /// @brief   Gets the pointer to the starting region.
    /// @details You can use this for validation purposes. There is no `end()`
    ///          equivalent because this is a linked list and you can easily
    ///          traverse it yourself to find the end.
    /// @returns A pointer to the starting region.
    const region*
    first_region() const noexcept {
        return first_;
    }

    /// @brief   Gets the pointer to the start of the freed blocks.
    /// @details You can use this for validation purposes. There is no `end()`
    ///          equivalent because this is a linked list and you can easily
    ///          traverse it yourself to find the end.
    /// @returns A pointer to the start of the freed blocks.
    const std::pmr::vector<freed>&
    free_list() const noexcept {
        return free_list_;
    }

    /// @brief   Finds a free block to allocate the number of bytes specified.
    /// @details This will attempt to find a free block to allocate into. It
    ///          prefers smaller blocks over larger blocks, as to find the best
    ///          fit whilst saving larger blocks for larger allocations.
    /// @param   bytes The number of bytes that need to be allocated.
    /// @param   alignment The alignment of the object to be allocated.
    /// @returns A pointer to the memory which the object can be placed into.
    void*
    do_allocate(size_t bytes, size_t alignment) override {
        return vmem_allocate_region(bytes, alignment);
    }

    /// @brief   Creates a free block from the pointer that was allocated.
    /// @details This will set the underlying memory of the pointer to `nullptr`
    ///          before replacing it with a free block that the arena needs to
    ///          track.
    /// @param   ptr The pointer to the object to deallocate.
    /// @param   bytes The size of the object to deallocate.
    /// @param   alignment The alignment of the object to deallocate.
    void
    do_deallocate(void* ptr, size_t bytes, size_t alignment) override {
        vmem_deallocate_region(ptr, bytes, alignment);
    }

    /// @brief   Checks if the memory resource provided is an arena memory
    ///          resource itself, and if the initial regions are the same.
    /// @details Two arena memory resources cannot in any way, have the same
    ///          initial region if they were created separately. If one arena
    ///          memory resource was copied, then those two may share the same
    ///          initial region, but they may not reflect the same current
    ///          region or the same totals. For this reason, we only compare the
    ///          initial region.
    /// @param   other The other memory resource to compare to.
    /// @returns True if the other memory resource is a arena memory resource
    ///          and the initial regions are the same; otherwise false.
    bool
    do_is_equal(const memory_resource& other) const noexcept override {
        auto casted = dynamic_cast<const self*>(&other);
        return casted != nullptr && first_ == casted->first_;
    }

private:
    linear_buffer_resource  linbufres_;
    std::pmr::vector<freed> free_list_;

    region* first_{nullptr};
    size_t  total_used_{0};
    size_t  total_size_{0};
    size_t  total_regions_{0};
    size_t  allocations_{0};


    void
    vmem_init_free_blocks() {
        // Reconstruct the linear buffer resource using move semantics.
        const auto [buffer, length] = std::invoke([this] {
            const auto begin  = reinterpret_cast<uintptr_t>(first_) + sizeof(region);
            const auto length = k_free_list_size * sizeof(freed);
            return std::make_pair(reinterpret_cast<void*>(begin), length);
        });
        linbufres_   = linear_buffer_resource(buffer, length);
        total_used_ += length;

        // Reserve the entirety of the linear buffer resource through the free
        // list vector, and push the first free node into the list.
        free_list_.reserve(k_free_list_size);
        free_list_.push_back(freed {
            .size = k_max_alloc_size - length,
            .addr = reinterpret_cast<uintptr_t>(buffer) + length
        });
        allocations_++;

        // Create a free list node for each of the regions acquired by this
        // arena memory resource.
        auto temp = &first_->next;
        while (*temp != nullptr) {
            // Inject free block
            free_list_.push_back(freed {
                .size = k_max_alloc_size,
                .addr = reinterpret_cast<uintptr_t>(*temp) + sizeof(region)
            });

            temp = &(*temp)->next;
        }
    }

    void
    vmem_acquire(size_t capacity) {
        constexpr size_t k_sizeof  = sizeof(region);
        constexpr size_t k_regsize = k_max_alloc_size + k_sizeof;

        // Determine number of regions to acquire.
        const auto remain = capacity % k_regsize;
        const auto blocks = std::invoke([&remain, &capacity] {
            const auto res = capacity / k_regsize;
            return remain != 0 ? res + 1 : res;
        });

        auto temp = &first_;
        for (auto index = 0u; index < blocks; index++) {
            vmem_acquire(temp, k_regsize);
            temp = &(*temp)->next;
        }

        vmem_init_free_blocks();
        total_size_ = blocks * k_regsize;
    }

    void
    vmem_release(region** pp_region) {
        assert(*pp_region != nullptr);
        if ((*pp_region)->next != nullptr)
            vmem_release(&(*pp_region)->next);

    #if SIMULAR_ALLOCATORS_PLATFORM_WIN32
        ::VirtualFree(*pp_region, 0, MEM_RELEASE);
    #elif SIMULAR_ALLOCATORS_PLATFORM_POSIX
        ::munmap(*pp_region, k_max_alloc_size);
    #endif /* Platform specific code */

        total_used_ = 0;
        total_size_ = 0;
        *pp_region  = nullptr;
    }

    void
    vmem_acquire(region** pp_region, size_t capacity) {
    #if SIMULAR_ALLOCATORS_PLATFORM_WIN32
        constexpr int32_t k_memops = MEM_COMMIT | MEM_RESERVE;
        constexpr int32_t k_pageops = PAGE_READWRITE;
        auto ptr = ::VirtualAlloc(0, capacity, k_memops, k_pageops);
        *pp_region = reinterpret_cast<region*>(ptr);
        if (*pp_region == nullptr)
            std::bad_alloc();
    #elif SIMULAR_ALLOCATORS_PLATFORM_POSIX
        constexpr int32_t k_memops = PROT_READ | PROT_WRITE;
        constexpr int32_t k_memprms = MAP_PRIVATE | MAP_ANONYMOUS;
        const auto min = std::invoke([&capacity] {
            auto res = capacity / 16;
            return res < 1 ? capacity : res;
        });

        void* ptr = nullptr;
        while (capacity >= min) {
            ptr = ::mmap(0, capacity, k_memops, k_memprms, -1, 0);
            if (ptr != MAP_FAILED && ptr != nullptr)
                break;
            
            // Unrecoverable.
            if (errno != ENOMEM    &&
                errno != EOVERFLOW &&
                errno != EAGAIN)
                    break;
            
            // Try smaller size.
            capacity /= 2;
        }

        // Did we fail?
        if (ptr == MAP_FAILED || ptr == nullptr)
            throw std::bad_alloc();
        *pp_region = reinterpret_cast<region*>(ptr);
    #else /* Unsupported platform */
        throw std::bad_alloc();
    #endif /* Platform specific code */

        constexpr std::size_t k_regsize = sizeof(region);
        total_used_ += k_regsize;
        total_regions_++;
        (*pp_region)->next = nullptr;
    }

    void*
    vmem_find_free_block(size_t bytes, size_t alignment) {
        size_t to_allocate{bytes};
        size_t adjustment{0};

        auto itr = free_list_.begin();
        while (itr != free_list_.end()) {
            const auto adjustment = detail::calc_fwd_adjust(itr->addr, alignment);
            to_allocate += adjustment;
            if (itr->size == to_allocate)
                break;

            const auto next = itr + 1;
            const auto fits_in_current = bytes < itr->size;
            const auto next_is_the_end = next == free_list_.end();
            const auto next_is_better  = !next_is_the_end && itr->size < next->size;
            if (fits_in_current && next_is_better == false)
                break;

            itr++;
        }

        // Failed to find a free block to allocate into.
        if (itr == free_list_.end())
            throw std::bad_alloc();

        // Shrink this freed block by the number of bytes
        // to allocate.
        auto result = itr->addr;
        if (itr->size > to_allocate) {
            itr->size -= to_allocate;
            itr->addr  = itr->addr + to_allocate;
            std::sort(
                free_list_.begin(),
                free_list_.end(),
                freed_size_comparator()
            );
        } else free_list_.erase(itr);

        total_used_ += to_allocate;
        ++allocations_;

        return reinterpret_cast<void*>(result + adjustment);
    }

    void*
    vmem_allocate_region(size_t bytes, size_t alignment) {
        // Try to find free block first.
        auto res = vmem_find_free_block(bytes, alignment);
        if (res != nullptr)
            return res;

        // The region allocation size.
        const auto size = k_max_alloc_size + sizeof(region);
        
        // Allocate new region.
        auto last = &first_;
        while (*last != nullptr)
            last = &(*last)->next;
        vmem_acquire(last, size);
        total_size_ += size;

        // Add new block to free list.
        const auto addr = reinterpret_cast<uintptr_t>(*last);
        free_list_.push_back(freed {
            .size = k_max_alloc_size,
            .addr = addr + sizeof(region)
        });

        return vmem_find_free_block(bytes, alignment); 
    }

    void
    vmem_deallocate_region(void* ptr, size_t bytes, size_t alignment) {
        const auto pointer    = reinterpret_cast<uintptr_t>(ptr);
        const auto adjustment = detail::calc_fwd_adjust(pointer, alignment);

        bytes += adjustment;
        const auto block_start = pointer - adjustment;
        const auto block_end   = block_start + bytes;

        // Iterate through each node and merge possibles.
        auto index = 0u;
        while (index != free_list_.size()) {
            const auto curr = free_list_[index];
            if (block_end <= curr.addr)
                break;
            index++;
        }

        // There were no free blocks that start after the one we're freeing.
        // Add it to the beginning of the list.
        if (index == 0) {
            free_list_.insert(free_list_.begin(), freed {
                .size = bytes,
                .addr = block_start
            });
        } else {
            // The block that starts after the one we're freeing has a block before
            // it on the free list, and it ends right on the boundary starting our
            // block. These blocks can be merged.
            auto prev = free_list_.begin() + (index - 1);
            if (prev->addr + prev->size == block_start) {
                prev->size += bytes;
                goto MERGE_BLOCKS;
            }

            // Insert block between previous and current.
            auto curr = free_list_.begin() + index;
            free_list_.insert(curr, freed {
                .size = bytes,
                .addr = block_start
            });
        }

    MERGE_BLOCKS:
        if (1 < free_list_.size()) {
            auto curr = free_list_.begin() + index;
            auto next = free_list_.begin() + index + 1;
            if (curr->addr + curr->size == next->addr) {
                curr->size += next->size;
                free_list_.erase(next);
            }

            // Sort by size again.
            std::sort(
                free_list_.begin(),
                free_list_.end(),
                freed_size_comparator()
            );
        }

        --allocations_;
        total_used_ -= bytes;
    }
};


/// @brief   Provides a default arena memory resource.
/// @details This is provided as a convenience function so you do not have to
///          set up your own. It's also required for some of the utility
///          function provided by this library.
/// @returns A pointer to the default arena memory resource.
inline arena_memory_resource*
arena_allocator_instance() {
    static arena_memory_resource
    k_arena_memory_resource;
    return &k_arena_memory_resource;
}

} // namespace simular::allocators

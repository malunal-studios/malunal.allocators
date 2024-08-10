/// @file   virtual.hpp
/// @brief  Provides the virtual allocator implementation.
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
#ifndef SIMULAR_ALLOCATORS_VMEM_MAXIMUM_ALLOCATION
/// @def     SIMULAR_ALLOCATORS_VMEM_MAXIMUM_ALLOCATION
/// @brief   The maximum allowed allocation size for the virtual allocator.
/// @details This is modifiable by you the developer. It configures how much the
///          virtual allocator can allocate in one allocation call. The value
///          must be between 1 and the 64-bit max.
#define SIMULAR_ALLOCATORS_VMEM_MAXIMUM_ALLOCATION 0x003F'FFF8
#elif SIMULAR_ALLOCATORS_VMEM_MAXIMUM_ALLOCATION < 0x1000 || \
      SIMULAR_ALLOCATORS_VMEM_MAXIMUM_ALLOCATION > INT64_MAX
#  error Maximum allocation size must be > 0 and < INT64_MAX
#endif /* SIMULAR_ALLOCATORS_VMEM_MAXIMUM_ALLOCATION */

// Set default region capacity if not yet set.
#ifndef SIMULAR_ALLOCATORS_VMEM_DEFAULT_CAPACITY
/// @def     SIMULAR_ALLOCATORS_VMEM_DEFAULT_CAPACITY
/// @brief   A default capacity for virtual memory allocated by this library.
/// @details This is modifiable by you the developer. It configures how much
///          memory the virtual allocator can allocate per region it stores.
///          There is no upper bounds to how much memory can be allocated for
///          a region, but there is a lower bounds of 1.
/// @remarks This is in mebibytes.
#define SIMULAR_ALLOCATORS_VMEM_DEFAULT_CAPACITY 4
#elif SIMULAR_ALLOCATORS_VMEM_DEFAULT_CAPACITY < 1
#  error Region default capacity must be greater than 1
#endif /* SIMULAR_ALLOCATORS_VMEM_DEFAULT_CAPACITY */


namespace simular::allocators {

/// @brief   A memory resource for managing regions of virtual memory acquired
///          from the operating system.
/// @details Manages the acquisition, initialization, and release of virtual
///          memory regions from the operating system, as well as (de)allocation
///          of data in those regions.
struct arena_memory_resource : std::pmr::memory_resource {
    /// @brief   The allowed page commit size, mostly used for windows related
    ///          memory allocations since we have to tell windows what was
    ///          committed of what it gave us.
    inline static constexpr std::size_t
    k_commit_page_size = 0x10000;

    /// @brief   The maximum size of an allocation into any given region.
    /// @details Regions of the vmem will be the size of this since one whole
    ///          allocation can fit into it.
    inline static constexpr std::size_t
    k_max_alloc_size = SIMULAR_ALLOCATORS_VMEM_MAXIMUM_ALLOCATION;

    /// @brief   The default capacity of the arena memory resource.
    /// @details This is controlled by a macro which you, the developer, can
    ///          adjust to your needs.
    inline static constexpr std::size_t
    k_default_capacity = SIMULAR_ALLOCATORS_VMEM_DEFAULT_CAPACITY;


    /// @brief   Initializes the arena memory resource by acquiring a linked
    ///          list of unallocated regions.
    /// @details Each unallocated region will have a small header at the start
    ///          which conveys how much of that region is used, how much is
    ///          committed to the operating system (only applicable to windows),
    ///          and a pointer to the next region.
    /// @param   capacity The initial capacity of the arena memory resource
    ///          measured in MiB (mebibytes).
    explicit
    arena_memory_resource(std::size_t capacity = k_default_capacity) {
        constexpr std::size_t mebibytes = 1048576;
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
    std::size_t
    total_used() const noexcept {
        return total_used_;
    }

    /// @brief   Provides the amount of memory that this resource has acquired.
    /// @details The arena memory resource tracks its total size, the size of
    ///          all its regions combined, primarily for diagnostic purposes.
    /// @returns The total amount of memory that this resource has acquired.
    std::size_t
    total_size() const noexcept {
        return total_size_;
    }

    /// @brief   Provides the number of regions acquired by this arena.
    /// @details The arena memory resource tracks the number of regions acquired
    ///          from the operating system as a means of diagnostics.
    /// @returns The total number of regions acquired by this resource.
    std::size_t
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
    std::size_t
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
        freed* next;
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
    const freed*
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
    do_allocate(
        std::size_t bytes,
        std::size_t alignment
    ) override {
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
    do_deallocate(
        void*       ptr,
        std::size_t bytes,
        std::size_t alignment
    ) override {
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
    do_is_equal(
        const memory_resource& other
    ) const noexcept override {
        using self = arena_memory_resource;
        auto casted = dynamic_cast<const self*>(&other);
        return casted != nullptr && first_ == casted->first_;
    }

private:
    region* first_;
    freed*  free_list_;
    std::size_t total_used_{0};
    std::size_t total_size_{0};
    std::size_t total_regions_{0};
    std::size_t allocations_{0};

    void
    vmem_acquire(std::size_t capacity) {
        constexpr std::size_t k_sizeof  = sizeof(region);
        constexpr std::size_t k_regsize = k_max_alloc_size + k_sizeof;

        // Determine number of regions to acquire.
        const auto remain = capacity % k_regsize;
        const auto blocks = std::invoke([&remain, &capacity] {
            auto res = capacity / k_regsize;
            return remain != 0 ? res + 1 : res;
        });

        auto temp = &first_;
        auto free = &free_list_;
        for (auto index = 0u; index < blocks; index++) {
            vmem_acquire(temp, k_regsize);
            auto addr = reinterpret_cast<std::uintptr_t>(*temp);
            *free = reinterpret_cast<freed*>(addr + k_sizeof);
            (*free)->size = k_max_alloc_size;
            (*free)->next = nullptr;

            // Move forward one.
            temp = &(*temp)->next;
            free = &(*free)->next;
        }

        total_size_ = blocks * k_regsize;
    }

    void
    vmem_acquire(
        region**    pp_region,
        std::size_t capacity
    ) {
    #if SIMULAR_ALLOCATORS_PLATFORM_WIN32
        constexpr std::int32_t k_memops = MEM_COMMIT | MEM_RESERVE;
        constexpr std::int32_t k_pageops = PAGE_NOACCESS;
        auto ptr = ::VirtualAlloc(0, capacity, k_memops, k_pageops);
        *pp_region = reinterpret_cast<region*>(ptr);
        if (*pp_region == nullptr)
            std::bad_alloc();
    #elif SIMULAR_ALLOCATORS_PLATFORM_POSIX
        constexpr std::int32_t k_memops = PROT_READ | PROT_WRITE;
        constexpr std::int32_t k_memprms = MAP_PRIVATE | MAP_ANONYMOUS;
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

    void
    vmem_release(region** pp_region) noexcept {
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

    void*
    vmem_find_free_block(
        freed*      start,
        std::size_t bytes,
        std::size_t alignment
    ) {
        assert(start != nullptr);
        freed* prev_free{nullptr};
        freed* curr_free{start};
        freed* best_prev{nullptr};
        freed* best_curr{nullptr};

        std::size_t adjustment{0};
        while (curr_free != nullptr) {
            const auto pointer = reinterpret_cast<std::uintptr_t>(curr_free);
            const auto aligned = (pointer + alignment - 1u) & ~(alignment - 1u);
            
            // Favor splitting smaller blocks than big blocks.
            adjustment = aligned - pointer;
            bytes     += adjustment;
            const auto fits_in_current = bytes < curr_free->size;
            const auto best_is_better  = best_curr == nullptr || best_curr->size < curr_free->size;
            if (fits_in_current == false || best_is_better == false)
                goto TRY_NEXT_FREE_BLOCK;

            best_prev = prev_free;
            best_curr = curr_free;
            if (curr_free->size == bytes)
                break;

        TRY_NEXT_FREE_BLOCK:
            prev_free = curr_free;
            curr_free = curr_free->next;
        }

        // Failed to find an free block to allocate into.
        if (best_curr == nullptr)
            return best_curr;

        // Calculate new address of the free block
        auto new_block = std::invoke([&best_curr, &bytes] {
            auto casted = reinterpret_cast<std::uintptr_t>(best_curr);
            return reinterpret_cast<freed*>(casted + bytes);
        });

        new_block->size = best_curr->size - bytes;
        new_block->next = best_curr->next;
        if (best_prev != nullptr)
            best_prev->next = new_block;
        else free_list_ = new_block;

        total_used_ += bytes;
        ++allocations_;
        auto aligned = reinterpret_cast<std::uintptr_t>(best_curr + adjustment);
        return reinterpret_cast<void*>(aligned);
    }

    void*
    vmem_allocate_region(
        std::size_t bytes,
        std::size_t alignment
    ) {
        // Try to find free block first.
        auto res = vmem_find_free_block(
            free_list_, bytes, alignment);
        if (res != nullptr)
            return res;

        // The region allocation size.
        const auto size = k_max_alloc_size + sizeof(region);
        
        // Otherwise we need to allocate new region.
        auto last = &first_;
        while (*last != nullptr)
            last = &(*last)->next;
        vmem_acquire(last, size);
        total_size_ += size;

        // Find end of free list.
        auto free = &free_list_;
        while (*free != nullptr)
            free = &(*free)->next;
        
        // Update free list.
        auto addr = reinterpret_cast<std::uintptr_t>(*last);
        *free = reinterpret_cast<freed*>(addr + sizeof(region));
        return vmem_find_free_block(*free, bytes, alignment);
    }

    void
    vmem_deallocate_region(
        void*       ptr,
        std::size_t bytes,
        std::size_t alignment
    ) {
        const auto pointer    = reinterpret_cast<std::uintptr_t>(ptr);
        const auto aligned    = (pointer + alignment - 1u) & ~(alignment - 1u);
        const auto adjustment = aligned - pointer;
        
        bytes += adjustment;
        auto block_start = pointer - adjustment;
        auto block_end   = block_start + bytes;

        // Find first block that starts after, or is at the boundary of this
        // block.
        freed* prev_free = nullptr;
        freed* curr_free = free_list_;
        while (curr_free != nullptr) {
            const auto casted = reinterpret_cast<std::uintptr_t>(curr_free);
            if (block_end <= casted)
                break;
            prev_free = curr_free;
            curr_free = curr_free->next;
        }

        // Before we insert the freed block header, perform memset to make it
        // invalid in case the user decide they want to use it again.
        ::memset(reinterpret_cast<void*>(block_start), 0, bytes);

        // There was no free block that starts after the one we're freeing.
        // Add it to the start of the free list.
        if (prev_free == nullptr) {
            prev_free = reinterpret_cast<freed*>(block_start);
            prev_free->size = bytes;
            prev_free->next = free_list_;
            free_list_ = prev_free;
            goto MERGE_BLOCKS;
        } else {
            // The block that starts after the one we're freeing has a block before
            // it on the free list, and it ends right on the boundary starting our
            // block. These blocks can be merged.
            const auto casted = reinterpret_cast<std::uintptr_t>(prev_free);
            if (casted + prev_free->size == block_start) {
                prev_free->size += bytes;
                goto MERGE_BLOCKS;
            }

            // Inject block between two others.
            freed* temp = reinterpret_cast<freed*>(block_start);
            temp->size = bytes;
            temp->next = prev_free->next;

            prev_free->next = temp;
            prev_free       = temp;
        } 
        

    MERGE_BLOCKS:
        auto casted_prev = reinterpret_cast<std::uintptr_t>(prev_free);
        auto casted_next = reinterpret_cast<std::uintptr_t>(prev_free->next);
        if (casted_prev + prev_free->size == casted_next) {
            // Merge these.
            prev_free->size += prev_free->next->size;
            prev_free->next  = prev_free->next->next;
        }

        --allocations_;
        total_used_ -= bytes;
    }
};

} // namespace simular::allocators

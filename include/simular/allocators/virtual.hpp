/// @file   virtual.hpp
/// @brief  Provides the virtual allocator implementation.
/// @author John Christman sorakatadzuma@gmail.com
/// @copyright 2024 Simular Technologies, LLC.
#pragma once
#include <cassert>
#include <memory>
#include <new>

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
#define SIMULAR_ALLOCATORS_VMEM_MAXIMUM_ALLOCATION 0x0040'0000
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

/// @brief   Controls the virtual memory allocations for all allocators.
/// @details Manages the initialization and allocation of data into a block of
///          virtual memory obtained from the operating system.
/// @remarks Based on the implementation found in the `C3` compiler.
struct vmem final {
    /// @brief   The allowed page commit size, mostly used for windows related
    ///          memory allocations since we have to tell windows what was
    ///          committed of what it gave us.
    inline static constexpr std::size_t
    k_commit_page_size = 0x10000;

    /// @brief   The maximum size of an allocation into any given region.
    /// @details Regions of the vmem will be the size of this since one whole
    ///          allocation can fit into it.
    inline static constexpr std::size_t
    k_max_allocation_size = SIMULAR_ALLOCATORS_VMEM_MAXIMUM_ALLOCATION;

    /// @brief   The default capacity of the virtual memory manager.
    /// @details This is controlled by a macro which you, the developer, can
    ///          adjust to your needs.
    inline static constexpr std::size_t
    k_default_capacity = SIMULAR_ALLOCATORS_VMEM_DEFAULT_CAPACITY;


    /// @brief Initializes the vmem with the size in mebibytes of memory.
    /// @param size_mib The number of mebibytes to initialize with. Has a
    ///        default capacity equal to `k_default_capacity`.
    explicit
    vmem(std::size_t size_mib = k_default_capacity) {
        constexpr std::size_t mebibytes = 1048576;
        if (size_mib > k_max_allocation_size)
            size_mib = k_max_allocation_size;
        vmem_acquire(mebibytes * size_mib);
    }

    /// @brief   Releases the memory managed by this object.
    /// @details More specifically, memory obtained from the operating system
    ///          will be released back to the operating system.
    virtual
    ~vmem() noexcept {
        // If we couldn't allocate to begin with
        // there will be nothing to release.
        if (first_ != nullptr)
            vmem_release(first_);
    }

    /// @brief   Allocates a piece of the virtual memory block.
    /// @param   size The size of the data to allocate.
    /// @param   alignment The alignment of that data within the memory block.
    /// @returns A pointer to the allocated memory.
    void*
    allocate(std::size_t size, std::size_t alignment) {
        return vmem_allocate_region(&curr_, size, alignment);
    }

protected:
    // So let me explain something, this structure will actually be much larger
    // in memory than it is here. We allocate enough virtual memory to meet the
    // maximum allocation size, and we represent that as the block. So the data
    // of the block will be injected into the beginning of the memory space that
    // the data will occupy. The `data` member will point to its actual location
    // in memory because that would, ironically, be the start of the data of the
    // block. The `used` member is used to find the next available location in
    // the block to allocate into.
    struct region final {
        std::size_t used{0};
    #if SIMULAR_ALLOCATORS_PLATFORM_WIN32
        std::size_t committed{0};
    #endif /* SIMULAR_ALLOCATORS_PLATFORM_WIN32 */
        region* next{nullptr};
        void* data{nullptr};
    };

private:
    void
    vmem_acquire(std::size_t size) {
        auto count     = size / k_max_allocation_size;
        auto remainder = size % k_max_allocation_size;
        if (remainder != 0)
            count++;

        // Create first block and then create chain.
        // Okay lemme explain, the pointer to pointer allows us to work with an
        // arbitrary block*. We iterate over the number of blocks that need to
        // be created, taking the address of the `next` pointer and creating
        // that block until all blocks are created.
        region** temp = &first_;
        vmem_acquire_region(temp, k_max_allocation_size);
        for (auto index = 0; index < count - 1; index++) {
            temp = &((*temp)->next); // Get pointer to next.
            vmem_acquire_region(temp, k_max_allocation_size);
        }

        curr_ = first_;
        total_size_ = size;
        total_used_ = 0;
    }

    void
    vmem_release(region* pregion) {
        if (pregion->next != nullptr)
            vmem_release(pregion->next);

        total_used_ -= pregion->used;
        total_size_ -= k_max_allocation_size;

    #if SIMULAR_ALLOCATORS_PLATFORM_WIN32
        total_commit_ -= pregion->committed;
        ::VirtualFree(pregion, 0, MEM_RELEASE);
    #elif SIMULAR_ALLOCATORS_PLATFORM_POSIX
        ::munmap(pregion, k_max_allocation_size);
    #endif /* Platform specific code */
    }

    void
    vmem_acquire_region(region** ppregion, std::size_t size) {
    #if SIMULAR_ALLOCATORS_PLATFORM_WIN32
        auto ptr = ::VirtualAlloc(0, size, MEM_RESERVE, PAGE_NOACCESS);
        *ppregion = reinterpret_cast<region*>(ptr);
        if (*ppregion == nullptr)
            std::bad_alloc();
    #elif SIMULAR_ALLOCATORS_PLATFORM_POSIX
        void* ptr = nullptr;
        auto min_size = size / 16;
        if (min_size < 1)
            min_size = size;
        
        constexpr std::int32_t k_rwperms  = PROT_READ | PROT_WRITE;
        constexpr std::int32_t k_memperms = MAP_PRIVATE | MAP_ANON;
        while (size >= min_size) {
            // Try to map some memory.
            ptr = ::mmap(0, size, k_rwperms, k_memperms, -1, 0);
            if (ptr != MAP_FAILED && ptr != nullptr)
                break;
            
            // Non-retriable error.
            if (errno != ENOMEM    &&
                errno != EOVERFLOW &&
                errno != EAGAIN)
                    break;
            
            // Try smaller size.
            size /= 2;
        }

        // Did we end on a failure?
        if (ptr == MAP_FAILED || ptr == nullptr)
            throw std::bad_alloc();
        *ppregion = reinterpret_cast<region*>(ptr);
    #else /* Unsupported platform */
        throw std::bad_alloc();
    #endif /* Platform specific code */

        // Must commit block header.
        vmem_init_region_header(ppregion);
    }

    void
    vmem_init_region_header(region** ppregion) {
        // Should commit first block of memory.
        vmem_commit_region_block(ppregion, sizeof(region));

        // The amount of the block used should be the size of the region header.
        // The data should point to the start of the region, because the next
        // available location will be the amount of the region used plus the
        // base address of the region.
        (*ppregion)->used = sizeof(region);
        (*ppregion)->next = nullptr;
        (*ppregion)->data = reinterpret_cast<void*>(*ppregion);
    }

    void*
    vmem_allocate_region(
        region**    ppregion,
        std::size_t size,
        std::size_t alignment
    ) {
        assert(*ppregion != nullptr);
        auto allocated = size + (*ppregion)->used;
        vmem_commit_region_block(ppregion, allocated);

        // Calculate the aligned address.
        auto region_data = reinterpret_cast<std::uintptr_t>((*ppregion)->data);
        auto current_addr = region_data + (*ppregion)->used;
        auto aligned_addr = (current_addr + alignment - 1) & ~(alignment - 1);

        // Calculate the padding needed to align the address.
        allocated += aligned_addr - current_addr;
        if (k_max_allocation_size < allocated &&
            total_used_ + allocated < total_size_) {
            // Didn't have enough space in this region, we should allocate a
            // new region and return the result of allocating in that region.
            region** result = &((*ppregion)->next);
            vmem_acquire_region(result, k_max_allocation_size);
            return vmem_allocate_region(result, size, alignment);
        }

        // We had enough room to allocate the result.
        (*ppregion)->used = allocated;
        total_used_ += allocated;
        return reinterpret_cast<void*>(aligned_addr);
    }

    void
    vmem_commit_region_block(
        region**    ppregion,
        std::size_t allocated
    ) {
    #if SIMULAR_ALLOCATORS_PLATFORM_WIN32
        auto blocks_committed = (*ppregion)->committed / k_commit_page_size;
        auto end_of_block = (allocated + k_commit_page_size - 1) / k_commit_page_size;
        auto blocks_to_allocate = end_of_block - blocks_committed;
        if (blocks_to_alllocate > 0) {
            auto to_commit = blocks_to_allocate * k_commit_page_size;
            auto region_mem = reinterpret_cast<std::uintptr_t>(*ppregion);
            auto committed_mem = region_mem + (*ppregion)->committed;
            auto result = ::VirtualAlloc(committed_mem, to_commit, MEM_COMMIT, PAGE_READWRITE);
            if (result == nullptr)
                throw std::bad_alloc();
            (*ppregion)->committed += to_commit;
            total_commit_ += to_commit;
        }
    #endif /* SIMULAR_ALLOCATORS_PLATFORM_WIN32 */
    }

protected:
    region* first_{nullptr};
    region* curr_{nullptr};
    std::size_t total_used_{0};
    std::size_t total_size_{0};
#if SIMULAR_ALLOCATORS_PLATFORM_WIN32
    std::size_t total_commit_{0};
#endif /* SIMULAR_ALLOCATORS_PLATFORM_WIN32 */
};

} // namespace simular::allocators

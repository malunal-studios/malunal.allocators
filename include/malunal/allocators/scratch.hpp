/// @file   scratch.hpp
/// @brief  Provides the scratch buffer resource implementation.
/// @author John Christman sorakatadzuma@gmail.com
/// @copyright 2024 Malunal Studios, LLC.
#pragma once


namespace malunal::allocators {

/// @brief   A scratch buffer resource is a memory resource which is created
///          from an existing piece of memory, either stack or heap, that can
///          be used like a scratch pad or note taking area.
/// @details For all intents and purposes, this memory resource is simply a push
///          pointer allocator. Once it runs out of the memory in the buffer
///          that it was constructed from, it will try and acquire more from an
///          upstream
struct scratch_buffer_resource : linear_buffer_resource {
    using upstream = std::pmr::memory_resource;
    using super    = linear_buffer_resource;
    using self     = scratch_buffer_resource;


    /// @brief   Constructs the scratch buffer resource from the given buffer,
    ///          and buffer length.
    /// @details The scratch buffer requires that the buffer it is given is not
    ///          `nullptr` and that the length of that buffer is not 0. The
    ///          scratch buffer resource is considered ill-formed if these pre-
    ///          conditions are not met.
    /// @param   buffer A pointer to the buffer that the scratch buffer resource
    ///          utilizes to allocate into.
    /// @param   length The length of the buffer the scratch buffer resource
    ///          will be allocating into.
    scratch_buffer_resource(
        void*  buffer,
        size_t length
    ) noexcept : super(buffer, length) {
    }

    /// @brief   Constructs the scratch buffer resource from the given buffer,
    ///          buffer length, and the upstream resource.
    /// @details This constructor is the same as `self(void*,size_t)` but with
    ///          the additional requirement that the provided `upstream` memory
    ///          resource is also not nullptr. If you didn't need an upstream
    ///          resource, use the other constructor.
    /// @param buffer 
    /// @param length 
    /// @param upstream 
    scratch_buffer_resource(
        void*     buffer,
        size_t    length,
        upstream* upstream
    ) noexcept
        : super(buffer, length)
        , upstream_{ upstream }
    {
        assert(upstream != nullptr);
    }


    /// @brief Provided for overriding classes to properly destruct themselves.
    virtual
    ~scratch_buffer_resource() noexcept = default;


protected:
    /// @brief   Allocates a piece of the buffer by the given size in bytes and
    ///          aligned to the provided alignment.
    /// @details This implementation will try to allocate using the linear
    ///          buffer resource first (it's parent), and if that fails, it will
    ///          try the upstream memory resource. In the event that the
    ///          upstream is able to provide data, it will update the super's
    ///          tracked buffer to the new one. In the event that both memory
    ///          resources are unable to provide a memory address, this will
    ///          throw `std::bad_alloc`.
    /// @param   bytes The amount of bytes to allocate.
    /// @param   alignment The alignment for determining the byte boundary where
    ///          the resulting pointer should start.
    /// @returns A pointer to where the data can be stored.
    /// @throws  std::bad_alloc If no memory address could be obtained.
    void*
    do_allocate(size_t bytes, size_t alignment) override {
        void* result;
        
        result = try_super_allocate(bytes, alignment);
        if (result != nullptr)
            return result;
        
        result = try_upstream_allocate(bytes, alignment);
        if (result != nullptr) {
            change_buffer(result, bytes);
            return result;
        }
        
        // Could not allocate at all.
        throw std::bad_alloc();
    }

    /// @brief   Deallcoates the given pointer from the scratch buffer resource.
    /// @details This will call the super method only, and the super method will
    ///          not deallocate anything because it's simply a push pointer
    ///          resource.
    /// @param   ptr The pointer to deallocate.
    /// @param   bytes The number of bytes of the object being deallocated.
    /// @param   alignment The alignment of the object being deallocated.
    void
    do_deallocate(void* ptr, size_t bytes, size_t alignment) {
        super::do_deallocate(ptr, bytes, alignment);
    }

    /// @brief   Checks if the memory resource provided is a scratch buffer
    ///          resource itself, and if it has the same upstream, buffer,
    ///          length, and count as this one.
    /// @details This will first check the provided other memory resource's type
    ///          and then check if it has an upstream that matches this one's.
    ///          From there it delegates the buffer, length, and count comparison
    ///          to the super class.
    /// @param   other The other memory resource to compare to.
    /// @returns True if the other memroy resource is a scratch buffer resource
    ///          and it shares the same upstream, buffer, length, and count with
    ///          this one; false otherwise.
    bool
    do_is_equal(const memory_resource& other) const noexcept override {
        auto casted = dynamic_cast<const self*>(&other);
        return casted    != nullptr           &&
               upstream_ == casted->upstream_ &&
               super::do_is_equal(*casted);
    }

private:
    upstream* upstream_{nullptr};

    void*
    try_super_allocate(size_t bytes, size_t alignment) {
        void* result{nullptr};
        try {
            result = super::do_allocate(bytes, alignment);
        } catch (const std::bad_alloc&) {
        }

        return result;
    }

    void*
    try_upstream_allocate(size_t bytes, size_t alignment) {
        void* result{nullptr};
        try {
            if (upstream_ != nullptr)
                upstream_->deallocate(result, bytes, alignment);
        } catch (const std::bad_alloc&) {
        }

        return result;
    }
};


} // namespace malunal::allocators

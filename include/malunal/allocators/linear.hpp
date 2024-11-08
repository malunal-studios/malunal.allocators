/// @file   linear.hpp
/// @brief  Provides the linear buffer resource implementation.
/// @author John Christman sorakatadzuma@gmail.com
/// @copyright 2024 Malunal Studios, LLC.
#pragma once


namespace malunal::allocators {

/// @brief   A linear buffer resource is a memory resource that linearly
///          allocates a provided buffer, pushing a pointer forward everytime
///          an allocation is performed.
/// @details This is one of the simplest types of memory resources provided by
///          this library. It's very useful for when you just need to allocate
///          and not deallocate. Deallocation of this buffer resource is done
///          simply by calling the `reset()` or `clear()` methods.
struct linear_buffer_resource : std::pmr::memory_resource {
    using super = std::pmr::memory_resource;
    using self  = linear_buffer_resource;

    /// @brief Default constructor provided so the linear buffer resource can be
    ///        move asigned and move constructed later.
    explicit
    linear_buffer_resource() noexcept = default;

    /// @brief Provided for overriding classes to properly destruct themselves.
    virtual
    ~linear_buffer_resource() noexcept = default;

    /// @brief   Defaulted copy constructor.
    /// @details Linear buffer resource is a trivial type, so we can default the
    ///          copy constructor and let the compiler generate the code for us.
    /// @param   other The linear buffer resource that we're copying from.
    linear_buffer_resource(const linear_buffer_resource& other) noexcept = default;

    /// @brief   Defaulted move constructor.
    /// @details Linear buffer resource is a trivial type, so we can default the
    ///          move constructor and let the compiler generate the code for us.
    /// @param   other The linear buffer resource that we're moving from.
    linear_buffer_resource(linear_buffer_resource&& other) noexcept = default;

    /// @brief   Defaulted copy assignment operator.
    /// @details Linear buffer resource is a trivial type, so we can default the
    ///          move constructor and let the compiler generate the code for us.
    /// @param   other The linear buffer resource that we're copying from.
    /// @returns This linear buffer resource with the copied members of the
    ///          other linear buffer resource.
    linear_buffer_resource&
    operator=(const linear_buffer_resource& other) noexcept = default;

    /// @brief   Defaulted move assignment operator.
    /// @details Linear buffer resource is a trivial type, so we can default the
    ///          move constructor and let the compiler generate the code for us.
    /// @param   other The linear buffer resource that we're moving from.
    /// @returns This linear buffer resource with the moved members of the other
    ///          linear buffer resource.
    linear_buffer_resource&
    operator=(linear_buffer_resource&& other) noexcept = default;

    /// @brief   Constructs a linear buffer resource from the given pre-acquired
    ///          buffer, and buffer length.
    /// @remarks Will assert that the buffer provided is not nullptr, and that
    ///          the length of that buffer is not zero upon calling this
    ///          constructor.
    /// @param   buffer The buffer by which this linear buffer resource can
    ///          allocate data into.
    /// @param   length The length of the buffer provided to this linear buffer
    ///          resource.
    linear_buffer_resource(
        void*  buffer,
        size_t length
    ) noexcept
        : buffer_{ buffer }
        , length_{ length }
    {
        assert(buffer != nullptr);
        assert(length != 0);
    }

    /// @brief   Resets the linear buffer resource used count to 0.
    /// @details This allows the linear buffer resource to be used again. It
    ///          will not clear the contents of the linear buffer so that it
    ///          retains performance, but if you need the buffer cleared, then
    ///          you should call the `clear()` method.
    void
    reset() noexcept {
        count_ = 0;
    }

    /// @brief   Clears the linear buffer resource underlying buffer and resets
    ///          the buffer used count.
    /// @details This method should beused over the `reset()` method, whenever
    ///          the data should be wiped from the buffer. Otherwise, it's
    ///          recommended that you use the `reset()` method instead.
    void
    clear() noexcept {
        std::memset(buffer_, 0, length_);
        reset();
    }

protected:
    /// @brief   Allocates a piece of the buffer by the given size in bytes and
    ///          aligned to the provided alignment.
    /// @param   bytes The amount of bytes to allocate.
    /// @param   alignment The alignment for determining the byte boundary of
    ///          where the resulting pointer should start.
    /// @returns A pointer to where the data can be stored.
    /// @throws  std::bad_alloc If no memory address could be obtained.
    void*
    do_allocate(size_t bytes, size_t alignment) override {
        assert(buffer_ != nullptr);
        assert(length_ != 0);
        if (bytes == 0 || alignment == 0)
            throw std::bad_alloc();

        const auto address    = reinterpret_cast<uintptr_t>(buffer_);
        const auto adjustment = detail::calc_fwd_adjust(address + count_, alignment);
        const auto old_count  = count_ + adjustment;
        const auto new_count  = count_ + bytes;
        if (new_count > length_)
            throw std::bad_alloc();
        
        count_ = new_count;
        return reinterpret_cast<void*>(address + old_count);
    }

    /// @brief   Deallocates the given pointer from the linear buffer resource.
    /// @details This method actually won't do anything because linear buffer
    ///          resources do not actually deallocate anything unless requested
    ///          to reset or clear.
    /// @param   ptr The pointer to the object to deallocate.
    /// @param   bytes The number of bytes of the object being deallocated.
    /// @param   alignment The alignment of the object being deallocated.
    void
    do_deallocate(void* ptr, size_t bytes, size_t alignment) override {
        (void)ptr;
        (void)bytes;
        (void)alignment;
    }

    /// @brief   Checks if the memory resource provided is a linear buffer
    ///          resource itself, and if it has the same buffer, length, and
    ///          count as this one.
    /// @details When checking linear buffer resources, we must be careful to
    ///          check all member of the resources as linear buffer resources
    ///          can be created independently, and may be given the same buffer,
    ///          but have different lengths and counts.
    /// @param   other The other memory resource to compare to.
    /// @returns True if the other memory resource is a linear buffer resource
    ///          and it shares the same buffer, length, and count with this one;
    ///          false otherwise.
    bool
    do_is_equal(const memory_resource& other) const noexcept override {
        auto casted = dynamic_cast<const self*>(&other);
        return casted  != nullptr         &&
               buffer_ == casted->buffer_ &&
               length_ == casted->length_ &&
               count_  == casted->count_;
    }

    /// @brief   An protected function for a child implementation to change the
    ///          buffer of this object.
    /// @details Imagine for a moment, a situation where the child implementation
    ///          of this object uses some kind of upstream memory resource where
    ///          it obtains new memory from when this object's initial buffer is
    ///          full. This function allows the child to now track that new
    ///          buffer using this implementation instead of having to rely on
    ///          the upstream the entire time.
    /// @param   buffer The new buffer that the linear buffer resource can
    ///          allocate to.
    /// @param   length The length of the new buffer, must be larger or equal to
    ///          the number of bytes that were previously allocated.
    void
    change_buffer(void* buffer, size_t length) noexcept {
        assert(buffer != nullptr);
        assert(length != 0);
        assert(count_ <= length);

        buffer_ = buffer;
        length_ = length;
    }

private:
    void*  buffer_{nullptr};
    size_t length_{0};
    size_t count_{0};
};

} // namespace malunal::allocators

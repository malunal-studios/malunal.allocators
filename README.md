# Simular.Allocators

A header-only collection of allocators which can be used to serve whatever allocation strategies you may need. For our own purposes, it serves as a manner to facilitate the large swaths of allocations and memory managment that we need in our `simular.cherry` project.

### Requirements

As with most of our libraries, you'll need a compiler that supports C++20 or higher. You'll also need CMake `>=3.16`, because we intend to support precompiled headers and that is the first version of CMake that supports it.

# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.1.0] - 2024-11-08

### Changed

- Company rebranding, changed any instance of the terms Simular to Malunal and Simular Technologies, LLC to Malunal Studios, LLC

## [1.0.0] - 2024-08-11

### Added

- [Linear Buffer Resource](./include/malunal/allocators/linear.hpp) a push pointer allocator which the arena uses internally for the free list
- [Scratch Buffer Resource](./include/malunal/allocators/scratch.hpp) an extension of the linear buffer resource which can handle allocating from an upstream memory resource when it's full
- [Arena Memory Resource](./include/malunal/allocators/arena.hpp) which handles arena allocations, and tracks free blocks within the arena
- [Arena Memory Resource Tests](./tests/arena.cpp) which validate the arena capabilities
- [Arena Memory Resource Benchmarks](./benchmarks/arena.cpp) which validates arena performance over standard memory resource

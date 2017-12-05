# OMR Compiler Memory Manager

The OMR Compiler doesn't necessarily operate in a vacuum, but rather 
as one component of a Managed runtime. Additionally,  communications 
between components could be using C linkage. Therefore, one can't simply 
use something like `::operator new` since it can throw. Additionally,
any generic common allocator would have poor interaction between 
components, with little to no insight into usage.

Using an allocator specific to a Managed Runtime, such as 
`omrmem_allocate_memory` in the Port Library, is expensive 
(both in terms of memory footprint and execution time) 
for many of the types of allocations performed 
by the Compiler; there are too many participants for informal trust, 
book-keeping is expensive, and optimizing for divergent component 
use-cases is difficult to impossible.

Therefore, the compiler manages memory its own memory.

## Compiler Memory Manager Hierarchy

### Low Level Allocators
Low Level Allocators cannot be used by STL containers. They are 
generally used by High Level Allocators as the underlying 
provider of memory.

```
                          +---------------------+
                          |                     |
                          | TR::SegmentProvider |
                          |                     |
                          +-----+-----------+---+
                                ^           ^
                                |           |
                                |           |
                                |           |
          +---------------------+--+     +--+--------------+
          |                        |     |                 |
          |  TR::SegmentAllocator  |     | TR::SegmentPool |
          |                        |     |                 |
          +--------+------------+--+     +-----------------+
                   ^            ^
                   |            |
                   |            |
+------------------+---------+  |
|                            |  |
| OMR::SystemSegmentProvider |  |
|                            |  |
+----------------------------+  |
                                |
                      +---------+-----------------+
                      |                           |
                      | OMR::DebugSegmentProvider |
                      |                           |
                      +---------------------------+
```

#### TR::SegmentProvider
`TR::SegmentProvider` is the basest class in the hierarchy. It is a 
pure virtual class. It provides APIs to 
* Request Memory
* Release Memory
* Determine the amount of memory allocated

#### TR::SegmentPool
`TR::SegmentPool` implements `TR::SegmentProvider`. It maintains a pool 
of memory segments. It currently does not have any users.

#### TR::SegmentAllocator
`TR::SegmentAllocator` is a pure virtual class. It extends 
`TR::SegmentProvider`. It providers further APIs to
* Determine system bytes allocated
* Determine region bytes allocated
* Set an allocation limit

Generally, implementors of this class allocate big segments of memory 
and subdivide those segments into smaller chunks of memory. Therefore,
_system bytes_ refers to the total amount of big segments of memory 
allocated, and _region bytes_ refers to the amount of memory carved 
into smaller chunks. This process of allocating big segments of memory 
in order to subdivide into smaller chunks is done to minimize the 
number of times the allocator needs to request memory from *__its__* 
backing provider (whether that be the Port Libary, or the OS, or 
something else).

#### OMR::SystemSegmentProvider
`OMR::SystemSegmentProvider` implements `TR::SegmentAllocator`. 
It uses `TR::RawAllocator` (see below) in order to allocate both 
`TR::MemorySegment`s (see below) as well as memory for all its 
internal memory management requirements. This is the default Low 
Level Allocator used by the Compiler.

#### OMR::DebugSegmentProvider
`OMR::DebugSegmentProvider` implements `TR::SegmentAllocator`. 
It uses `mmap` (or equivalent) to allocate `TR::MemorySegment`s. 
It does not free memory when requested; instead it changes the 
permission on the freed memory. The purpose of this is to help 
debug memory corruption issues. It uses `TR::RawAllocator` for 
all its internal memory mangement requirements. The compiler 
will use this Low Level Allocator when the 
`TR_EnableScratchMemoryDebugging` option is enabled.

### High level Allocators
High Level Allocators can be used by STL containers by wrapping them
in a `TR::typed_allocator` (see below). They generally use a Low 
Level Allocator as their underlying provider of memory.

#### TR::Region
`TR::Region` facilitates Region Semantics. It is used to allocate 
memory that exists for its lifetime. It frees all of its memory when 
it is destroyed. It uses `TR::SegmentProvider` as its backing provider 
of memory. It also contains an automatic conversion (which wraps it 
in a `TR::typed_allocator`) for ease of use with STL containers.

#### TR::StackMemoryRegion
`TR::StackMemoryRegion` extends `TR::Region`. It is used to facilitate 
Stack Mark / Stack Release semantics. A Stack Mark is implemented by 
constructing the object, at which point it registers itself with the 
`TR_Memory` object (see below), and saves the previous 
`TR::StackMemoryRegion` object. A Stack Release is implemented by 
destroying the object, at which point it unregisters itself with the 
`TR_Memory` object, restoring the previous `TR::StackMemoryRegion` 
object. `TR_Memory` is then used to allocate memory from the region.
The use of this object **must** be consistent; it must either 
be used for Stack Mark / Stack Release via `TR_Memory`, OR, it must 
be used as a Region (though at that point, `TR::Region` should be used). 
Interleaving Stack Mark / Stack Release and Region semantics **will** 
result in invalid data.

#### TR::PersistentAllocator
`TR::PersistentAllocator` is used to allocate memory that persists 
for the lifetime of the Managed Runtime. It uses `TR::RawAllocator` 
to allocate memory. It receives this allocator via the 
`TR::PersistentAllocatorKit` (see below). It also contains 
an automatic conversion (which wraps it in a `TR::typed_allocator`) 
for ease of use with STL containers.

#### TR::RawAllocator
`TR::RawAllocator` uses `malloc` (or equivalent) to allocate memory. 
It contains an automatic conversion (which wraps it in a 
`TR::typed_allocator`) for ease of use with STL containers.

#### TR::Allocator
`TR::Allocator` is an allocator provided by the CS2 library. 
Use of `TR:Allocator` is strongly discouraged; it only exists for 
legacy purposes. It will quite likley be removed in the 
future. It contains an automatic conversion (which wraps it in a 
`TR::typed_allocator`) for ease of use with STL containers.

## Other Compiler Memory Manager Related Concepts

#### TR::typed_allocator
`TR::typed_allocator` is used to wrap High Level 
Allocators such as `TR::Region` in order to allow them to 
be used by STL containers.

#### TR::RegionProfiler
`TR::RegionProfiler` is used to profile a `TR::Region`. 
It does so via Debug Counters.

#### TR::PersistentAllocatorKit
`TR::PersistentAllocatorKit` contains data that is used 
by the `TR::PersistentAllocator`.

#### TR::MemorySegment
`TR::MemorySegment` is used to describe a chunk, or segment, 
of memory. It is also used to carve out that segment into smaller 
pieces of memory. Both Low and High Level Allocators above describe and 
subdivide the memory they manage using `TR::MemorySegment`s.

#### TR_PersistentMemory
`TR_PersistentMemory` is an object that is used to allocate memory 
that persists throughout the lifetime of the runtime. It uses 
`TR::PersistentAllocator` to allocate memory. For the most part, 
it is used in conjunction with `TR_ALLOC` (and related macros 
described below). It is also used as a substitute for `malloc` in 
legacy code where memory is still allocated and freed explicitly. 
Generally, newer code should favour using `TR::PersistentAllocator` 
instead of `jitPersistentAlloc`/`jitPersistentFree`.

#### TR_Memory
`TR_Memory` is a legacy object that was used as the interface by 
which memory could be allocated. Each `TR::Compilation` contains 
a `TR_Memory` object. `TR_Memory` is similar to `TR_PersistentMemory` 
in that it is used in conjunction with `TR_ALLOC` (and related 
macros described below), as well as is used as the means to allocate 
and free memory explicity. Generally, newer code should favour 
using `TR::Region`.

#### TR_ALLOC, TR_PERSISTENT_ALLOC, TR_PERSISTENT_ALLOC_THROW, TR_ALLOC_WITHOUT_NEW
These are macros that are placed inside a class' definition. 
They are used to define a set of `new`/`delete` operators, as 
well as to add the `jitPersistentAlloc`/`jitPersistentFree` methods 
into the class. They exist to help keep track of the memory 
allocated by various objects, as well as to ensure that the 
`new`/`delete` operators are used in a manner that is consistent 
with the Compiler memory management.

## How the Compiler Allocates Memory

The Compiler deals with two categories of allocations:
1. Allocations that are only useful during a compilation
2. Allocations that need to persist throughout the lifetime of the Managed Runtime

### Allocations only useful during a compilation
The Compiler initializes a `OMR::SystemSegmentProvider` 
(or `OMR::DebugSegmentProvider`) local object at the start of a 
compilation. It then initializes a local `TR::Region` object, and a 
local `TR_Memory` object which uses the `TR::Region` for general 
allocations (and sub Regions), as well as for the first 
`TR::StackMemoryRegion`. At the end of the compilation, the 
`TR::Region` and `OMR::SystemtSegmentProvider` 
(or `OMR::DebugSegmentProvider`) go out of scope, invoking their 
destructors and freeing all the memory.

There are a lot of places (thanks to `TR_ALLOC` and related macros) 
where memory is explicity allocated. However, `TR::Region` 
should be the allocator used for all new code as much as possible.

### Allocations that persist for the lifetime of the Managed Runtime
The Compiler initializes a `TR::PeristentAllocator` object when 
it is first initialized (close to bootstrap time). For the most 
part it allocates persistent memory either directly using the global 
`jitPersistentAlloc`/`jitPersistentFree` or via the object methods 
added through `TR_ALLOC` (and related macros). Again, 
`TR::PersistentAllocator` should be the allocator used for all new 
code as much as possible.


## Subtleties and Miscellaneous Information

:rotating_light:`TR::Region` allocations are untyped raw memory. In order to have a region
destroy an object allocated within it, the object would need to be created
using `TR::Region::create`. This requires passing in a reference to an
existing object to copy, which requires that the object be
copy-constructable. The objects die in LIFO order when the Region is destroyed.:rotating_light:

`TR::PersistentAllocator` is not a `TR::Region`. That said, if one wanted
to create a `TR::Region` that used `TR::PersistentAllocator` as its backing
provider, they would simply need to extend `TR::SegmentProvider`, perhaps
calling it `TR::PersistentSegmentProvider`, and have it use
`TR::PersistentAllocator`. The `TR::Region` would then be provided this 
`TR::PersistentSegmentProvider` object.



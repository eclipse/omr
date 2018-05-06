# Om: Meditations on Objects

A dynamic object model for Eclipse OMR.

## Super Quick Start

### Building Om

```sh
git clone https://github.com/b9org/om
cd om
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug -GNinja
ninja
ninja test
```

### Basic API usage

```c++
using Om = OMR::Om;

int main(int argc, char** argv) {
	// The runtime handles process-wide initalization.
	Om::ProcessRuntime runtime;

	// The manager has it's own isolated heap.
	Om::MemoryManager manager(runtime);

	// The Context is per-thread, and provides shared heap access.
	Om::RunContext cx(manager);

	// Allocate an object and root it.
	Om::RootRef<Object> root(cx, Om::Object::allocate(cx));
}
```

## The Object Model

Objects are untyped, and grow slots dynamically during execution. Om supports
both untyped and typed slots, of variable widths.

Om takes a hybrid approach to object classes, where objects can freely
transition between layouts, but Maps (a kind of class) are able to describe a
range of slots in the objects. At each layout transition, the object takes on
a new map. Maps are created and derived on demand.

## How is this different from the OMR Garbage Collector?

The OMR GC is written to be application agnostic, it does not understand the
shape of objects. Instead, the GC gives the user a set of "glue" APIs, which
the application must implement. These APIs might tell the GC how to walk
objects, or how to calculate an object's size. These APIs are compiled
directly into the OMR GC library. Om implements that glue, and gives
applications a high level API for working with objects.

## Configuring the build

Om uses cmake as it's build tool.

### Building Om for Debug

Common CMake options:
* `CMAKE_BUILD_TYPE=Debug`

Om options:
* `OM_DEBUG`
* `OM_DEBUG_SLOW`: Turn on some very slow integrity checks
/// Debug: Poison reclaimed memory.
* `OMR_OM_POISON_RECLAIMED_MEMORY`: Set to On, and Om will poison reclaimed
  memory at the end of the GC. This is a helpful, and relatively fast debugging
  tool.
* `OMR_OM_COLLECT_ON_ALLOCATE`: Run a full system GC at every allocation. Very
  slow, but useful for debugging roots.
* `OMR_OM_TRACE`: Verbose tracing prints to stderr

Complete example (copy-paste-able)

```sh
# in the build directory:
cmake -DCMAKE_BUILD_TYPE=Debug -DOM_DEBUG=Yes ..
```

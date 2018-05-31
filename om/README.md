# Om: Meditations on Objects

A distribution of the Eclipse OMR GC providing a dynamic object model.

## Super Quick Start

### Building Om

```sh
git clone https://github.com/eclipse/omr
cd omr
mkdir build
cd build
cmake ..   -GNinja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DOMR_OM=ON \
  -DOMR_EXAMPLE=OFF \
  -DOMR_GC=ON \
  -DOMR_JITBUILDER=ON
ninja
ninja test
```

### Basic API usage

```c++

#include <OMR/Om/ObjectOperations.hpp>
#include <OMR/Om/MemorySystem.hpp>
#include <OMR/Om/ProcessRuntime.hpp>
#include <OMR/Om/Context.hpp>
#include <OMR/Om/collect.hpp>

using Om = OMR::Om;

int main(int argc, char** argv) {
	// The runtime handles process-wide initalization.
	OMR::Om::ProcessRuntime runtime;
	runtime.initialize();

	// The system has it's own isolated heap.
	OMR::Om::MemorySystem system(runtime);
	system.initialize();

	// The Context is per-thread, and provides shared heap access.
	OMR::Om::RunContext cx(system);
	cx.initialize();

	// Allocate an object
	OMR::Om::Object* object = OMR::Om::allocateObject(cx);

	// Root the object
	OMR::Om::RootRef<OMR::Om::Object> root(cx, object);

	// Run a complete system collection
	OMR::Om::systemCollect(cx);

	return 0;
}
```

and link with:

```sh
gcc ./om-example.cpp -o om-example -lomrgc
```

## The Object Model

Objects provide a mix of strongly typed and untyped slots. Object layouts can
be modified during execution.

Objects are untyped, and grow slots dynamically during execution. Om supports
both untyped and typed slots, of variable widths.

Om takes a hybrid approach to object classes, where objects can freely
transition between layouts, but Maps (a kind of class) are able to describe a
range of slots in the objects. At each layout transition, the object takes on
a new shape. Maps are created and derived on demand.

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

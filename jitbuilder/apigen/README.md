# JitBuilder API generator

The core principle driving the design of the binding generator is to have a tool
that consumes a machine readable description of the JitBuilder API and produces
appropriate bindings for a given language.

## High-level Design

The basic principle followed is to have implemented functionality on one side
and the language-specific interface or *client API* on the other side.

On the implementation side, an object contains all the functionality. On the
client API side, a one-to-one corresponding object forwards API calls to the
implementation class.

A C client API is used as base for bindings to other languages. Since the JitBuilder
implementation relies heavily on virtual functions and overriding, a special
mechanism is needed to allow overriding to be done in C. Because a user can 
override the implementation of certain JitBuilder services (e.g. `IlBuilder::buildIL()`
and `VirtualMachineState::Commit()`), the implementation side cannot rely on calling
those services on its side to get the correct behaviour. Instead it must call
the service on the *client side* and ensure user overrides are invoked if present.

The C overriding mechanism makes use of function pointers to set callbacks. When
a client-side object is constructor or initialized, it sets a callback for each
virtual function on the implementation side that can be overriden by a user.

For the mechanism to work, every function that can have a user overridden must be
paired with a second, non-virtual function that handles dispatch. While the virtual
function just contains the function's implementation, the non-virtual *dispatch function*
invokes the callback corresponding to the function that is set by the client-side
object. By convention, the name of the virtual function is capitalized
(e.g. `virtual Foo()`) since it provides the functionality of the client API.
The name of the dispatch function is not capitalized (e.g. `foo()`) as it's only
intended for internal use in the implementation.

On the implementation side, instead of calling a virtual function `Foo()`, the
corresponding dispatch function `foo()` is called to ensure that, if set, the
client-side override of `Foo()` is executed. As a result, the callback should,
by default, just invoke the default implementation provided on the implementation
side, assuming one exists.

```
     Client API       :       Implementation
                      :
                      :     +---------------+
                      :     | Base          |
  +---------------+   :     +---------------+
  + FooCallback() <---------+ foo()         |
  +-----------+---+   :     |               |
              `-------------> virtual Foo() |
                      :     +---------------+
                      :
```

To override the default implementation, a different callback with the custom
implementation should be set.

In client APIs built on top of the C API, the callback may instead forward its
invocation to a function (or method) implemented in the target language, forwarding
the responsibility of calling the default implementation. For example, in the C++
client API, a call to the callback would invoke a client-side virtual function
`Foo()`  that would take care calling back into the implementation side when no
user overrides are exist.

```
                     Client API         :       Implementation
                                        :
+---------------+                       :       +---------------+
| Base          |                       :       | Base          |
+---------------+   +---------------+   :       +---------------+
| virtual Foo() <---+ FooCallback() <-----------+ foo()         |
+------------+--+   +---------------+   :       |               |
             |                          :  ,----> virtual Foo() |
             `-----------------------------'    +---------------+
                                        :

```

If an implementation-side derived class overrides `Foo()`, then a matching
client-side derived class also defines an override that will just invoke
the implementation-side override. The client-side class also sets a different
callback that will invoke the a client-side override of `Foo()`.

```
                     Client API         :       Implementation
                                        :
+---------------+                       :       +---------------+
| Base          |                       :       | Base          |
+---------------+   +---------------+   :       +---------------+
| virtual Foo() |   | +---------------+ :  ,----+ foo()         |
+---.-----------+   +-| FooCallback() <----'    |               |
   /_\                +--+------------+ :       | virtual Foo() |
    |                    |              :       +--------.------+
    |                    |              :               /_\
    |                    |              :                |
+---+-----------+        |              :       +--------+------+
| Derived       |        |              :       | Derived       |
+---------------+        |              :       +---------------+
| virtual Foo() <--------'              :  ,----> virtual Foo() |
+------------+--+                       :  |    +---------------+
             `-----------------------------'
                                        :
```

If a user overrides `Foo()`, then virtual dispatch will ensure that the
user's override is invoked when `foo()` is called on the implementation side.

```
                     Client API         :       Implementation
                                        :
+---------------+                       :       +---------------+
| Base          |                       :       | Base          |
+---------------+   +---------------+   :       +---------------+
| virtual Foo() |   | +---------------+ :  ,----+ foo()         |
+---.-----------+   +-| FooCallback() <----'    |               |
   /_\                +--+------------+ :       | virtual Foo() |
    |                    |              :       +--------.------+
    |                    |              :               /_\
    |                    |              :                |
+---+-----------+        |              :       +--------+------+
| Derived       |        |              :       | Derived       |
+---------------+        |              :       +---------------+
| virtual Foo() |<-------'              :       | virtual Foo() |
+---.-----------+ \                     :       +---------------+
   /_\             |                    :
    |              | [virtual dispatch] :
+---+-----------+  |                    :
| UserDerived   |  |                    :
+---------------+  |                    :
| virtual Foo() <--'                    :
+---------------+                       :
                                        :
```


## Implementation

### The basics

A minimal implementation (base-)class declaration will look like:

```c++
namespace TR {

extern "C" {
    typedef void * (*ImplGetter)(void *client);
    typedef void * (*ClientAllocator)(void *implObject);
}

class Base {
    public:

    vitual void * client();

    void setClient(void * c);
    static void setGetImpl(ImplGetter getter);
    static void setClientAllocator(ClientAllocator allocator);

    protected:
    void * _client;
    static ImplGetter * _getImpl;
    
    private:
    static ClientAllocator * _clientAllocator;
};

}
```

Let's break it down.

The `_client` field is an opaque pointer to the client-side object corresponding
to the current object. It is the only means by which an implementation can
communicated with a client object. The member function `client()` is a (special)
getter for the `_client`. It *must* be virtual to allow subclasses to override it,
as will be shown later. As it's name implies `setClient()` is just an old
fashion setter for the field.

The static field `_getImpl` is a pointer to a function that, when called with an
instance of a client object, will return the corresponding implementation
object. `ImplGetter` is a `typedef` for the corresponding function pointer type.
It is the responsibility of client APIs to set this field by a call to the
static member function `setGetImpl()`.

The static field `_clientAllocator` is a pointer to a function that returns a
pointer to a newly allocated client object. When called, the corresponding
implementation object is passed as argument. It's type, `ClientAllocator` is
just a `typedef` for the function pointer type. Once again, it is the client
API's responsibility to set this field with a call the static member function
`setClientAllocator()`.

These fields and member functions are the bare minimum all implementation-side
classes must have in order to fully support code produced by the binding
generator.

### Regular functions

Regular, non-virtual functions are implemented as normal member functions.
When a call is made on a client-side object, it will simply be forwarded to the
corresponding implementation-side function (more on this later).

```c++
// declaration
namespace TR {
class Base {
    public:
    // ...
    void Foo();
    // ...
};
}

// implementation
void TR::Base::Foo() {
    // implementation
}
```

### Class fields

Like regular class functions, class fields are also straight forward. The only
requirement is that fields must be public in order to be visible to the client.

However, fields present a risk because they must never be modified by either
the client API or a consuming project. Using private fields with public getters
and setters should be preferred.

### Virtual functions and callbacks

> This section needs work. There are clearly some weaknesses in the design that
will have to be resolved. Hopefully, these will be mostly safe changes. There
should still be a description of the short-commings and ways to improve.

Virtual functions, normally used to implement callbacks, require a bit more
consideration. First, the function implementation itself looks the same as
normal virtual functions:

```c++
// declaration
namespace TR {
class Base {
    public:
    // ...
    virtual void Bar(); // contains implementation
    // ...
};
}

// implementation
void TR::Base::Bar() {
    // implementation
}
```

Next, there needs to be a way of allow both classes on the client-side and the
implementation-side to override the function. Let's consider the first scenario.
In C, a virtual function can be simply thought of as a function pointer. When
the function is "overriden", the function pointer is simply assigned to a new
function. So, we can treat a client-side override as assigning some function
pointer that we keep internally. A setter function is also required to allow
the clients to set these overrides.

```c++
// declaration

extern "C" {
// ...
typedef void * (*BarCallback)();
// ...
}

namespace TR {
class Base {
    public:
    // ...
    virtual void Bar(); // contains implementation

    void setClientCallback_Bar(void * callback) {
        _clientCallbackBar = (BarCallback *) callback;
    }
    // ...
    protected:
    BarCallback * _clientCallbackBar; // pointer to client override
};
}

// implementation
void TR::Base::Bar() {
    // implementation
}
```

Finally, a special function is used to decide whether to call the client side
override or an implementation override. By convention such a function is given
the same name as the implementation function, but with the first letter lower
case. In addition, this is the function that must be called from other parts of
the implementation, *not the virtual implementation function*.

```c++
// declaration

extern "C" {
// ...
typedef void * (*BarCallback)();
// ...
}

namespace TR {
class Base {
    public:
    // ...
    virtual void Bar(); // contains implementation

    void bar() {
        // call the callback, if it is set
        if (_clientCallbackBar != NULL)
            _clientCallbackBar(client());
    }

    void setClientCallback_Bar(void * callback) {
        _clientCallbackBar = (BarCallback *) callback;
    }
    // ...
    protected:
    BarCallback * _clientCallbackBar; // pointer to client override
};
}

// implementation
void TR::Base::Bar() {
    // implementation
}
```

## Client API Design

This section describes how the client API is implemented, not how to use it.

For every class on the implementation side, there is a corresponding client API
class.

```c++
namespace OMR {
namespace JitBuilder {
class Base {
    public:
    void Foo();
    virtual void Bar();
    void * _impl;
};
}
}
```

For every member function of the implementation class, there is also a
corresponding client API function.

Fields are mapped in a similar fashion.

There is an addition `_impl` field, which is an opaque pointer to the
corresponding implementation object. This pointer must always point to the base
type of the object.

### Function call

In all cases, the implementation of client-side functions simply forward the
call to the implementation side.

```c++
void OMR::JitBuilder::Base::Foo() {
    static_cast<TR::Base *>(_impl)->Foo();
}

void OMR::JitBuilder::Base::Bar() {
    static_cast<TR::Base *>(_impl)->Bar();
}
```

The `_impl` field must be cast to the class type corresponding to the current
client API class. Because `_impl` always points to the base type of an object,
casting directly to the current type would result in undefined behaviour, if the
current type is not the base type. So, two casts must be done. The first cast
goes from `void *` to the base type and the second goes from the base type to
the derived type.

```c++
void OMR::JitBuilder::Derived::Bar() {
    // double cast to avoid undefined behaviour
    static_cast<TR::Derived *>(static_cast<TR::Base *>(_impl))->Bar();
}
```

#### Returning values

Returning values from a function is the first instance of a complication in the
client API. Two cases need to be considered:

1. the function returns a primitive type
2. the function returns an API/implementation defined type (a class)

The first case is straight forward, as the value returned from the
implementation-side function can be simply forwarded.

```c++
uint32_t OMR::JitBuilder::Base::Foo() {
    return static_cast<TR::Base *>(_impl)->Foo();
}
```

In the second case, given the value returned by the implementation-side function
is a pointer to an implementation-side object, the corresponding client-side
object must be retrieved and returned, taking care to properly handle possible
null pointers. For convenience, the `GET_CLIENT_OBJECT()` macro can be used.

```c++
OMR::JitBuilder::Other * OMR::JitBuilder::Base::Foo() {
    TR::Other * implRet = static_cast<TR::Base *>(_impl)->Foo();
    GET_CLIENT_OBJECT(clientObj, Other, implRet);
    return clientObject;
}
```

`GET_CLIENT_OBJECT(clientObj, Other, implRet)` will get the client object
corresponding to `implRet` by calling the `client()` method on the object. The
client object is then assigned to a new variable named `clientObj` of type
`Other` (the namespace qualifier must not be specified). If th`implRet` is null,
the `clientObj` will also be null.

#### Argument marshalling

Argument marshalling further complicates things as arguments of the following
types must be handled:

1. primitive types (including arrays of primitive types and in/out parameters
of primitive types)
2. API/implementation types (a class)
3. in-out API/implementation types
4. array of API/implementation types
5. vararg (mapped to an array of API/implementation type)

##### Primitive types

Once again, primitive types represent the simple case as the arguments can be
simply forwarded to the implementation-side function.

```c++
void OMR::JitBuilder::Base::Foo(int32_t a, double * b) {
    static_cast<TR::Base *>(_impl)->Foo(a, b);
}
```

##### API/implementation types

For class types, the implementation-side object corresponding to each object
must be retrieved, taking care to properly handle null pointers.

```c++
void OMR::JitBuilder::Base::Foo(OMR::JitBuilder::A * a) {
    static_cast<TR::Base *>(_impl)->Foo(static_cast<TR::A *>(a != NULL ? a->_impl : NULL));
}
```

Note that `_impl` always points the the type of the object itself, not the base type!

##### In-out API/implementation types

##### Arrays of API/implementation type

##### Vararg functions

### Callback thunks

### Constructors

#### Common initializer

#### The "impl" constructor

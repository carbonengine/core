# Telemetry

The `CcpTelemetry.h` header provides an instrumented profiling solution. Instrumentation is expressed
directly in the source code — you mark the scopes you care about, tag them with a *category*, and the
data is streamed to a connected [Tracy](https://github.com/wolfpld/tracy) profiler while a capture is
running.

Everything the header exposes falls into one of four groups:

| Group | Purpose | Key entry points |
| --- | --- | --- |
| Session control | Start, stop and pump a capture | `CcpStartTelemetry`, `CcpStopTelemetry`, `CcpTelemetryTick` |
| Instrumentation | Describe what the program is doing | `TelemetryZone`, `CcpTelemetrySetActiveFiber` |
| Categories | Group zones by subsystem | `CcpTelemetryCategoryRegister`, `CcpTelemetryGetRegisteredCategories` |
| Capture masks | Choose which categories are recorded | `CcpTelemetrySetActiveCategories`, `CcpTelemetryGetActiveCategories` |

## Build-time configuration

Telemetry is a compile-time feature, controlled by the `WITH_TELEMETRY` CMake option (`ON` by default).
The option defines `CCP_TELEMETRY_ENABLED` as a `PUBLIC` compile definition on the `CcpCore` target, so
consumers that link against `CcpCore` automatically agree with the library on whether telemetry is
compiled in:

```shell
cmake --preset <preset> -DWITH_TELEMETRY=OFF
```

```{important}
Instrumentation code should not be wrapped in `#if CCP_TELEMETRY_ENABLED` guards by callers. The
declarations in `CcpTelemetry.h` are always visible, and `CcpRegisterMutex` degrades into an empty
macro when telemetry is disabled.
```

## On-demand profiling

Nothing is captured until two things are true at the same time:

1. The application has requested a capture with `CcpStartTelemetry()`, and
2. a Tracy profiler client has connected to the process.

Because the Tracy client is built with `manual-lifetime`, the profiler server inside the process is
only spun up once a capture is requested, and it is `CcpTelemetryTick()` that drives the transition.
The session therefore behaves like a small state machine:

| State | Reached by | `CcpTelemetryIsConnectionRequested()` | `CcpTelemetryIsStarted()` | `CcpTelemetryIsStopped()` |
| --- | --- | --- | --- | --- |
| Stopped | initial state, or a tick after `CcpStopTelemetry()` | `false` | `false` | `true` |
| Start requested | `CcpStartTelemetry()` | `true` | `false` | `false` |
| Started | a tick after a profiler client connected | `false` | `true` | `false` |
| Stop requested | `CcpStopTelemetry()`, a lost connection, or an elapsed `captureDuration` | `false` | `false` | `false` |

`CcpTelemetryIsConnected()` is the one to check before doing work purely for the profiler's benefit: it
is `true` only while the session is started *and* a client is still attached.

### Starting a session

A session is configured with `CcpTelemetryConfig`:

| Field | Meaning |
| --- | --- |
| `applicationName` | Name shown in the profiler UI. |
| `captureDuration` | If non-zero, the capture stops itself this long after the profiler connected. Zero (the default) captures until `CcpStopTelemetry()` is called. |
| `trackMemoryAllocations` | Forward allocations reported by `CCPMemory` to the profiler. |
| `trackLocks` | Report `CcpMutex`, `CcpSpinLock` and `CcpSemaphore` contention to the profiler. |

```cpp
#include <CcpTelemetry.h>

CcpTelemetryConfig config;
config.applicationName = "MyGame";
config.captureDuration = std::chrono::seconds( 30 ); // omit for an open-ended capture
config.trackMemoryAllocations = true;
config.trackLocks = true;

CcpStartTelemetry( config );
```

`CcpStartTelemetry()` only records the request; it returns `false` if a capture is already started or
already pending. The actual work happens in `CcpTelemetryTick()`, which must be called regularly —
typically once per frame from the main loop:

```cpp
while( isRunning )
{
    CcpTelemetryTick();

    // ... rest of the frame ...
}
```

```{warning}
Without a regular `CcpTelemetryTick()`, a requested capture never becomes active, no frame marks are
produced, and timed captures never expire.
```

### Reacting to session changes

Register an event handler to hook subsystems into the session lifecycle. This is the natural place to
reset per-capture bookkeeping.

```cpp
void OnTelemetryEvent( CcpTelemetryEvent event, void* userData )
{
    switch( event )
    {
    case CCP_TELEMETRY_STARTED:
        // your code, f.e. toggle some UI state
        break;
    case CCP_TELEMETRY_STOPPED:
        // your code, f.e. toggle some UI state
        break;
    }
}

CcpRegisterTelemetryEventHandler( &OnTelemetryEvent, nullptr );
```

If a profiler is already connected when the handler is registered, it is invoked immediately with
`CCP_TELEMETRY_STARTED`. `CcpUnregisterTelemetryEventHandler()` matches on both the function pointer
*and* the `userData` value, so pass back exactly what was registered.

For a timed capture, `CcpTelemetryRemainingCaptureDuration()` reports how much of `captureDuration` is
left, clamped to zero. It returns zero for open-ended captures.

## Instrumenting code

### Zones

A *zone* is a named, timed span of work. Zones are created with `TelemetryZone`, which reports the span
using RAII: the zone begins where the object is constructed and ends when it goes out of scope.

```cpp
#include <CcpTelemetry.h>

namespace
{
    // Register once, reuse everywhere. The returned reference is valid for the lifetime of the
    // process, because categories are never unregistered.
    const CcpTelemetryCategory& PhysicsCategory()
    {
        static const CcpTelemetryCategory& category =
            CcpTelemetryCategoryRegister( "physics", CcpColor::Orange ).first;
        return category;
    }
}

void StepSimulation( const World& world, float deltaTime )
{
    TelemetryZone zone( PhysicsCategory(), "StepSimulation", __FILE__, __LINE__ );

    // ... simulate; the zone ends automatically here ...
}
```

The constructor takes the category, the name to display, and the source location of the zone:

| Parameter | Value to pass |
| --- | --- |
| `category` | A category obtained from `CcpTelemetryCategoryRegister()`. |
| `name` | Display name of the zone. Must not be `nullptr`. |
| `filename` | `__FILE__`. |
| `lineno` | `__LINE__`. |

Zones nest naturally with the call stack. A zone created inside another zone's scope is shown as a
child of it in the profiler. The zone's color comes from its category, so all zones belonging to one
subsystem are visually grouped.

Because the source location is part of every zone, it is common to hide the boilerplate behind a
per-subsystem macro. `CCP_ANONYMOUS_VARIABLE` from `CcpMacros.h` keeps the variable name unique:

```cpp
#include <CcpMacros.h>

#define PHYSICS_ZONE( zoneName ) \
    TelemetryZone CCP_ANONYMOUS_VARIABLE( physicsZone_ )( PhysicsCategory(), zoneName, __FILE__, __LINE__ )

void StepSimulation( const World& world, float deltaTime )
{
    PHYSICS_ZONE( "StepSimulation" );
    // ...
}
```

### Annotating zones

`TelemetryZone::text()` attaches a free-form string to a zone, which shows up next to it in the
profiler. Use it for the data that makes a particular sample interesting — an asset path, an entity
count, a request id:

```cpp
void LoadAsset( const std::string& path )
{
    TelemetryZone zone( AssetCategory(), "LoadAsset", __FILE__, __LINE__ );
    zone.text( path.c_str() );

    // ...
}
```

### Zone lifetime and ownership

`TelemetryZone` has no default constructor and cannot be copied — a zone must have exactly one owner
that is responsible for ending it. It *can* be moved, which is what makes it possible to keep a zone
alive beyond the scope that created it:

```cpp
std::optional<TelemetryZone> pending;

void BeginStreaming()
{
    pending.emplace( StreamingCategory(), "StreamChunk", __FILE__, __LINE__ );
}

void EndStreaming()
{
    pending.reset(); // the zone ends here
}
```

```{note}
Whether a zone is recorded is decided once, when the `TelemetryZone` is constructed: a capture must be
running and the zone's category must be active in the capture mask. A zone constructed outside a
capture stays a no-op for its whole lifetime, even if a capture starts before it goes out of scope.
Keep zones short-lived, and prefer creating them inside the scope being measured.
```

### Fibers

Code that runs on fibers (for example Python tasklets) can be resumed on a different OS thread than the one it
started on, which would otherwise produce interleaved, nonsensical call stacks. Telling telemetry which
fiber is currently active makes zone bookkeeping per-fiber instead of per-thread:

```cpp
CcpTelemetrySetActiveFiber( "WorkerFiber1" );
{
    TelemetryZone zone( ScriptCategory(), "RunTasklet", __FILE__, __LINE__ );
    // ... work performed on behalf of the fiber ...
}
CcpTelemetrySetActiveFiber( "" ); // back to the root, "no fiber" context
```

- The empty string is the root context, meaning "not on a fiber".
- `CcpTelemetryGetActiveFiber()` returns the calling thread's current fiber name.
- `CcpTelemetryRemoveFiber()` retires a fiber name once the fiber is gone. If the removed fiber is the
  calling thread's active one, the thread falls back to the root context. The name itself is only
  released after a short grace period, on a later `CcpTelemetryTick()`.

A zone always ends on the fiber it started on, even if the owning `TelemetryZone` is destroyed after
the thread has switched to another fiber.

### Locks and memory

Lock and allocation tracking need no instrumentation in application code — they are wired into the
primitives themselves and gated on the session configuration:

- **Locks.** `CcpMutex`, `CcpSpinLock` and `CcpSemaphore` announce themselves to the profiler and report
  their wait/obtain/release events while `trackLocks` is enabled and a profiler is connected. A
  `CcpMutex` appears under `<owner>-<name>` from its constructor arguments, so pass something
  meaningful there. `CcpRegisterMutex()` is called by `CcpMutex` itself; there is no need to call it
  directly.
- **Memory.** The `CCPMemory` allocators call `CcpTelemetryTrackAllocation()` and
  `CcpTelemetryTrackDeallocation()` for you while `trackMemoryAllocations` is enabled. Call them
  directly only from a custom allocator.

Use `CcpTelemetryLockTrackingIsEnabled()` and `CcpTelemetryMemoryTrackingIsEnabled()` to query what the
running session was configured with.

## Telemetry categories

A `CcpTelemetryCategory` groups related zones — usually one category per subsystem. Categories give
zones their color in the profiler, and, more importantly, they are the unit of selection for capture
masks.

The type is opaque: `CcpTelemetry.h` only forward-declares it. Instances are only ever handed out by
reference from the registry, and are inspected through free functions:

```cpp
auto [category, ok] = CcpTelemetryCategoryRegister( "rendering", CcpColor::Green );

const std::string& name = CcpTelemetryCategoryGetName( category );
CcpColor color = CcpTelemetryCategoryGetColor( category );
```

To hold several of them, use `CcpTelemetryCategories` which is also the type the registry and mask
functions speak.

### Registering a category

`CcpTelemetryCategoryRegister()` is idempotent by name: registering a name that already exists returns
the existing category rather than creating a second one. This makes it safe to call from several
translation units, and means a subsystem never has to publish its category handle to its callers.

```cpp
auto [category, ok] = CcpTelemetryCategoryRegister( "rendering", CcpColor::Green );

// Later, elsewhere — same category, the color argument is ignored for an existing name.
auto [same, stillOk] = CcpTelemetryCategoryRegister( "rendering" );
```

```{note}
Category names are case-sensitive.
```

The `bool` is `false` when a category cannot be registered. This can happen, for example, when the name was empty, or 
all category slots are taken. On failure the returned reference refers to a shared, empty placeholder category.

```{warning}
Always check the `bool`. Zones tagged with the placeholder category returned on failure can never be
captured, because the placeholder owns no capture flag.
```

### Built-in categories

Three categories are registered before any application code runs, and always occupy the first three
slots:

| Name | Color | Notes |
| --- | --- | --- |
| `general` | `CcpColor::SteelBlue` | Default category; corresponds to the legacy `TMCM_GENERAL`. |
| `cpp` | `CcpColor::Yellow` | Used by the deprecated `CcpTelemetryEnterZone()` path; corresponds to the legacy `TMCM_CPP`. |
| `core` | `CcpColor::LightGreen` | Zones inside *carbon-core* itself. |

`CcpTelemetryGetRegisteredCategories()` returns everything registered so far, in registration order,
starting with those three:

```cpp
for( const CcpTelemetryCategory& category : CcpTelemetryGetRegisteredCategories() )
{
    printf( "%s\n", CcpTelemetryCategoryGetName( category ).c_str() );
}
```

## Capture masks

A capture mask is the set of categories that are actually recorded. When a `TelemetryZone` is constructed, 
its category is tested against the set of active categories. If a zone's category is not part of that set,
said zone will not show up in the profiler.

This is what makes instrumentation affordable to leave in shipping code: you can instrument
generously, then decide at runtime which subsystems a given capture should actually pay for.

```{important}
The active mask starts out **empty**, and `CcpStartTelemetry()` does not change it. Until
`CcpTelemetrySetActiveCategories()` is called, a connected profiler receives frame marks, lock and
memory events — but no zones.
```

### Setting the mask

`CcpTelemetrySetActiveCategories()` **replaces** the active set; it is not additive:

```cpp
auto [rendering, renderingOk] = CcpTelemetryCategoryRegister( "rendering" );
auto [physics, physicsOk] = CcpTelemetryCategoryRegister( "physics" );

// Capture rendering and physics zones, and nothing else.
CcpTelemetrySetActiveCategories( { rendering, physics } );
```

Passing an empty list clears the mask and stops all zone capture:

```cpp
CcpTelemetrySetActiveCategories( {} );
```

Capturing everything is a matter of feeding the registry back in:

```cpp
CcpTelemetrySetActiveCategories( CcpTelemetryGetRegisteredCategories() );
```

The call returns `false` — leaving the mask unchanged — if too many entries are passed in.
Duplicate entries are harmless, as are references to categories that never registered successfully;
they are simply ignored.

### Reading the mask back

`CcpTelemetryGetActiveCategories()` returns the currently active categories, in registry order. Pairing
it with `CcpTelemetryGetRegisteredCategories()` is all that is needed to drive a debug UI, a console
command, or a config file:

```cpp
void DumpCategoryState()
{
    const auto active = CcpTelemetryGetActiveCategories();

    for( const CcpTelemetryCategory& category : CcpTelemetryGetRegisteredCategories() )
    {
        const bool isActive = std::find_if( active.begin(), active.end(),
            [&category]( const CcpTelemetryCategory& candidate ) { return candidate == category; } ) != active.end();

        printf( "[%c] %s\n", isActive ? 'x' : ' ', CcpTelemetryCategoryGetName( category ).c_str() );
    }
}
```

Since the setter replaces the whole set, toggling a single category is a read-modify-write of the
active list:

```cpp
void SetCategoryActive( const CcpTelemetryCategory& category, bool active )
{
    auto categories = CcpTelemetryGetActiveCategories();

    auto it = std::find_if( categories.begin(), categories.end(),
        [&category]( const CcpTelemetryCategory& candidate ) { return candidate == category; } );

    if( active && it == categories.end() )
    {
        categories.emplace_back( category );
    }
    else if( !active && it != categories.end() )
    {
        categories.erase( it );
    }

    CcpTelemetrySetActiveCategories( categories );
}
```

### When mask changes take effect

The mask is consulted when a zone is *constructed*, so a change applies to zones created afterwards.
Zones that are already open keep the state they were created with and are reported normally when they
end. In practice this means a mask change lands within a frame, and never produces a truncated or
orphaned zone.

```{note}
Registering categories and setting the mask are serialized against each other, and are meant to be
driven from a single control path — a debug UI, a console command, or a telemetry event handler. Zone
construction reads the mask without synchronization, so a mask change may be observed a moment later by
threads that are already running.
```

### A worked example

Putting the pieces together — a 10 second capture that only records the rendering subsystem:

```cpp
void CaptureRenderingProfile()
{
    auto [rendering, ok] = CcpTelemetryCategoryRegister( "rendering", CcpColor::Green );
    if( !ok )
    {
        return;
    }

    CcpTelemetryConfig config;
    config.applicationName = "MyGame";
    config.captureDuration = std::chrono::seconds( 10 );

    if( CcpStartTelemetry( config ) )
    {
        CcpTelemetrySetActiveCategories( { rendering } );
    }
}
```

The capture becomes active as soon as a Tracy profiler connects, records only zones tagged
`rendering`, and stops itself 10 seconds later — provided `CcpTelemetryTick()` keeps being called.

## Legacy API

Older call sites use an earlier, bitmask-oriented API that is still supported but deprecated. New code
should not use any of it:

| Deprecated | Replacement |
| --- | --- |
| `TelemetryZone( uint32_t handle, ... )` | `TelemetryZone( const CcpTelemetryCategory&, ... )` |
| `CcpTelemetryEnterZone()` / `CcpTelemetryLeaveZone()` | A scoped `TelemetryZone` |
| `CcpTelemetryZoneAddText()` | `TelemetryZone::text()` |
| `CcpStartTelemetry( const char*, int, uint32_t )` | `CcpStartTelemetry( const CcpTelemetryConfig& )` |

`TMCM_GENERAL` and `TMCM_CPP` are the surviving legacy category constants. They date from when
categories were selected with a hand-written bitmask, and they happen to line up with the capture bits
of the built-in `general` and `cpp` categories, which is why the deprecated zone constructor still
works. The manual `CcpTelemetryEnterZone()` / `CcpTelemetryLeaveZone()` pair keys zones on an opaque
pointer and requires the calls to be balanced by hand; `TelemetryZone` does the same job with RAII.

## API reference

Signatures and per-function details for everything described here are generated from the sources — see
the {doc}`api` section.

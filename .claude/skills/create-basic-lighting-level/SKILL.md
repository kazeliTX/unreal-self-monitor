---
name: create-basic-lighting-level
description: This skill should be used when the user asks to "create a new level with lighting", "add sky and lighting to a level", "set up basic lighting", "create outdoor scene", "新建关卡并添加基础光照", "添加天空大气和光照", or any variation of creating/setting up an Unreal Engine level with sky atmosphere and directional lighting via MCP.
version: 1.0.0
---

# Create Basic Lighting Level (UnrealMCP)

Complete workflow for creating a new named level with realistic sky atmosphere and lighting via UnrealMCP tools.

## When This Skill Applies

- User wants to create a new Unreal Engine level
- User asks to set up basic outdoor lighting (sun, sky, fog)
- User needs a starting point for a scene with visible sky

## Required MCP Tools

- `safe_switch_level` — create/switch levels without save dialogs
- `spawn_actor` — spawn `DirectionalLight`, `SkyLight`, `SkyAtmosphere`, `ExponentialHeightFog`
- `set_actor_property` — set component-level properties (searches actor + all components)
- `set_actor_transform` — position/rotate actors
- `save_current_level` — persist changes

## Complete Workflow

### Step 1 — Create Named Level

```python
safe_switch_level(asset_path="/Game/Maps/MyLevelName")
# Automatically pre-saves current level to avoid "Save changes?" dialog
```

> **Note**: After `new_level`/`safe_switch_level`, the editor may take 5-10s for shader
> compilation. Send follow-up commands individually; batch may timeout.

### Step 2 — Spawn Lighting Actors

```python
# Directional light (sun) — elevated, angled downward
spawn_actor(actor_type="DirectionalLight", name="Sun_Light",
            location=[0, 0, 300], rotation=[-45, 30, 0])

# Sky light — captures ambient light from sky
spawn_actor(actor_type="SkyLight", name="Sky_Light")

# Sky atmosphere — physical sky scattering model
spawn_actor(actor_type="SkyAtmosphere", name="Sky_Atmosphere")

# Optional: exponential height fog for depth/haze
spawn_actor(actor_type="ExponentialHeightFog", name="Height_Fog")
```

### Step 3 — Enable Atmospheric Sun Light (CRITICAL)

```python
set_actor_property(name="Sun_Light",
                   property_name="bAtmosphereSunLight",
                   property_value=True)
```

> **Why required**: `SpawnActor<ADirectionalLight>` via C++ does NOT auto-set
> `bAtmosphereSunLight = true` (unlike editor drag-drop). Without this, `SkyAtmosphere`
> won't connect to the directional light → scene renders black.
>
> The property lives on `UDirectionalLightComponent` (component level). `set_actor_property`
> searches the actor first, then falls back to all components automatically.

### Step 4 — Save

```python
save_current_level()
```

### Step 5 — Verify with Screenshot

```python
take_and_read_screenshot()
# Confirm sky is visible and blue
```

## Expected Result

A level with:
- Blue atmospheric sky (gradient from blue to pale at horizon)
- Soft directional sunlight casting shadows
- Ambient light from sky capturing (SkyLight)
- Optional ground haze (ExponentialHeightFog)

## Compile Notes

### When to use LiveCoding vs full_rebuild

| Change | Method |
|--------|--------|
| New actor types in `spawn_actor` | LiveCoding (`trigger_hot_reload`) |
| `set_actor_property` component fallback fix | LiveCoding |
| New MCP command registered in `RegisterCommands()` | **full_rebuild** required |

### LiveCoding limitations
`RegisterCommands()` runs at module init — LiveCoding only patches function bodies.
New command registrations require `full_rebuild` (kill + UBT + relaunch).

## Troubleshooting

| Symptom | Cause | Fix |
|---------|-------|-----|
| Scene completely black | `bAtmosphereSunLight` not set | Step 3 above |
| "Save current changes?" dialog | Level was dirty before switch | Use `safe_switch_level` |
| "Recover Packages" dialog on restart | Force-killed editor left recovery files | Delete `Saved/Autosaves/PackageRestoreData.json` |
| Command timeout after `new_level` | Shader compilation in progress | Wait 5-10s, send commands individually |
| New MCP command not in `get_capabilities` | LiveCoding won't re-run `RegisterCommands()` | Use `full_rebuild` |

## Key C++ Implementation Notes

### `HandleSpawnActor` — lighting actor types
```cpp
// UnrealMCPEditorCommands.cpp
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Components/SkyAtmosphereComponent.h"

else if (ActorType == TEXT("DirectionalLight"))
    NewActor = World->SpawnActor<ADirectionalLight>(...);
else if (ActorType == TEXT("SkyLight"))
    NewActor = World->SpawnActor<ASkyLight>(...);
else if (ActorType == TEXT("SkyAtmosphere"))
    NewActor = World->SpawnActor<ASkyAtmosphere>(...);
else if (ActorType == TEXT("ExponentialHeightFog"))
    NewActor = World->SpawnActor<AExponentialHeightFog>(...);
```

### `HandleSetActorProperty` — component property fallback
```cpp
// Falls back to actor components when property not found on actor itself
bool bFound = SetObjectProperty(TargetActor, PropertyName, ...);
if (!bFound)
{
    for (UActorComponent* Comp : TargetActor->GetComponents().Array())
        if (SetObjectProperty(Comp, PropertyName, ...)) { bFound = true; break; }
}
// Returns {"success":true, "component":"LightComponent0"} when found on component
```

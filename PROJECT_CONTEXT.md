# Project Context: h-AI-lucinations

Last checked: 2026-10-26. This is a handoff for an LLM or developer continuing the project. It describes the current implementation, not a redesign proposal. Source code and current asset files take precedence if they differ from this snapshot.

## Project Identity

- A desktop, top-down pixel-art game about friendly robots that unpredictably become hostile.
- Repository/workspace folder: `AIGoneWrong`.
- Visible game title: `h-AI-lucinations`.
- Internal CMake project and executable target: `hAIlucinations`.
- Output executable basename: `h_AI_lucination`.
- Debug has a console and defines `DEBUG` plus `DEBUG_BUILD`. Windows Release uses the GUI subsystem, without a console.
- Entry point: [source/main.cpp](source/main.cpp). It loads config and starts `MainMenu`; SDL supplies the Windows entry-point wrapper through `SDL_main.h`.

## Build and Run

The verified development environment is Windows, PowerShell, CMake, Ninja Multi-Config, and MinGW UCRT. The code uses C++23. Ensure the selected toolchain, Ninja, CMake, and Git are available; initial configuration may fetch dependencies.

From the repository root:

```powershell
cmake --preset windows-ninja
cmake --build --preset debug --target hAIlucinations
cmake --build --preset release --target hAIlucinations
```

Build settings are in [CMakeLists.txt](CMakeLists.txt) and [CMakePresets.json](CMakePresets.json). The build directory is **inside the project**, at `out/build/windows-ninja`, not the old parent-directory `../out` location.

Run with the configuration-specific output folder as the working directory:

```powershell
Set-Location .\out\build\windows-ninja\Debug
.\h_AI_lucination.exe
```

The output folder contains runtime configs, assets, SDL DLLs, and license files. Relative paths are resolved from the working directory; launching with the wrong directory can break config loading, maps, audio, or progression.

For config-only or asset-only refreshes, these helper targets avoid unnecessary recompilation:

```powershell
cmake --build --preset debug --target hAIlucinationsConfig
cmake --build --preset debug --target hAIlucinationsAssets
```

In VS Code, prefer CMake Tools and the explicit `hAIlucinations` target. A stale selected target from the former `GDFramework` name can prevent a build. A running game may lock copied assets or the executable; close that game normally before rebuilding if copying/linking reports permission errors.

### Dependencies

Versions/pins are controlled by CMake; do not upgrade casually:

| Dependency | Current Version/Use |
| --- | --- |
| SDL3 | 3.4.14, windows/input/rendering |
| SDL3_image | 3.4.4, image loading |
| SDL3_mixer | 3.2.4, audio |
| SDL3_ttf | 3.2.2, fonts |
| Box2D | 3.1.1, C API physics and spatial queries |
| nlohmann/json | 3.11.3 fallback download; local headers can override |
| tinyxml2 | 11.0.0, TSX parsing |
| Mapbox Earcut | 2.2.4, collision-polygon triangulation |
| Vienna Game AI | Commit `6c551ff614e891e81172e640a0a96bcb06e7642e`, A* |

Windows SDL packages live under `libs/<toolchain>/`. CMake fetches other dependencies under the build directory's `_deps` folder. [cmake/PrepareViennaGameAI.cmake](cmake/PrepareViennaGameAI.cmake) prepares a build-local Vienna header; do not edit downloaded upstream sources to implement game changes.

## Code Entry Points

| Area | Owner |
| --- | --- |
| Main loop, scene lifecycle, presentation | [source/Game/Game.cpp](source/Game/Game.cpp) |
| Scene fades and handoff | [source/Game/Scenes/Scene.h](source/Game/Scenes/Scene.h), [source/Game/Scenes/Scene.cpp](source/Game/Scenes/Scene.cpp) |
| Shared map gameplay, camera, pause, dialog, Game Over | [source/Game/Scenes/GameScenes/MapScene.cpp](source/Game/Scenes/GameScenes/MapScene.cpp) |
| Numbered levels and progression | [source/Game/Scenes/GameScenes/Level.cpp](source/Game/Scenes/GameScenes/Level.cpp) |
| Finale | [source/Game/Scenes/GameScenes/Office.cpp](source/Game/Scenes/GameScenes/Office.cpp) |
| Packed entities, physics, animation, rendering | [source/Entities/Entities.h](source/Entities/Entities.h), [source/Entities/Entities.cpp](source/Entities/Entities.cpp) |
| Robot decisions, perception, patrol, chase, teleport | [source/Entities/Robot.cpp](source/Entities/Robot.cpp) |
| Shared robot types and tuning schema | [source/Types/RobotTypes.h](source/Types/RobotTypes.h) |
| Rendering and texture ownership | [source/Core/Renderer.h](source/Core/Renderer.h), [source/Core/Renderer.cpp](source/Core/Renderer.cpp) |
| Tiled map loading | [source/Core/TiledLevelLoader.cpp](source/Core/TiledLevelLoader.cpp) |
| Vienna/Box2D navigation adapter | [source/Core/Navigation/Navigation.cpp](source/Core/Navigation/Navigation.cpp) |
| Config parsing and window-settings persistence | [source/Config/Config.h](source/Config/Config.h) |
| Menus and overlays | `source/Game/Scenes/Menus/` |
| Reusable dialog | [source/Game/UI/DialogSystem.cpp](source/Game/UI/DialogSystem.cpp) |
| Input | [source/Input/InputManager.cpp](source/Input/InputManager.cpp) |
| Sprites, audio, fonts | `source/AssetManagers/` |

## Configuration

- [config.json](config.json): window settings, initial entity capacity, per-scene zoom, audio keys, image/sheet registrations, and fonts.
- [Config/robotProperties.json](Config/robotProperties.json): robot sprite keys, scale/pivots, colliders, vision, behavior intervals, weighted personalities, movement/chase, rotation, and teleport tuning.
- Source config and the copied runtime config are separate files. The game loads config at startup; restart it after changing source zoom or registrations and refresh the build's config/assets.
- Settings saves merge only window width/height/fullscreen into the working-directory config. Preserve other keys, including `view_scales`; do not restore an AppData settings system.
- [cmake/CopyConfigIfNewer.cmake](cmake/CopyConfigIfNewer.cmake) preserves a newer runtime config rather than overwriting saved window settings on every code rebuild.
- `view_scales` keys are exact scene names. Current values: `Room=1.0`, `Level_01=2.0`, `Level_02=2.5`, `Level_03=3.0`, `Office=1.0`. Larger values zoom in over the map-fit scale. Missing entries fall back to `1.0`; supplied values must be positive and finite.
- `Level(index)` and `Room()` read configured zoom. `Level(index, explicitScale)` remains available; restarts preserve the current explicit scale. Scene jumps use the destination's configured zoom, not the outgoing scene's zoom.

## Scene Flow and Controls

Normal entry is `MainMenu -> Tutorial -> Room -> Level 1`.

Numbered levels progress sequentially: **Level 1 -> Level 2 -> Level 3 -> Office**. Exit checks for `assets/Maps/Level_{next:02}.json`: load it when present, otherwise enter Office. Do not skip Level 2. Adding Level 4 automatically makes Level 3 -> Level 4 -> Office. Office completes to MainMenu.

- Movement: WASD or arrow keys.
- Dialog advance: a fresh Space press.
- Pause/resume: Escape.
- Debug gameplay `M`: toggle physics outlines and cyan Medusa teleport-radius circles.
- While paused in Debug: `1`, `2`, `3` select numbered levels; `R` selects Room; `O` selects Office. Numpad 1/2/3 also work. Repeats are ignored. These shortcuts do not intercept the Settings child and are excluded from Release.
- Pause freezes gameplay, physics, robot timers, particles, and intro timers; music pauses and resumes only if it was playing before pause.
- In-game Settings: Apply stays on Settings; Return or Escape goes back to Pause, not directly into gameplay.
- Game Over freezes gameplay, stops music, and offers Restart Level or Main Menu. Restart recreates the same scene/index/zoom.
- Scene handoffs use shared fades. Clean up the previous scene before initializing its replacement; keep a single final presentation step in the game loop.
- Room and Level 1 contain placeholder intro dialogs. Preserve user-authored speaker text and other narrative edits unless asked to change them.

## Entity and Physics Invariants

- There is **no Player class**. The player is ordinary `EntityData` with `EntityBehavior::PlayerInput`.
- `Entities` is a singleton owning parallel packed arrays for positions, sprites, animation, bodies, doors, robot types/state, etc. Add/remove/rollback/clear operations must keep every array aligned.
- `Entities::PlayerIndex == 0` is reserved for the name `player`. Do not reorder packed arrays for rendering or promote another entity into that slot.
- Controller/personality dispatch uses shared member-function tables, not per-entity captured callbacks or enemy subclasses.
- World/gameplay positions are map pixels, anchored at the entity's **feet**. Box2D body positions are collider **centres**. Convert `bodyY = (feetY - colliderHeight/2) / PixelsPerMeter`, and add half-height when syncing back. This distinction is essential for teleports and exact home arrival.
- `PixelsPerMeter=32`, zero gravity, fixed `1/60` second physics steps, four Box2D substeps.
- Player speed is 96 pixels/second, visual scale 0.8, collider 9.6 x 4.8 pixels.
- Moving/hostile robots are kinematic; stationary friendly robots are static. Player forces must not push robots around.
- Terrain/doors use category 1; actors use category 2. Vision normally queries mask 1. Robot movement and teleport occupancy use mask 3, excluding the robot's own body.
- Sweep the full collider before movement; retain collision clearance and never solve stuck AI by disabling collisions.
- Track attempted movement even when collision clamps actual velocity to zero. Blocked patrols reverse on either leg. A stalled chase/home return replans around other robots, ignoring self and the target player.
- Box2D contact skin can produce fraction-zero casts while objects are separated. Permit strictly separating motion only; do not ignore inward hits or remove wall checks.

## Robot Behavior

Currently integrated types are ScoutBot, TrooperBot, and MedusaDrone. More robot asset folders exist but are not automatically gameplay types.

- Personality metadata supplies `friendly`, `detectsPlayer`, and `rotates`; runtime state lives in the packed entity owner.
- Timed selection alternates friendly/hostile groups, applying JSON weights only within the opposite group. Do not invent fallback weights in C++.
- Default friendly duration is 2-4 seconds; hostile duration is 3-5 seconds. Level 1 overrides only the friendly interval to 1 second.
- A hostile phase that acquires the player receives one 1.5x duration multiplier: 4.5-7.5 seconds before transition effects. Reacquisition must not repeatedly extend it.
- Changing personality stops movement and plays seven `hits-4` frames; the cone blinks pink/orange. Routine cone colors: friendly pink, hostile unaware green, peripheral yellow, chasing red.
- Friendly robots, personality transitions, and teleport phases cannot trigger capture. Gameplay capture also respects intro/dialog control locks.
- Capture checks hostile close-contact collider overlap plus unobstructed terrain LOS, before exit triggers, then raises a consumed Game Over event.
- All current chase speeds are 76.8 pixels/second; patrol/home speed is 32 by default.
- Scout/Trooper default hostile choice is `change-axis-aggressive`: swap the patrol axis, patrol until acquiring a player, then use the shared chase. Do not reduce it to a patrol-only handler.
- Normal friendly changes return to the original spawn through A*, with the friendly countdown paused until home.
- Once chasing, keep tracking a visible player through turns; initial cone-facing restrictions no longer apply, but range/LOS and bounded last-seen memory still do.

### MedusaDrone

- Uses 96 x 96 idle/drift cells, eight frames per row, directions `S, SW, W, NW, N, NE, E, SE`; pivot `(48,79)`, scale 0.5. Rotation drives both directional frames and continuous cone direction.
- Friendly: rotate in place at home. Hostile: complete one turn while scanning, then teleport when not chasing; acquiring the player uses normal A* pursuit.
- Teleport: seven `hits-6` departure frames, 0.5 seconds hidden with body disabled, then seven arrival frames and resumed rotation/chase.
- Random destinations are in a 100-pixel disk around departure, constrained by map bounds, full-collider floor coverage, walls/doors, and actors. Blank space inside the rectangular map is not valid floor.
- After becoming friendly, return to the original spawn. If return takes 2 seconds, trigger the same teleport effect sequence targeting home rather than a random point. Complete arrival before restarting friendly rotation/countdown; arrival handling must precede timeout checks to avoid an endless effect loop.
- Tune rotation speed, radius, hidden/frame durations, attempts, and return timeout in the Medusa profile, not hardcoded per scene.

## Reaction Bubbles

- Root `config.json` has a `reactions` section: player text `Yikes!`, speech duration 1 second, shared player cooldown 10 seconds, robot emote duration 1 second, frame interval 0.1 seconds.
- Entering a hostile robot's actual cone with terrain LOS raises an animated `!` and may show player speech. This is an entry event, not a continuous retrigger. All robot types support it, but friendly personalities do not trigger spotting emotes or player speech. The hostile-to-friendly `?` event remains separate.
- A committed hostile-to-friendly change raises `?`, which takes priority over a coincident `!`. Initial friendly spawn does not raise a question mark.
- Speech uses the first panel from `assets/Sprites/UI/single-line-tilemap.png`; emotes use six explicit cells from `bubble emotes july update.png` (three exclamation frames, then three question frames). Preserve these config registrations when changing UI assets.
- Reaction timers respect gameplay control locks and pause, and clear with scene entities. The player cooldown is shared across all robots in the current scene.
- `Entities::RenderReactions` draws after world props using the shared camera, before dialog/pause overlays. Offscreen and hidden actors do not leave edge bubbles; visible bubbles fit the viewport and stack to reduce overlap.

## Maps and Tiled Authoring

- Runtime exports live in `assets/Maps/`; editable `.tmx` sources live in `metadata/`.
- Supports finite orthogonal Tiled JSON-array maps, external TSX atlases and per-tile image collections, tile objects, flips, offsets, visibility/opacity, and rectangle/polygon collisions. Do not assume all TSX files contain a single atlas.
- TSX is XML, not React/TypeScript. Keep the workspace's tileset file association scoped accordingly.
- Resolve map -> TSX -> image paths relative to each containing file. Runtime exports must not require the source-only `metadata/` folder.
- Required gameplay point: `PlayerSpawn`. Optional end point: `triggerEndLevel`, activated within 16 world pixels.
- Robot spawns are uniquely named Tiled points, using class/type `ScoutBot`, `TrooperBot`, or `MedusaDrone`; optional string property `direction` is up/down/left/right.
- `floor`: occupied tiles define playable landing coverage. `collision`: full occupied-cell blockers. `collision_in_frame`: precise TSX collision shapes; concave polygons are triangulated with Earcut. Hidden collision layers remain active.
- Doors use existing tile-object/entry metadata and can change their visual frame and enabled physics state.
- Level 3 uses ChemicLab image-collection props through [assets/Maps/tileset/Lab.tsx](assets/Maps/tileset/Lab.tsx). It is the packaged canonical tileset; the older metadata copy is not the runtime source.
- [metadata/Level_03.tmx](metadata/Level_03.tmx) exports to [assets/Maps/Level_03.json](assets/Maps/Level_03.json). [metadata/Office.tmx](metadata/Office.tmx) exports to [assets/Maps/Office.json](assets/Maps/Office.json). Do not point these exports at Level 2.
- Preserve user-authored geometry, prop placements, collision edits, spawns, and names. Duplicate robot names prevent loading; one duplicate Scout was previously renamed `Scout_03`.

## Rendering and Occlusion

- Route game/map drawing and image loading through `Renderer`. Use SDL3's automatic batching; do not add a custom flush/batch queue or SDL2 batching hints.
- Tiles, entities, debug geometry, and props share the same `RenderView` camera transform. World pixels are not screen pixels or physics metres.
- `WorldSprite` represents source rect, world destination, physical depth, opacity, angle, and flip.
- Lab collection props merge with actors in stable feet/footprint-depth order. Preserve authored floor/upper-layer order and do not sort the packed entity arrays themselves.
- The Tiled render callback passes a fifth `std::span<const WorldSprite>` argument to the entity pass. Collection maps without an actor marker fall back to normal prop drawing.
- When an actor is behind a foreground prop, only prop pixels overlapping that actor's visible silhouette fade to 50% opacity. Keep the rest opaque; actors in front remain fully visible. Respect frame changes, alpha holes, flips, and rotations.
- `Renderer::loadTexture` stores original RGBA pixels in texture properties and reuses a streaming scratch texture for the mask. Sprite loading must use this same path; separate SDL uploads bypass actor-alpha caching and break occlusion.
- Cyan Debug circles show the full configured Medusa sampling radius, not the floor-clipped set of valid destinations. During disappearance/hidden phases they remain centred on the departure origin.

## Navigation

- Keep Vienna as the A* engine. Its header is private to one navigation translation unit to avoid upstream ODR/link errors.
- The adapter uses half-tile grid cells, cardinal edges, Box2D overlap and full-shape sweeps, live doors, and a bounded grid size.
- Grid nodes/edges keep a one-pixel clearance. Exact start/goal connectors use actual collider size so wall-adjacent actors or authored spawns do not become unreachable solely due to extra planning padding.
- Actor-aware recovery uses `NavigationRequest.obstacleMask` and `ignoredBodies`; default terrain-only queries remain supported.
- Preserve turning waypoints and validate connections. Aggressive smoothing and incorrect near-contact handling previously caused stalls or corner cutting.

## Audio and Asset Credits

Music is registered under `sound` in the root config and selected through `AudioTrack`; `AudioManager` lazily loads compressed tracks and loops one selected track.

| Scene | Current Track |
| --- | --- |
| Main menu | `vgm-atmospheric-deepspacefrigate.mp3` |
| Room | `QueerSample.flac` |
| Level 1 | `Dead Cities Cut.flac` |
| Level 2 | `Infiltration.flac` |
| Level 3 | `UNATCO.mp3` |
| Office | `Costa del Sol.mp3` |

Preserve supplied licenses and Credits entries, including the Garbage sample, Future Sound of London, Norihiko Hibino / Metal Gear Solid 2: Sons of Liberty, Michiel van den Bos / Deus Ex, and Nobuo Uematsu / Final Fantasy VII attribution.

## Working and Validation Rules

- Inspect current files before editing; user edits frequently occur between requests. Do not undo unrelated changes, overwrite maps, rename the game again, or restore outdated progression/zoom defaults.
- Keep new behavior in the established packed systems and shared handlers. Prefer JSON tuning and existing helpers over per-enemy classes or scattered constants.
- **The user requested no permanent project or dependency test files/targets.** Keep `BUILD_TESTING` and `tinyxml2_BUILD_TESTING` off. Use scoped builds and temporary external probes where appropriate, then clean up probes and captures.
- Recent checks covered Debug builds, all packaged maps, image-collection pixels/flipped footprints, Lab front/behind alpha, robot contact recovery and detours, Medusa timing/home return, zoom, and Debug shortcut Release exclusion. These are past verification results, not a permanent regression suite; rerun checks relevant to a change.
- For native visual checks, review actual rendered pixels/screenshots rather than assuming a successful build proves rendering. Preserve player index, collider safety, frame pivots, camera alignment, and texture lifetimes.
- Local MinGW issue observed: a modified float `std::hypot` overload caused recursion. Existing code uses bounded-vector `sqrt`/squared distances; do not patch external SDK headers as a project fix.
- When linking temporary probes, derive object paths from current source files; stale objects from deleted/renamed files can remain in the build cache.
- PowerShell terminal input sometimes becomes corrupted or output is omitted. A standalone Ctrl+C followed by retry has recovered it. Do not infer test success from missing output, poll blindly, or change unrelated code to compensate for a terminal problem.

## Before Continuing Work

1. Read the relevant owner listed above and the current configs/map files.
2. Preserve current user changes and explicitly requested exceptions.
3. Make the smallest coherent change and run a focused check immediately.
4. For new numbered levels, export the correct JSON filename, keep assets self-contained, add a `view_scales` entry, verify spawns/collisions, and confirm the last numbered level still leads to Office.
5. Update this context when architecture, progression, build names, or data contracts change.
# jnpf. editor and runtime guide

The `jnpf` executable opens directly into an editor workbench. Editing and simulation happen in one process, but Play Mode reconstructs a private runtime copy and restores the authored scene when stopped.

## Workbench layout

The borderless main window contains these primary surfaces:

| Surface | Purpose |
|---|---|
| **Scene** | Editor-camera view used for navigation, selection, object creation, and transform gizmos |
| **Game** | Output from the primary authored camera; receives captured game input during Play Mode |
| **Elements** | Searchable object hierarchy with activation, multi-selection, drag-and-drop parenting, duplicate, and delete actions |
| **Inspector** | Object name, activation, transform, camera, material, texture, physics, behavior, and light properties |
| **Stats** | Frame time/FPS history, object and draw counts, physics timing/body/contact/worker counts, and render queue summary |
| **Asset Lens** | Filtered project asset list with refresh, drag sources, and OBJ import |
| **Console** | Filterable in-memory Info/Warning/Error stream with follow and clear controls |

The **Window** menu toggles Stats, Asset Lens, and Console. ImGui docking state is persisted in `imgui.ini`.

## Scene navigation

Scene navigation only activates when the Scene viewport is hovered or focused. Text entry and active UI widgets suppress conflicting shortcuts.

| Input | Action |
|---|---|
| Hold right mouse + move | Free-look |
| Right mouse + `W` / `A` / `S` / `D` | Move forward/left/back/right |
| Right mouse + `Q` / `E` | Move down/up |
| Hold `Shift` while moving | Boost from 10 to 40 units/second |
| Middle mouse + drag | Pan |
| Mouse wheel | Dolly along camera forward |
| `Alt` + right mouse + vertical drag | Dolly |
| `Alt` + left mouse + drag | Orbit the selected object or the current orbit pivot |
| Arrow keys | Move along camera forward/right while Scene is focused |
| `F` | Frame the selected object |

The editor camera starts at `(5, 4.5, -7.5)` with a 45-degree field of view. It is a special object named `Editor Camera`: it is preserved for authoring and cannot be deleted like ordinary scene content.

## Selection and hierarchy

- Click a visible object, camera marker, or light marker in Scene to select it.
- Click an object in Elements to select it.
- Hold `Ctrl` while clicking to toggle objects in the multi-selection set.
- Drag an Elements row onto another object to reparent it.
- Drop an object onto the hierarchy root target to clear its parent.
- Parent changes reject self-parenting and cycles.
- Disabling a parent makes descendants inactive in hierarchy without changing each child's local active flag.

The Inspector edits the primary selected object. Operations that require a single source, such as copy, use that primary selection. Merge consumes compatible selected geometry and creates a combined mesh object.

## Creating objects

Use **+ Add object** in Elements or the Scene context menu. New objects appear in front of the editor camera.

Available object types:

- Empty object
- Cube
- Sphere
- Cylinder
- Capsule
- Plane
- Batch cubes
- Camera
- Light

Renderable primitives receive a material automatically. Cameras receive a Fly Controller. If a loaded or new scene has no authored camera, the editor creates `Main Camera`; if no authored camera is marked primary, the first one becomes primary.

Use **Batch cubes** when you need many test cubes at once. It creates the whole grid as one undoable edit, lets you choose count, columns, and spacing, and can add Colliders and/or Rigid Bodies automatically. The batch Rigid Body settings include motion type, friction, restitution, linear damping, angular damping, and gravity factor. The default Motion is Static for lower runtime cost; switch it to Dynamic when the cubes need to fall or move. Enabling Rigid Bodies also enables Colliders because physics bodies need collision shapes. Wider spacing avoids instant overlap, which keeps large dynamic batches from spending the first few frames separating interpenetrating cubes.

## Transform editing

Choose a gizmo operation from the viewport toolbar or keyboard:

| Shortcut | Operation |
|---|---|
| `W` | Translate |
| `E` | Rotate |
| `R` | Scale |

These shortcuts only apply while Scene is focused and no text field, popup, right-mouse navigation, or gizmo drag owns input. Position, rotation, and scale can also be edited numerically in Inspector. Rotation values are radians in the current production object model.

## Editing shortcuts

| Shortcut | Action |
|---|---|
| `Ctrl+S` | Save the current scene |
| `Ctrl+O` | Open a scene |
| `Ctrl+Z` | Undo |
| `Ctrl+Y` | Redo |
| `Ctrl+C` | Copy the primary selected object |
| `Ctrl+V` | Paste with an offset |
| `Ctrl+D` | Duplicate the primary selected object |
| `Delete` | Delete selected object |

Undo/redo uses full versioned scene snapshots. Transform drags and Inspector edits record snapshots at edit activation so a continuous drag behaves as one authoring operation. Saving or loading while simulation is active first stops Play Mode.

The editor marks unsaved state with an asterisk in the title bar. Loading or exiting with unsaved edits opens Save/Discard/Cancel confirmation.

## Inspector components

Every object has a Transform. Other component surfaces depend on object type.

### Camera

- Primary camera flag
- Field of view in radians
- Near and far clip planes
- Current projection viewport dimensions

The first enabled, active authored camera marked Primary drives Game. `Editor Camera` is reserved for Scene.

### Material and texture

Material modes are Lit/Tint, Rainbow Debug, Flat Red, Flat Green, and Flat Blue. Lit/Tint exposes RGBA albedo, emissive RGB, and emission strength.

Add a Texture component to enter a path or drag a supported texture from Asset Lens. The production renderer decodes textures through Windows Imaging Component and caches successful and failed path lookups. Disable the component to retain the path without sampling it.

### Rigid Body

- Body type: Static, Dynamic, or Kinematic
- Friction
- Restitution, also known as bounciness
- Linear and angular damping
- Gravity factor
- Enabled state

### Collider

- Shape: Box, Sphere, Cylinder, or Capsule
- Box half extents
- Radius for curved shapes
- Half height for cylinder and capsule

Physics requires both an enabled Rigid Body and a Collider on an active object. Collider shape sizes are multiplied by the object's world scale when Play Mode builds the physics scene. Box colliders on Plane objects are aligned so the collider's top face follows the visible plane surface. Dynamic bodies with a parent are skipped because runtime world-space synchronization is only supported for root dynamic bodies.

For high cube counts, prefer Collider-only or Static bodies when the cubes do not need to move. Dynamic bodies are the most expensive path because every active object is simulated. The physics world uses a capped worker-thread pool during Play Mode, and the Stats panel shows the current physics time, active body count, contact count, and worker count.

### Rotator

The angular-velocity vector advances object rotation during world updates while enabled.

### Fly Controller

Fly Controller controls a primary camera during captured Play Mode. It exposes move speed, mouse-look sensitivity, boost multiplier, pitch limit, and enabled state.

### Light

The light component supports Directional, Point, and Spot modes with color, intensity, ambient strength, range, spot angle, shadow area, and Cast Shadows. Up to 16 active lights are sent to the production shader. The first active shadow-casting light drives the shadow pass.

## Play Mode

| Input | Action |
|---|---|
| `F6` | Enter Play Mode or stop and return to Edit Mode |
| `F7` | Pause or resume |
| `F8` | Advance one fixed simulation step |
| `Escape` | Release captured Game input |

Entering Play Mode serializes the editor scene, reconstructs the runtime scene, resets physics, focuses Game, and captures the cursor. The simulation uses a 60 Hz fixed step, clamps a long frame contribution to 250 ms, and performs at most eight catch-up steps per rendered frame.

Stopping restores the pre-play snapshot, resets physics again, discards runtime mutations, clears selection, and returns focus to Scene. This means edits made while playing are intentionally temporary.

During captured Play Mode, the primary camera's enabled Fly Controller uses:

- mouse movement to look;
- `W`, `A`, `S`, `D` to move;
- `Q` to descend;
- `E` or `Space` to ascend; and
- `Shift` to apply its boost multiplier.

## Assets

Asset Lens scans `assets/` and `DX3D/Assets/` recursively. It hides scene, JSON, metadata, and operating-system housekeeping files. Recognized display categories include models, textures, materials, prefabs, and HLSL.

- Double-click an `.obj` entry in Edit Mode to import it at the Scene spawn point.
- Save the selected object subtree as a production `.eprefab` with **Edit > Save Selected as Prefab** or the Elements right-click menu.
- Double-click a production `.eprefab`, choose **Instantiate Prefab**, or drag it into Scene to instantiate it.
- Drag supported image files onto a Texture component.
- Use **Refresh** after adding or removing files outside the editor.

Assimp-backed broader model preparation and canonical readable prefab fixtures exist in the test/migration layer but are not yet connected to the production Asset Lens.

## Saving and loading

The production editor saves `.dx3dscene` files. Current saves are readable JSON envelopes containing a versioned hexadecimal binary payload; earlier raw binary files remain loadable. See [Scenes and assets](SCENES_AND_ASSETS.md) for compatibility details and limits.

# jnpf. API documentation

The maintained Markdown handbook explains behavior and workflows. Doxygen complements it with browsable declarations, inheritance, include relationships, and source references for engine-owned C++.

## Install

Install a recent Doxygen release and ensure `doxygen.exe` is on `PATH`. Graphviz is optional; the checked-in configuration does not require it.

Verify:

```powershell
doxygen --version
```

## Generate directly

From the repository root:

```powershell
doxygen Doxyfile
```

Open:

```text
docs/doxygen/html/index.html
```

Warnings are written to `docs/doxygen/warnings.log`. Generated output is ignored by Git.

## Generate through CMake

CMake creates the `docs` target only when Doxygen was found during configuration:

```powershell
cmake --preset x64-debug
cmake --build --preset x64-debug --target docs
```

If Ninja reports that the target does not exist, install Doxygen, confirm it is visible in the same terminal, and rerun the configure preset.

## Inputs

The Doxyfile includes:

- this handbook and root README;
- public and private engine-owned C++ under `DX3D`;
- canonical C++ under `engine`;
- application code under `Game`; and
- `docs/DoxygenGroups.dox` module descriptions.

Vendored dependencies, build output, generated HTML, precompiled-header files, and user/build directories are excluded.

## Writing API comments

Use `/** ... */` comments on public types and operations whose contract is not obvious. Prefer:

- a one-sentence `@brief`;
- ownership and lifetime rules;
- valid state and thread restrictions;
- units and coordinate conventions;
- `@param` only when the parameter role is not obvious;
- `@return` for meaningful success/failure semantics;
- `@note` for non-obvious behavior; and
- `@warning` for invalidation, raw pointers, deferred mutation, or expensive operations.

Assign major types to a group with `@ingroup`, using the group names declared in `DoxygenGroups.dox`.

Example:

```cpp
/**
 * @brief Queues an object subtree for destruction.
 * @param object Non-owning pointer to an object owned by this world.
 * @warning Destruction is deferred until the next world update.
 * @ingroup production_scene
 */
void destroyGameObject(GameObject* object);
```

## Configuration notes

`EXTRACT_ALL` is enabled because documentation coverage is still growing. This makes every declaration discoverable without pretending every declaration has a complete contract comment. `WARN_IF_UNDOCUMENTED` is disabled, while malformed documentation still emits warnings.

To enable Graphviz diagrams locally, set `HAVE_DOT = YES` in a personal Doxyfile copy or temporary worktree edit. Do not require Graphviz for the baseline documentation build unless the team adopts it as a documented prerequisite.

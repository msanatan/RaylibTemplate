# Working on this template

Keep code easy for humans and agents to read and modify. See [README.md](README.md) for the project layout, build and verification commands, scene operations, and playtest scripting.

## Game design and assets

- Use Tiled for 2D level authoring; it is already installed. Keep Raylib as the game runtime. Use a custom editor for 3D scene authoring, with Blender reserved for modeling.
- Ask the user open-ended questions about how the game should look, sound, and feel. Help them think through their design without steering them too far toward a particular solution. Keep questions focused and give them room to decide.
- Do not generate AI assets, including artwork, models, music, or sound effects. Placeholder images and simple geometric stand-ins are fine; do not use AI asset generation for placeholders either. The user will find or create game assets, or direct you to sources.

## Where code goes

- `src/core/` contains reusable template systems. Keep game-specific logic out.
- Screens belong in `src/scenes/`, game rules and entities in `src/game/`, and asset files in `assets/`.
- Prefer focused files and simple control flow. Add abstractions when a concrete need warrants them.

- `levels/` is the 3D level library, owned by neither the game nor the editor so the format cannot drift between them: `document/` (data model, JSON, validation), `runtime/` (loading and drawing with raylib), `editing/` (operations and commands, tools only). The game links `runtime/` and never `editing/`. What a level's custom properties mean to a game belongs in `src/game/`.

## Scenic editor code

- `editor/src/` is grouped by responsibility: `document/` (session, document, history, selection), `commands/` (live commands, log), `transport/` (local socket, CLI), `viewport/` (camera, picking, gizmo, rendering the view), `ui/` (panels, menus, dialogs, shortcuts), `app/` (frame loop, file watching, launching the game), `testing/` (scripted runs). Tests are in `editor/tests/`.
- Every change to the open level goes through `EditorSession`. Panels, shortcuts, and live commands call its methods and read `session.doc()`; they never mutate the document. Express edits as level operations so the UI and agents share validation and undo.
- A continuous edit (drag, colour pick) is a gesture with a named owner; only that owner can finish it, and it becomes one undo step.
- Use `doc().index()` for lookups, children, and world transforms. It is rebuilt after every change, so do not keep it, or pointers into the level, across an edit.
- State belongs to the object that uses it: panel state in the panel class, not in file-level variables.
- Keep the document, commands, and transport free of any window or UI dependency so they stay testable without one.

## Authoring 3D levels

Levels are JSON files under `assets/levels/`. Prefer `scenic-cli` (built with the editor preset, in the build directory's `bin/`) over hand-editing: it validates every change and never leaves a half-applied edit. Hand-edited files are fine too; run `validate` and `format` afterwards.

- Conventions: metres, +Y up, XZ ground, objects and cameras face local -Z. Rotation is Euler degrees `[x, y, z]`, applied as roll (Z), pitch (X), then yaw (Y). Transforms are relative to the parent. Primitives are centred on their origin, so a height-2 box rests on the ground at Y = 1. See `levels/document/level.h` and `level.schema.json`.
- Object kinds: `group`, `marker`, `box`, `sphere`, `cylinder`, `plane`, `camera`. Game meaning goes in an object's custom `properties`; reference another object with `{"$ref": "<object id>"}`. Geometry does not imply collision.
- Always name a target: `--file <level>` edits a file, `--editor <session-id>` edits a running Scenic's in-memory document, including unsaved changes. `scenic-cli sessions` lists running editors. A missing editor is an error; never fall back to the file yourself without deciding to.
- Discover before guessing: `commands` lists commands, `commands describe apply` returns every operation and its defaults.
- Inspect, then edit with the revision you saw: `inspect` returns `document.revision` (and `document.id` for an editor). Mutations need `--expect <revision>`, plus `--document <id>` with `--editor`. Exit code 3 means the document moved on: inspect again and reapply.
- Edit with one `apply` batch. Operations run in order and the result is validated once; any failure changes nothing. Refer to objects by exact ID. IDs are UUIDs and anything else is rejected: leave `id` out of `create` and `ids` out of `duplicate` so they are generated, then read them from `generated_ids` in the result. Supply your own UUID only when a later operation in the same batch must reference the new object. Check `ok`, `changed`, and `errors` in the JSON result. Exit codes: 0 success, 2 invalid request or document, 3 revision conflict, 4 I/O or connection failure, 5 noncanonical file.
- A live `apply` is undoable and unsaved. `save` writes the file; the running game reloads saved files only. `screenshot`, `frame`, `camera preview`, `logs`, and `state` let you look at a running editor.

```sh
scenic-cli --file assets/levels/main.json inspect
scenic-cli --file assets/levels/main.json apply --ops edits.json --expect <revision>
scenic-cli --file assets/levels/main.json validate
scenic-cli --file assets/levels/main.json format --check
```

```json
{"operations": [
  {"op": "create", "name": "Door", "kind": "box", "transform": {"position": [2, 1, 0]}, "shape": {"size": [1, 2, 0.2]}},
  {"op": "set-property", "id": "9d12790b-dbd4-48b4-9b77-785f500cee1a", "path": "/fov_boost", "value": true}
]}
```

## Documentation

- Code is the source of truth for behavior and implementation details. Read it rather than relying on prose that duplicates it.
- Keep the README a short guide to getting started, using the template, and finding the relevant code. Do not append implementation summaries, troubleshooting notes, or details of every change.
- Update the README when setup or usage changes. Link to the relevant source for details instead of restating APIs or workflow internals. Put change summaries and verification results in your response or PR description.

## Runtime rules

- Load assets through `core/assets/assets.h` in scene constructors and unload them in destructors. Pair `LoadGameMusic` with `UnloadGameMusic` so packed music buffers are released correctly.
- Use `.glb` for models and OGG for looping music.
- Query named input actions; add bindings in `src/game/bindings.cpp`. Query an action every update while its on-screen control should be active.
- Use the `deltaTime` passed to `update` for gameplay timing. Avoid wall-clock APIs such as `GetFrameTime()` and `GetTime()` in game logic so scripted runs remain repeatable.
- Change screens through `SceneManager`. The README describes stack operations; changes are applied after the current scene's update.
- Log through raylib's `TraceLog` with a `SUBSYSTEM:` prefix.

## Adding a scene

- Implement `name()`, `update()`, and `draw()`.
- Register the scene in `src/scenes/registry.cpp`.
- Expose useful state through `debugState()` for playtest assertions.
- Implement `saveState()` and `restoreState()` when progress should survive an asset reload or session restart. Saved scene state must fit on one line.

## Testing and completion

**Keep tests that protect foundational behavior, prevent a consequential regression, or demonstrate how to test a game feature.**

- Focus permanent tests on core systems, core game mechanics, and representative flows. Temporary development checks do not all need to become permanent tests.
- Prefer a small, readable suite. Avoid duplicated coverage and elaborate test infrastructure for isolated edge cases. Exhaustive and exploratory QA are separate work when requested.
- Use the README's commands to build and run checks relevant to the change. Playtests need a real window. Inspect screenshots when verifying visual changes.
- For level and editor changes, make the windowless `integration` tests the main check. Keep windowed `editor` scripts to behaviour that needs graphics or input.
- Follow the formatter rather than hand-formatting. Prefer clear names. Ideally no comments should be added, code should be easy enough to understand. However, for bespoke/quirky implementations, comments should explain why we're coding that way.
- Address warnings introduced by the change, keep formatting clean, and ensure relevant checks pass. Once they pass, stop unless new changes or failures warrant more verification. Report what was checked and any limitations.

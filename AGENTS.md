# Working on this template

Keep code easy for humans and agents to read and modify. See [README.md](README.md) for the project layout, build and verification commands, scene operations, and playtest scripting.

## Approach

- Edit game logic, environments, reusable engine systems, and the editor as needed to complete the task. Follow the engine's existing conventions and keep changes in the layer that owns the behavior.
- Change Raylib itself only when necessary and with explicit user permission.

## Game design and assets

- Use Tiled for 2D level authoring; it is already installed. Keep Raylib as the game runtime. Use Scenic for 3D level authoring, with Blender reserved for modeling.
- Ask the user open-ended questions about how the game should look, sound, and feel. Help them think through their design without steering them too far toward a particular solution. Keep questions focused and give them room to decide.
- Do not generate AI assets, including artwork, models, music, or sound effects. Placeholder images and simple geometric stand-ins are fine; do not use AI asset generation for placeholders either. The user will find or create game assets, or direct you to sources. User must give explicit permission to generate an AI asset.

## Where code goes

- `game/src/core/` contains reusable template systems. Keep game-specific logic out.
- Screens belong in `game/src/scenes/`, game rules and entities in `game/src/game/`, and asset files in `game/assets/`.
- Prefer focused files and simple control flow. Add abstractions when a concrete need warrants them.

- `levels/` is the 3D level library, owned by neither the game nor the editor so the format cannot drift between them: `document/` (data model, JSON, validation), `runtime/` (loading and drawing with raylib), `editing/` (operations and commands, tools only). The game links `runtime/` and never `editing/`. What a level's custom properties mean to a game belongs in `game/src/game/`.

## Authoring 3D levels

- Read level JSON directly under `game/assets/levels/` to understand the environment; write changes through `scenic-cli` for validation and atomic edits. See the [README](README.md#3d-levels) for CLI usage, `scenic-cli commands` to discover commands, and `scenic-cli commands describe <name>` for details.
- Follow the level conventions in [level.h](levels/document/level.h) and [level.schema.json](levels/document/level.schema.json). Put gameplay meaning in custom properties; geometry alone does not imply collision.
- Choose an explicit target: `--file <level>` for a saved file or `--editor <session-id>` for a running editor, including unsaved changes. Use `sessions` to find editors and `inspect` to read their current document; do not silently switch targets.
- Inspect before writing, then batch related edits with `apply --expect <revision>` (also `--document <id>` for an editor). On a revision conflict, inspect again before retrying. Check the result for errors.
- Save live editor changes when they should reach the game; the game reloads saved files only. Inspect screenshots when checking appearance.

## Documentation

- Code is the source of truth for behavior and implementation details. Read it rather than relying on prose that duplicates it.
- Keep the README a short guide to getting started, using the template, and finding the relevant code. Do not append implementation summaries, troubleshooting notes, or details of every change.
- Update the README when setup or usage changes. Link to the relevant source for details instead of restating APIs or workflow internals. Put change summaries and verification results in your response or PR description.

## Runtime rules

- Load assets through `core/assets/assets.h` in scene constructors and unload them in destructors. Pair `LoadGameMusic` with `UnloadGameMusic` so packed music buffers are released correctly.
- Use `.glb` for models and OGG for looping music.
- Query named input actions; add bindings in `game/src/game/bindings.cpp`. Query an action every update while its on-screen control should be active.
- Use the `deltaTime` passed to `update` for gameplay timing. Avoid wall-clock APIs such as `GetFrameTime()` and `GetTime()` in game logic so scripted runs remain repeatable.
- Change screens through `SceneManager`. The README describes stack operations; changes are applied after the current scene's update.
- Log through raylib's `TraceLog` with a `SUBSYSTEM:` prefix.

## Adding a scene

- Implement `name()`, `update()`, and `draw()`.
- Register the scene in `game/src/scenes/registry.cpp`.
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

# Architecture & Design Notes

Living document. Claude reads this at the start of every session and updates it whenever the
code changes. If something here disagrees with the code, the code wins — fix this file.

- **Project:** `TheLatticeTestCopy.uproject`, module `TheLatticeTestCopy` (API macro `THELATTICETESTCOPY_API`),
  at `C:\Users\gabet\Desktop\UnrealProjects\TheLatticeTestCopy`. Git repo, branch `main`.
- **Engine:** Unreal Engine 5.6, C++ first. Blueprints only for asset references and content.
- **Game:** narrative first-person movement shooter about grief, set in a cyber-gothic brutalist
  megastructure (BLAME!-inspired). Gameplay splits into **puzzle/raid arenas** and **traversal**.
- **Current milestone:** proof of concept — showcase the art. Systems in scope: the first-person
  controller and the VTT. Everything else is roadmap.

---

## 1. Source layout

```
Source/TheLatticeTestCopy/
  TheLatticeTestCopy.Build.cs          module dependencies
  Core/
    MegaGameMode             spawns MegaCharacter + MegaPlayerController
  Player/
    MegaCharacter            input, camera feel, owns the movement component
    MegaCharacterMovementComponent   dash / slide / slam / wall jump / coyote time
    MegaPlayerController     creates the VTT overlay (the only HUD)
    MegaDefaultInput         code-built Enhanced Input fallback (no assets required)
  VTT/
    VTTTypes                 FVTTEntry, FVTTRequestParams, FVTTActiveCallout
    VTTSettings              Project Settings > Game > VTT (all tuning + style)
    VTTComponent             put on any actor to give it VTT text
    VTTSubsystem             registry, ambient + scripted triggering, callout lifetime
    VTTOverlayWidget         draws callouts (frame, leader line, typed/scrambled text)
    VTTTriggerVolume         level-placed scripted trigger
```

Planned folders (not created yet): `Combat/` (weapons, damage), `AI/` (enemies), `Arena/`
(raid encounter logic: waves, switches, buffs), `World/` (traversal set pieces, streaming).

**Naming:** `Mega` prefix for game-framework classes, `VTT` prefix for the VTT system. One
class per file pair, folder = feature. Includes are module-root relative, e.g.
`#include "VTT/VTTComponent.h"`.

---

## 2. Player

### Class responsibilities

| Class | Owns | Does not own |
|---|---|---|
| `AMegaCharacter` | Input binding, look, jump buffering, camera (eye height, FOV, roll, landing dip), first-person arms mesh | Movement rules |
| `UMegaCharacterMovementComponent` | All movement rules and state; movement events | Input, camera |
| `AMegaPlayerController` | HUD/UI lifetime (VTT overlay), input mode | Gameplay |

### Movement model

Dash, slide and slam are **states on top of `MOVE_Walking` / `MOVE_Falling`**, not custom
movement modes. The component only overrides `CalcVelocity` (velocity while dashing/sliding)
and `GetGravityZ` (0 while dashing). The engine keeps doing floor detection, step-up, slopes
and ledges. This is why there is no `PhysCustom`.

| Action | Input | Behaviour |
|---|---|---|
| Move | WASD / left stick | Fast accel/decel, air momentum preserved |
| Jump | Space / A | Ground, coyote (0.12s), wall (3 per airtime), slide-jump, dash-jump, slam-jump; buffered 0.12s |
| Dash | Shift / LB | 3 stamina charges, horizontal only, gravity off for 0.18s |
| Slide | Ctrl / B (ground, hold) | Constant speed, light steering, capsule crouch |
| Slam | Ctrl / B (air, press) | Straight down; jump within 0.25s of landing = slam jump scaled by fall height |
| Wall cling | Push into wall while falling | Caps fall speed |

Events for feedback hookup: `OnMegaJumped(type)`, `OnMegaDashed(dir)`,
`OnMegaSlideChanged(bool)`, `OnMegaSlamLanded(location, fallDistance)`.

All numbers are `EditAnywhere` under `Mega|*` categories on the movement component and
`Camera`/`Input` on the character. Tune them in a child Blueprint (`BP_Player`) or in the
C++ defaults.

### Input

Enhanced Input. If the character's input slots are empty, `FMegaDefaultInput` builds transient
actions + a mapping context at runtime, so the game runs with zero assets. To enable rebinding
later, author `IA_*`/`IMC_*` assets and assign them on `BP_Player`; any assigned slot wins.

Look is applied directly to the control rotation (`LookSensitivity` degrees per raw unit) so it
doesn't depend on the project's legacy input-scale settings.

---

## 3. VTT (Virtual Text Translator)

The only HUD. A bracketed frame is drawn around a world object, a leader line runs out of it,
and text is "translated" in (typed out with scrambled glyphs running ahead of the cursor).

### Data flow

```
UVTTComponent (on actors) ──register──▶ UVTTSubsystem ◀──read each frame── UVTTOverlayWidget
        ▲                                   │                                   │
  ShowVTT(Id) / AVTTTriggerVolume ──request─┘   ambient timer picks             projects bounds,
                                                visible candidates               draws frame/line/text
```

- **`UVTTComponent`** (a hidden, collision-less box component): list of `FVTTEntry`
  (Id, Label, Body, ambient flags, weight, hold override), bounds mode, max distance, line-of-sight
  requirement, ambient cooldown/weight.
  - `OwnerBounds` (default): frames the owner's visible geometry, which follows animated enemies.
  - `CustomBox`: frames the component's box. Place and scale it to frame a specific detail.
- **`UVTTSubsystem`** (`UTickableWorldSubsystem`, game/PIE worlds only):
  - *Perceivable* = enabled, within range (nearest point of bounds), center projects inside the
    screen minus the edge margin, and the center or upper body is not occluded (if LOS required).
  - *Ambient:* every 4s ± 2s, roll 50%; if a slot is free and the 10s global cooldown has passed,
    pick a perceivable component (weighted toward screen center), then an entry (weighted toward unseen).
  - *Scripted:* `RequestCallout(Component, EntryId, Params)` queues a request; by default it waits
    until the target is perceivable (20s timeout), FIFO, optionally interrupting the oldest callout.
  - *Lifetime:* frame intro → leader line → typing → hold (auto from text length) → outro.
    Dismissed early if the target stays unperceived for 0.75s or is destroyed.
- **`UVTTOverlayWidget`**: pure C++ `UUserWidget`. `NativeTick` builds a draw list, `NativePaint`
  emits Slate lines/boxes/text. Text side (left/right, up/down) is chosen once per callout so it
  never flips mid-read; the text block is kept on screen.
- **`UVTTSettings`** (Project Settings → Game → VTT): every timing, color, font, size and sound.

### Authoring a VTT object (no code)

1. Add the **VTT** component to an actor (or to the enemy Blueprint).
2. Add entries: `Label` (short header), `Body` (the text), optional `Id` for scripted use.
3. Optional: switch to `CustomBox` and fit the box, or set `MaxDistance`.

Scripted moments: place an **VTT Trigger Volume**, set `Target Actor` + `Entry Id`.
From C++: `VTTComponent->ShowVTT("SomeId")` or `UVTTSubsystem::Get(this)->RequestCallout(...)`.

### Debugging

- `vtt.Debug 1` draws every registered frame (green = perceivable, red = not).
- `vtt.ForceAmbient` shows an ambient callout immediately.

---

## 4. Editor setup (one time)

1. World Settings → GameMode Override → `MegaGameMode` (or set it in Project Settings → Maps & Modes).
2. Optional `BP_Player` (child of `MegaCharacter`) for the arms mesh and tuning; set it as the
   game mode's Default Pawn via a `BP_GameMode` child if you make one.
3. Project Settings → Game → VTT for the look.

---

## 5. Known limitations / decisions

- **Single-player only.** Movement states are not network-predicted (no `FSavedMove`). That was
  deliberate for speed of iteration; multiplayer would need a movement rewrite.
- The VTT uses the first local player controller. Split-screen would need a per-player subsystem.
- Text is plain (no rich-text markup). Line breaks in `Body` are respected; words wrap automatically.
- Dash is purely horizontal by design (Ultrakill-style).
- UE 5.6 has first-person rendering features (separate FOV/scale for arms). Not used yet; adopt
  them when weapons arrive.

---

## 6. Roadmap

- [ ] Compile + feel pass in the test level (movement numbers are first-pass guesses)
- [ ] Movement feedback: camera shake, audio, VFX hooked to movement events
- [ ] VTT: optional per-entry voice/sound, rich-text emphasis, priority tiers
- [ ] Combat: weapon base class, hitscan + projectile, damage
- [ ] Enemies: base enemy with VTT component, simple AI (StateTree)
- [ ] Arena: wave director, switches/items/buffs, puzzle state machine
- [ ] Traversal: level streaming for scale, vista set pieces

---

## 7. Change log

- **2026-10-10** — Initial player controller (movement, camera, default input), game mode,
  and the full VTT system (component, subsystem, overlay, settings, trigger volume).

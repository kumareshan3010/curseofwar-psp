# Curse of War — PSP / PPSSPP port

A clean PSP homebrew frontend for the original Curse of War strategy game.

## Build

The GitHub Actions workflow builds EBOOT.PBP inside the PSPDEV toolchain container.

1. Open **Actions** in this repository.
2. Select **Build Curse of War for PSP**.
3. Run the workflow, or push a commit to trigger it.
4. Download the `curseofwar-psp` artifact from the completed run.

The artifact contains `CurseOfWar/EBOOT.PBP` and the `CurseOfWar/images/` asset folder.

## Install in PPSSPP

Copy the complete `CurseOfWar` folder to `PSP/GAME/` in your PPSSPP memory stick directory. Launch **Curse of War** from Games.

## Controls

| PSP control | Game action |
|---|---|
| D-pad / analog | Move cursor |
| Cross | Place/remove flag |
| Circle | Build |
| Square | Remove all flags |
| Triangle | Pause/resume |
| L trigger | Slower |
| R trigger | Faster |
| Select | Remove half of flags |
| Start | Quit |

## Notes

- This is an early port. The first CI build is a toolchain integration check; fix any compiler/linker errors reported by Actions before treating the artifact as playable.
- The game uses a compact 14×14 map and scales the original graphical renderer to the PSP's 480×272 display.
- Multiplayer is not included in this PSP frontend.
- Original game logic and artwork are from Alexey Nikolaev's Curse of War, licensed under GPL-3.0-or-later. See `LICENSE`.

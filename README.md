# PKStorage (PSP)

Move Pokémon between a **Pokémon LeafGreen/FireRed `.sav`** on your PSP memory stick and a
separate **ROM storage** (`PKSTORAGE.bin`) using a dual-grid visual interface with 32×32 sprites.

Runs natively on the PSP (custom firmware), not inside gpSP.

## Features
- Dual 5×6 grid view (left = LeafGreen box, right = Storage box)
- 32×32 Gen 3-style sprites (placeholder colours until atlas is added)
- Pick-up / put-down movement (Square to pick, move cursor, Cross to place)
- Classic action menu (Withdraw / Store / Deposit / Copy / Drop)
- Safe writes: backups, verified write-back, checksum protection

## Controls

| Button | Action |
|--------|--------|
| D-pad  | Move cursor / switch pane at edges |
| L / R  | Previous / next box |
| **Square** | Pick up Pokémon |
| **Cross**  | Place held Pokémon **or** open action menu |
| **Circle** | Cancel hold / cancel menu |
| **Triangle** | Save changes |
| **Start** | Quit (prompts if unsaved) |

## Install
1. Build with GitHub Actions (artifact **PKStorage-psp**) or `make` with pspdev.
2. Copy the `PKStorage` folder (contains `EBOOT.PBP`) to `ms0:/PSP/GAME/`.
3. `PKSTORAGE.bin` is created next to the EBOOT the first time you save.

## Safety
- Edits stay in memory until you press Triangle.
- Before the first write each session a backup is made (`name.sav.bak`, `.bak2`…).
- Every write is read back and compared.
- The side that receives a Pokémon is written first.
- Corrupt `PKSTORAGE.bin` is refused, never overwritten.

## Sprites
Currently using coloured placeholders with the national dex number.
To add real 32×32 Gen 3 icons:
1. Prepare a sprite atlas (or individual files) mapped by national dex number.
2. Load it in `plat_psp.c` → `plat_sprite()`.
3. The drawing code is already in place.

## Layout
- `gen3.*` – Gen 3 save parsing
- `store.*` / `fileio.*` – storage file + verified writes
- `app.c` – grid UI + logic
- `plat_psp.c` – GU graphics, input, memory stick
- `plat_pc.c` – minimal PC stub for testing core logic

Tests: `make -f Makefile.pc test`

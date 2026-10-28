# PKStorage (PSP)

Move Pokémon between a **Pokémon LeafGreen/FireRed `.sav`** on your PSP memory stick and a
separate **ROM storage** (`PKSTORAGE.bin`) with a simple menu. Runs natively on the PSP
(custom firmware), not inside gpSP – gpSP can't see the SD/Memory Stick, so a GBA ROM can't do this.

## Install
1. Build (GitHub Actions → artifact **PKStorage-psp**) or `make` with PSPSDK/pspdev.
2. Copy the `PKStorage` folder (contains `EBOOT.PBP`) to `ms0:/PSP/GAME/`.
3. Keep your LeafGreen save on the card; it's found automatically (see below).
   `PKSTORAGE.bin` is created next to the EBOOT the first time you save.

## Use
Start it from the Game menu. It scans the whole memory stick for `.sav` files (≥128 KB) whose name
contains **Leaf** (SQ = show every .sav). Pick one, then:

| Button | Action |
|---|---|
| D-pad ◀ ▶ | switch pane (left = Leaf Green save, right = ROM storage) |
| D-pad ▲ ▼ | move cursor · **L / R** = previous / next box |
| **X** | open menu on the highlighted Pokémon |
| **□** | quick-move to the other side |
| **△** | write changes to the card |
| **START** | quit (asks if there are unsaved changes) |

Menu items: **Withdraw** (save → ROM storage, move) · **Store** (save → ROM storage, copy) ·
**Deposit** (ROM storage → save, move) · **Copy** (ROM storage → save, copy) · **Drop** (release).
The destination is the first empty slot, starting at the box currently shown on that side.

## Safety
- Edits stay in memory until you press △.
- Before the first write each session, a backup is made next to the file: `name.sav.bak`, `.bak2`, … (never overwritten).
- Every write is read back and compared. The side that *receives* a Pokémon is written first.
- A save is only touched if both slot sections' checksums validate; otherwise nothing is written.
- A corrupt `PKSTORAGE.bin` is refused, never overwritten.
- **Close gpSP first**, and use the in-game save (not a gpSP save state – states restore old RAM).
- Pokémon added this way aren't registered in the Pokédex by the game; held items from other games can glitch FRLG.

## Layout
- `gen3.*` Gen 3 save parsing (14 sections ×2 slots, newest slot, checksums, PC boxes, 80-byte Pokémon decode)
- `src/store.*`, `src/fileio.*` storage file + verified writes + backups
- `src/app.c` menu/UI (platform independent) · `src/plat.h` platform interface
- `src/plat_psp.c` PSP screen/input/memory-stick · `src/plat_pc.c` keyboard build for testing
- `tests/` unit tests, synthetic save generator, scripted end-to-end tests

Tests: `make -f Makefile.pc test` (keys in the PC build: w/s/a/d, z=X, x=O, t=△, q=□, l/r, p=START).

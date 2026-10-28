#!/bin/sh
# Drives the real menu code (PC keyboard build) with scripted keys against a fake Leaf Green save.
# keys: z=X  x=O  t=TRIANGLE  q=SQUARE  p=START  w/s/a/d=dpad
set -e
cd "$(dirname "$0")/build"
rm -rf e2e && mkdir -p e2e/PSP/GAME/gpSP
SAV=e2e/PSP/GAME/gpSP/PokemonLeafGreen.sav
./mkfake $SAV
./mkfake e2e/decoy.sav                 # 128KB .sav without 'Leaf' in the name: must not be listed first
printf 'tiny' > e2e/PSP/GAME/gpSP/Leaf_small.sav   # too small: must be ignored
cp $SAV e2e/orig.sav
export PK_ROOT=e2e PK_STORAGE=e2e/PKSTORAGE.bin
run() { printf "$1" | ./pkstorage_pc > "e2e/out_$2.txt" 2>&1; }

# 1. scan finds the save, Withdraw Bulbasaur (save -> ROM), save, quit
run 'zzztp' 1
grep -q 'PokemonLeafGreen.sav' e2e/out_1.txt
! grep -q 'Leaf_small' e2e/out_1.txt
! grep -q 'decoy.sav' e2e/out_1.txt
grep -q 'Withdrew Bulbasaur' e2e/out_1.txt
./checkfile withdrawn $SAV e2e/PKSTORAGE.bin
cmp -s $SAV e2e/orig.sav && { echo "save unchanged after withdraw?!"; exit 1; }
cmp e2e/orig.sav $SAV.bak               # backup is the untouched original

# 2. Deposit it back (right pane, Deposit) -> save must be byte-identical to the original
run 'zdzztp' 2
grep -q 'Deposited Bulbasaur' e2e/out_2.txt
cmp e2e/orig.sav $SAV
./checkfile storage-empty $SAV e2e/PKSTORAGE.bin
test -f $SAV.bak2                       # second session made a second backup, never overwrote .bak

# 3. Store (copy) keeps the original in the save
run 'zzsztp' 3
grep -q 'Stored Bulbasaur' e2e/out_3.txt
./checkfile stored $SAV e2e/PKSTORAGE.bin

# 4. Drop the copy from ROM storage (right pane, Drop, confirm yes)
run 'zdzsszztp' 4
grep -q 'Released Bulbasaur' e2e/out_4.txt
./checkfile storage-empty $SAV e2e/PKSTORAGE.bin

# 5. Unsaved changes + START + 'O' (discard): files must not change
cp $SAV e2e/before.sav; cp e2e/PKSTORAGE.bin e2e/before.bin
run 'zzzpx' 5
grep -q 'Unsaved changes' e2e/out_5.txt
cmp $SAV e2e/before.sav; cmp e2e/PKSTORAGE.bin e2e/before.bin

# 6. Cancel on the Drop confirmation keeps the Pokemon; the screen says Cancelled
run 'zzsszxp' 6                           # menu, down x2 = Drop, answer O (no)
grep -q 'Cancelled' e2e/out_6.txt
cmp $SAV e2e/before.sav

# 7. Input ends abruptly (EOF) with unsaved edits: nothing is written
run 'zzz' 7
cmp $SAV e2e/before.sav; cmp e2e/PKSTORAGE.bin e2e/before.bin

# 8. Corrupt storage file is refused and left untouched
printf 'garbage' > e2e/PKSTORAGE.bin
cp e2e/PKSTORAGE.bin e2e/garbage.bin
run 'zzztp' 8
grep -q 'Storage file problem' e2e/out_8.txt
cmp e2e/PKSTORAGE.bin e2e/garbage.bin
cmp $SAV e2e/before.sav
echo "E2E: all scenarios passed"

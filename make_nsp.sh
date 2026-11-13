#!/bin/zsh
# Empaqueta el port como NSP instalable: ./make_nsp.sh /ruta/prod.keys
set -e
cd "$(dirname "$0")"
KEYS="${1:-$HOME/.switch/prod.keys}"
[ -f "$KEYS" ] || { echo "faltan las claves: $KEYS"; exit 1; }
export DEVKITPRO=/opt/devkitpro PATH=/opt/devkitpro/tools/bin:/opt/devkitpro/devkitA64/bin:$PATH
make CFLAGS_EXTRA=-DNSP_BUILD >/dev/null
elf2nso tetris-port.elf nsp/exefs/main
npdmtool nsp/npdm.json nsp/exefs/main.npdm
rm -rf nsp/out nsp/tmp nsp/nca nsp/backup
./tools/hacbrewpack -k "$KEYS" --titleid 0100545055444501 --titlename "Tetris Party Deluxe" --titlepublisher "Papa Huevo Cuchi" \
  --exefsdir nsp/exefs --romfsdir romfs --controldir nsp/control --nologo \
  --nspdir nsp/out --ncadir nsp/nca --tempdir nsp/tmp --backupdir nsp/backup
ls -la nsp/out

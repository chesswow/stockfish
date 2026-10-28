#!/bin/sh
# Downloads the Stockfish NNUE network into net/ and checks its SHA-256 prefix,
# which is part of the file name (nn-<sha256[0:12]>.nnue).
set -e
cd "$(dirname "$0")"
NAME=$(grep -o 'nn-[0-9a-f]*\.nnue' src/evaluate.h | head -1)
EXPECTED=$(echo "$NAME" | sed -n 's/^nn-\([0-9a-f]\{12\}\)\.nnue$/\1/p')
[ -n "$EXPECTED" ] || { echo "Unexpected network name: $NAME" >&2; exit 1; }
mkdir -p net
DEST="net/$NAME"
if [ -s "$DEST" ] && [ "$(shasum -a 256 "$DEST" | cut -c1-12)" = "$EXPECTED" ]; then
  echo "Network present and verified: $NAME"; exit 0
fi
curl -fL --retry 3 -o "$DEST.part" "https://tests.stockfishchess.org/api/nn/$NAME"
[ "$(shasum -a 256 "$DEST.part" | cut -c1-12)" = "$EXPECTED" ] || { rm -f "$DEST.part"; echo "Checksum mismatch" >&2; exit 1; }
mv "$DEST.part" "$DEST"
echo "Done: $DEST"

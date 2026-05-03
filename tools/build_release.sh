#!/usr/bin/env bash
# Stage everything a Glide install needs into dist/glide-<os>-<arch>-<ver>/
# and pack it up. Layout:
#
#   glide/
#     glide(.exe)
#     stdlib/
#     runtime/zig/...
#     README.md
#     LICENSE
#
# Assumes:
#   - tools/install_zig.sh has already been run (runtime/zig/ exists)
#   - A working `glide` binary exists at ./glide(.exe), built via the
#     bootstrap from bootstrap/seed/bootstrap.c.

set -e

VERSION="${VERSION:-0.1.0}"

case "$(uname -s)" in
    Linux*)              OS=linux ;;
    Darwin*)             OS=macos ;;
    CYGWIN*|MINGW*|MSYS*) OS=windows ;;
    *) echo "unsupported OS: $(uname -s)" >&2; exit 1 ;;
esac
case "$(uname -m)" in
    x86_64|amd64) ARCH=x86_64 ;;
    aarch64|arm64) ARCH=aarch64 ;;
    *) echo "unsupported arch: $(uname -m)" >&2; exit 1 ;;
esac

if [ "$OS" = "windows" ]; then
    BIN="glide.exe"
    ARCHIVE_EXT="zip"
else
    BIN="glide"
    ARCHIVE_EXT="tar.gz"
fi

NAME="glide-${OS}-${ARCH}-${VERSION}"
STAGE="dist/${NAME}"

if [ ! -x "$BIN" ]; then
    echo "no $BIN found in repo root. Build it first:" >&2
    echo "  cc bootstrap/seed/bootstrap.c -o glide_seed -O2 -lpthread -lm" >&2
    echo "  ./glide_seed build bootstrap/main.glide -o $BIN" >&2
    exit 1
fi
if [ ! -d runtime/zig ]; then
    echo "no runtime/zig/ found. Run tools/install_zig.sh first." >&2
    exit 1
fi

echo ">> Staging at $STAGE"
rm -rf "$STAGE"
mkdir -p "$STAGE"
cp "$BIN" "$STAGE/"
cp -r stdlib "$STAGE/"
cp -r runtime "$STAGE/"
cp README.md "$STAGE/" 2>/dev/null || true
cp LICENSE "$STAGE/" 2>/dev/null || true

echo ">> Archiving"
case "$ARCHIVE_EXT" in
    zip)
        ( cd dist && rm -f "${NAME}.zip" && \
          if command -v zip >/dev/null 2>&1; then \
              zip -qr "${NAME}.zip" "$NAME"; \
          else \
              powershell -NoProfile -Command "Compress-Archive -Path '$NAME' -DestinationPath '${NAME}.zip' -Force"; \
          fi )
        echo ">> dist/${NAME}.zip"
        ;;
    tar.gz)
        ( cd dist && tar -czf "${NAME}.tar.gz" "$NAME" )
        echo ">> dist/${NAME}.tar.gz"
        ;;
esac

# Report final size
SIZE=$(du -sh "dist/${NAME}.${ARCHIVE_EXT}" | cut -f1)
echo ">> Done: dist/${NAME}.${ARCHIVE_EXT} ($SIZE)"

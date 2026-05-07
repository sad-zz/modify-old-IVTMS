#!/usr/bin/env bash
# ─────────────────────────────────────────────────────────────────────
# setup_after_cubemx.sh
#
# این اسکریپت بعد از اجرای "Generate Code" در STM32CubeIDE/CubeMX اجرا
# می‌شود تا فایل‌های پیش‌فرض CubeMX را با نسخه‌های اپلیکیشن RATCX1
# جایگزین کند.
#
# روال استفاده:
#   ۱) STM32CubeIDE → File → Open Projects from File System → پوشه‌ی
#      stm32-firmware را انتخاب کنید
#   ۲) RATCX1-STM32.ioc را باز کنید → Generate Code
#   ۳) (لینوکس/مک/WSL) از ترمینال در داخل پوشه‌ی پروژه:
#         bash setup_after_cubemx.sh
#      (ویندوز): setup_after_cubemx.bat را دو-کلیک کنید
#   ۴) در STM32CubeIDE: راست‌کلیک پروژه → Refresh (F5)
#   ۵) Build (Ctrl+B) → Run (Ctrl+F11)
# ─────────────────────────────────────────────────────────────────────

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

echo "──────────────────────────────────────────────"
echo " RATCX1-STM32 – Post-CubeMX Setup"
echo " Working directory: $SCRIPT_DIR"
echo "──────────────────────────────────────────────"

# Sanity check: the .ioc generation must have already produced these
# scaffold files. If they don't exist the user hasn't run Generate Code yet.
if [ ! -f "Core/Src/system_stm32f1xx.c" ]; then
    echo ""
    echo "❌ ERROR: ساختار پروژه پیدا نشد."
    echo "   ابتدا در CubeIDE فایل RATCX1-STM32.ioc را باز کنید و"
    echo "   روی 'Generate Code' کلیک کنید، سپس این اسکریپت را اجرا کنید."
    echo ""
    exit 1
fi

# ─── Backup originals (one-time) ─────────────────────────────────────
mkdir -p .cubemx_backup
backup() {
    if [ -f "$1" ] && [ ! -f ".cubemx_backup/$(basename "$1")" ]; then
        cp "$1" ".cubemx_backup/$(basename "$1")"
        echo "  📦 backed up $1"
    fi
}

echo ""
echo "Step 1: Backing up CubeMX-generated files..."
backup Core/Src/main.c
backup Core/Src/stm32f1xx_it.c
backup Core/Inc/main.h
backup Core/Inc/stm32f1xx_it.h

# ─── Application files are already in Core/Src and Core/Inc.
#     Only main.c and stm32f1xx_it.c need re-applying because CubeMX
#     just overwrote them. Our originals live in .ratcx1_originals/
#     which we populate now (idempotent). ───────────────────────────────
echo ""
echo "Step 2: Re-applying RATCX1 application files..."

# main.c, stm32f1xx_it.c, stm32f1xx_it.h, main.h were already replaced
# by us in this repository, but Generate Code overwrote them. To make
# this script idempotent, we keep golden copies in .ratcx1_originals/
# and restore from there each run.
GOLDEN=".ratcx1_originals"
if [ ! -d "$GOLDEN" ]; then
    mkdir -p "$GOLDEN/Core/Src" "$GOLDEN/Core/Inc"
    # First-run: assume current Core/ contains our originals (i.e., the
    # user runs this script BEFORE the first Generate Code).
    cp Core/Src/main.c          "$GOLDEN/Core/Src/"          2>/dev/null || true
    cp Core/Src/stm32f1xx_it.c  "$GOLDEN/Core/Src/"          2>/dev/null || true
    cp Core/Inc/main.h          "$GOLDEN/Core/Inc/"          2>/dev/null || true
    cp Core/Inc/stm32f1xx_it.h  "$GOLDEN/Core/Inc/"          2>/dev/null || true
    echo "  📋 saved golden copies to $GOLDEN/"
fi

# Restore from golden
cp "$GOLDEN/Core/Src/main.c"          Core/Src/main.c
cp "$GOLDEN/Core/Src/stm32f1xx_it.c"  Core/Src/stm32f1xx_it.c
cp "$GOLDEN/Core/Inc/main.h"          Core/Inc/main.h
cp "$GOLDEN/Core/Inc/stm32f1xx_it.h"  Core/Inc/stm32f1xx_it.h
echo "  ✅ Core/Src/main.c"
echo "  ✅ Core/Src/stm32f1xx_it.c"
echo "  ✅ Core/Inc/main.h"
echo "  ✅ Core/Inc/stm32f1xx_it.h"

echo ""
echo "Step 3: Verifying additional application files exist..."
for f in \
    Core/Inc/config.h \
    Core/Inc/variables.h \
    Core/Inc/loop_detector.h \
    Core/Inc/classification.h \
    Core/Inc/interval.h \
    Core/Inc/protocol.h \
    Core/Inc/air780_tcp.h \
    Core/Inc/storage.h \
    Core/Inc/w25q80.h \
    Core/Inc/sd_spi.h \
    Core/Src/loop_detector.c \
    Core/Src/classification.c \
    Core/Src/interval.c \
    Core/Src/protocol.c \
    Core/Src/air780_tcp.c \
    Core/Src/storage.c \
    Core/Src/w25q80.c \
    Core/Src/sd_spi.c
do
    if [ ! -f "$f" ]; then
        echo "  ❌ MISSING: $f"
        exit 1
    fi
done
echo "  ✅ all application sources present"

echo ""
echo "──────────────────────────────────────────────"
echo " ✅ Setup complete!"
echo ""
echo " Next steps:"
echo "   1) در CubeIDE: راست‌کلیک پروژه → Refresh (F5)"
echo "   2) Build (Ctrl+B)"
echo "   3) برد را با ST-Link وصل کنید → Run (Ctrl+F11)"
echo "──────────────────────────────────────────────"

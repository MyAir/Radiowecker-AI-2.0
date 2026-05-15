"""
patch_lvgl.py — PlatformIO extra_script (pre:)

LVGL 9.x ships ARM-only assembly files (Helium and NEON) that include
<stdint.h> unconditionally, causing the Xtensa assembler to fail with
"unknown opcode or format name 'typedef'".

This script is loaded via  extra_scripts = pre:scripts/patch_lvgl.py
Running as a 'pre' script means SCons runs it before setting up any build
targets, so both files are stubbed out before the compiler sees them.

The patch is idempotent.
"""

import os
Import("env")  # noqa: F821  (SCons magic)

ARM_ASM_STUBS = {
    os.path.join("draw", "sw", "blend", "helium", "lv_blend_helium.S"):
        "/* empty stub: Helium assembly not supported on Xtensa/ESP32-S3 */\n",
    os.path.join("draw", "sw", "blend", "neon", "lv_blend_neon.S"):
        "/* empty stub: NEON assembly not supported on Xtensa/ESP32-S3 */\n",
}

lvgl_src = os.path.join(
    env.subst("$PROJECT_DIR"),
    ".pio", "libdeps",
    env.subst("$PIOENV"),
    "lvgl", "src",
)

for rel_path, stub in ARM_ASM_STUBS.items():
    full_path = os.path.join(lvgl_src, rel_path)
    if not os.path.isfile(full_path):
        continue  # library not downloaded yet — nothing to patch
    with open(full_path, "r") as fh:
        current = fh.read()
    if current.strip() == stub.strip():
        continue  # already patched
    print(f"patch_lvgl.py: patching {os.path.basename(full_path)} for Xtensa compatibility")
    with open(full_path, "w") as fh:
        fh.write(stub)



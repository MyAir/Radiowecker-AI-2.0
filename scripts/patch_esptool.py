"""
patch_esptool.py — PlatformIO extra_script (pre:)

esptool 5.0.0 calls click.ParamType.get_metavar(param) with one argument,
but click >= 8.2 changed the signature to require two arguments (param, ctx).
This script patches the installed cli_util.py to handle both signatures.

The patch is idempotent — safe to run on machines with any click version.
"""

import os
Import("env")  # noqa: F821  (SCons magic)

ESPTOOL_CLI_UTIL = os.path.join(
    os.path.expanduser("~"),
    ".platformio", "packages", "tool-esptoolpy", "esptool", "cli_util.py",
)

OLD_LINE = '            self.metavar = f"[{self.type.get_metavar(None) or self.type.name.upper()}]"'
NEW_LINES = '''\
            try:
                _gm = self.type.get_metavar(None, None)  # click >= 8.2
            except TypeError:
                _gm = self.type.get_metavar(None)        # click < 8.2
            self.metavar = f"[{_gm or self.type.name.upper()}]"'''

if not os.path.isfile(ESPTOOL_CLI_UTIL):
    print(f"patch_esptool.py: {ESPTOOL_CLI_UTIL} not found — skipping")
else:
    with open(ESPTOOL_CLI_UTIL, "r", encoding="utf-8") as fh:
        src = fh.read()

    if OLD_LINE in src:
        print("patch_esptool.py: patching esptool cli_util.py for click >= 8.2 compatibility")
        with open(ESPTOOL_CLI_UTIL, "w", encoding="utf-8") as fh:
            fh.write(src.replace(OLD_LINE, NEW_LINES))
        # Invalidate any cached bytecode so Python uses the patched source
        pycache = os.path.join(os.path.dirname(ESPTOOL_CLI_UTIL), "__pycache__")
        if os.path.isdir(pycache):
            for fname in os.listdir(pycache):
                if fname.startswith("cli_util"):
                    os.remove(os.path.join(pycache, fname))
    else:
        print("patch_esptool.py: cli_util.py already patched or not matching — skipping")

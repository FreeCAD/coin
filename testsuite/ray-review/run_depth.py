"""Run the native probe with the supplied DLL isolated next to the executable."""
from pathlib import Path
import os
import shutil
import subprocess
import sys
from tempfile import TemporaryDirectory

exe, dll, mode = sys.argv[1:]
with TemporaryDirectory() as tmp:
    target = Path(tmp)
    probe = shutil.copy2(exe, target)
    module = shutil.copy2(dll, target / "Coin4.dll")
    output = target / "observations"
    env = dict(os.environ, COIN_CPU_RAY_ASYNC="1" if mode == "replacement" else "0")
    subprocess.run([probe, mode, output], env=env, check=True, timeout=15)
    if mode == "replacement":
        sys.exit(0)
    subprocess.run([sys.executable, "-I", "-B", Path(__file__).with_name("check_depth.py"),
                    output, module], check=True, timeout=10)

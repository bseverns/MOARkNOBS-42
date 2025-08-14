Import("env")
import os
import subprocess

fw = os.getenv("FW_VERSION", "0.0.0-dev")
try:
    git = subprocess.check_output(["git", "rev-parse", "--short", "HEAD"]).strip().decode()
except Exception:
    git = "unknown"

env.Append(CPPDEFINES=[("FW_VERSION", f'"{fw}"'), ("GIT_SHA", f'"{git}"')])

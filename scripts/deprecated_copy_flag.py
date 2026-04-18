from pathlib import Path

# Root invocations are blocked, but keep this shim so legacy references execute
# the canonical helper under `firmware/scripts/`.
helper = Path(__file__).resolve().parents[1] / "firmware" / "scripts" / "deprecated_copy_flag.py"
code = helper.read_text(encoding="utf-8")
exec(compile(code, str(helper), "exec"), globals(), locals())

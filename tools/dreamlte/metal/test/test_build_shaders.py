import os
from pathlib import Path
import subprocess
import sys

BUILDER = Path(__file__).resolve().parents[1] / "build-shaders.py"
SCRIPT = """
import runpy
import sys

build = runpy.run_path(sys.argv[1])
sets = [{"zeta", "alpha"}, {"delta", "beta"}, {"alpha", "gamma"}]
slots = build["colour"](sets)
print(",".join(f"{name}={slot}" for name, slot in sorted(slots.items())))
"""


def colour_for_seed(seed):
    env = os.environ.copy()
    env["PYTHONHASHSEED"] = str(seed)
    return subprocess.check_output(
        [sys.executable, "-c", SCRIPT, str(BUILDER)],
        env=env,
        text=True,
    ).strip()


def main():
    outputs = [colour_for_seed(seed) for seed in (1, 2, 3)]
    if len(set(outputs)) != 1:
        raise SystemExit(f"varying slots changed across hash seeds: {outputs}")
    print(outputs[0])


if __name__ == "__main__":
    main()

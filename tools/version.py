# PlatformIO pre-build script: firmware_version.h with FIRMWARE_VERSION from the nearest v* tag, e.g.
# "v1.0.0", "v1.0.0-3-g725e442" between tags, "-dirty" with uncommitted changes. Rewritten only when
# the version changes, so only the files including it recompile.
import os
import subprocess

Import("env")

try:
    version = subprocess.run(["git", "describe", "--tags", "--match", "v[0-9]*", "--dirty", "--always"],
                             cwd=env["PROJECT_DIR"], capture_output=True, text=True).stdout.strip()
except OSError:
    version = ""
folder = os.path.join(env.subst("$BUILD_DIR"), "generated")
os.makedirs(folder, exist_ok=True)
path = os.path.join(folder, "firmware_version.h")
text = '#define FIRMWARE_VERSION "%s"\n' % (version or "unknown")
if not os.path.exists(path) or open(path).read() != text:
    with open(path, "w") as f:
        f.write(text)
env.Append(CPPPATH=[folder])

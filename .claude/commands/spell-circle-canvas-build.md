---
description: Configure and build the spell-circle-canvas Qt/C++ app
argument-hint: Optional flags, e.g. --config Release --force
---

# Build spell-circle-canvas

Run the setup script from the repo root, passing any user-supplied arguments directly:

```
python apps/spell-circle-canvas/scripts/setup.py $ARGUMENTS
```

If you are unsure what flags are available, run `python apps/spell-circle-canvas/scripts/setup.py --help` first and relay the output to the user.

## Diagnosing failures

Read the script's output — it prints the detected Qt and vcpkg paths before invoking CMake, which is usually enough to spot misconfiguration. For deeper failures:

- **configure** — read `apps/spell-circle-canvas/build/CMakeFiles/CMakeConfigureLog.yaml`
- **build** — the compiler error will name the file and line; source lives in `apps/spell-circle-canvas/src/`

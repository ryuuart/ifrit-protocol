#!/usr/bin/env python3
"""The build slot is scripts/buildslot.py; this file forwards every call there."""

import os
import sys
from pathlib import Path

target = Path(__file__).resolve().parents[1] / "scripts" / "buildslot.py"
os.execv(sys.executable, [sys.executable, str(target), *sys.argv[1:]])

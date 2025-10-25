"""
Waffle factory UI package.

This package currently provides static HTML, CSS, and JavaScript assets for the
control room skeleton served by ``tools/webdp8.py``.
"""

from __future__ import annotations

from pathlib import Path

# Expose helper paths so the web front-end can locate the assets without hard
# coding directory strings in multiple places.
PACKAGE_ROOT = Path(__file__).resolve().parent
TEMPLATES_DIR = PACKAGE_ROOT / "templates"
STATIC_DIR = PACKAGE_ROOT / "static"

__all__ = ["PACKAGE_ROOT", "STATIC_DIR", "TEMPLATES_DIR"]


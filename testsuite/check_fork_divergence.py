#!/usr/bin/env python3
"""Fail if renderer-selection divergence from upstream reappears in the fork.

Upstream Coin selects its renderer with COIN_BUILD_LEGACY_GL_RENDERER in a small,
fixed set of files.  This fork once carried that macro in ~220 source/CMake
files plus a private SO_ENABLE_LEGACY_GL indirection (SoActionGLP.h); that was
removed so the tree matches upstream again.  This check pins the invariant:

  1. COIN_BUILD_LEGACY_GL_RENDERER (and the fork-only *_VALUE variable) may
     appear only in the files where upstream declares/uses it.
  2. The private SO_ENABLE_LEGACY_GL macro and the SoActionGLP.h header must not
     exist at all — the legacy renderer is always built.

With --upstream REF it additionally asserts that headers upstream owns
byte-for-byte (gl.h, gl-headers.h.cmake.in, ...) have not been re-forked.

Usage:
    python3 testsuite/check_fork_divergence.py [--repo DIR] [--upstream REF]
"""
import argparse
import os
import subprocess
import sys

# Files in which upstream itself references COIN_BUILD_LEGACY_GL_RENDERER.
ALLOWED = {
    ".github/workflows/continuous-integration-workflow.yml",
    "CMakeLists.txt",
    "include/Inventor/system/gl-headers.h.cmake.in",
    "src/CMakeLists.txt",
    "src/elements/GL/CMakeLists.txt",
}

# Fork-only artifacts that must not come back.
FORBIDDEN_TOKENS = (
    "SO_ENABLE_LEGACY_GL",
    "SoActionGLP",
    "COIN_BUILD_LEGACY_GL_RENDERER_VALUE",
)

MACRO = "COIN_BUILD_LEGACY_GL_RENDERER"

# Files that legitimately name these tokens: the lint tool itself.
IGNORE = {
    "testsuite/check_fork_divergence.py",
}

# Files that upstream owns and that the fork must keep byte-identical.
PARITY_FILES = (
    "include/Inventor/system/gl.h",
    "include/Inventor/system/gl-headers.h.cmake.in",
    "include/Inventor/C/basic.h.in",
    "src/elements/GL/CMakeLists.txt",
)


def grep(repo, token):
    """Tracked files containing `token` (working tree)."""
    proc = subprocess.run(
        ["git", "-C", repo, "grep", "-l", token, "--", "."],
        capture_output=True, text=True,
    )
    return [line for line in proc.stdout.splitlines() if line and line not in IGNORE]


def ref_exists(repo, ref):
    return subprocess.run(
        ["git", "-C", repo, "rev-parse", "--verify", "--quiet", ref + "^{commit}"],
        capture_output=True, text=True,
    ).returncode == 0


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo", default=".")
    parser.add_argument("--upstream", default=None)
    args = parser.parse_args()
    repo = args.repo
    failures = []

    for path in grep(repo, MACRO):
        if path not in ALLOWED:
            failures.append(
                "%s references %s outside the upstream-sanctioned files "
                "(allowed: %s)" % (path, MACRO, ", ".join(sorted(ALLOWED)))
            )

    for token in FORBIDDEN_TOKENS:
        for path in grep(repo, token):
            failures.append("fork-only token %r still present in %s" % (token, path))

    if args.upstream:
        if not ref_exists(repo, args.upstream):
            print("note: upstream ref %r not available; skipped parity check"
                  % args.upstream)
        else:
            for path in PARITY_FILES:
                diff = subprocess.run(
                    ["git", "-C", repo, "diff", "--quiet", args.upstream, "--", path]
                )
                if diff.returncode != 0:
                    failures.append(
                        "parity file diverged from upstream %s: %s"
                        % (args.upstream, path)
                    )
    else:
        print("note: no --upstream ref; skipped upstream parity check")

    if failures:
        for failure in failures:
            print("FAIL: " + failure)
        return 1

    print("OK: renderer-selection parity intact")
    return 0


if __name__ == "__main__":
    sys.exit(main())

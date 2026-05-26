#!/usr/bin/env python3
"""Regenerate ETCA test expectations by running llc on each test file."""

import subprocess
import re
import sys
import os
from pathlib import Path

LLC = "build-etca/bin/llc"
TEST_DIR = "llvm/test/CodeGen/ETCA"
FAILED_TESTS = [
    "calling-conv.ll", "calling-conv-masks.ll", "fibonacci.ll",
    "mem-libcalls.ll", "mul-div-16.ll", "mul-div-32.ll", "mul-div-64.ll",
    "mul-div-asm-16.ll", "no-identity-moves.ll", "saf.ll",
    "saturating-abs-16.ll", "saturating-abs-32.ll", "saturating-abs-64.ll",
    "saturating-abs-byte.ll", "select-with-call.ll", "stack-args.ll",
    "stack-frame.ll",
]

def parse_run_lines(filepath):
    """Extract RUN: lines that use llc with -march=etca."""
    run_lines = []
    with open(filepath) as f:
        for line in f:
            m = re.match(r'; RUN:\s+llc\s+(.*?)\s*<\s*%\s*\| FileCheck', line)
            if m:
                run_lines.append(m.group(1))
            m = re.match(r'; RUN:\s+llc\s+(.*?)\s*<\s*%\s*\|[^|]*FileCheck', line)
            if m:
                run_lines.append(m.group(1))
    return run_lines

def get_check_prefix(run_line, filepath):
    """Extract the check-prefix from a RUN line."""
    m = re.search(r'--check-prefix(?:es)?=(\S+)', run_line)
    if m:
        return m.group(1)
    # For saf.ll style without check-prefixes
    return "CHECK"

def main():
    os.chdir(os.path.dirname(os.path.abspath(__file__)))
    
    for test_name in FAILED_TESTS:
        filepath = os.path.join(TEST_DIR, test_name)
        if not os.path.exists(filepath):
            print(f"SKIP {test_name}: not found")
            continue
        
        print(f"UPDATING {test_name}...")
        
        # Read original content
        with open(filepath) as f:
            content = f.read()
        
        # Run llc with default flags (no mattr first)
        result = subprocess.run(
            [LLC, "-march=etca", "-mcpu=generic", filepath],
            capture_output=True, text=True
        )
        asm_output = result.stdout
        
        if not asm_output:
            print(f"  WARN: no output for {test_name}")
            continue
        
        # Generate new CHECK lines using a simple approach:
        # 1. For each function in the assembly, extract it
        # 2. Replace the corresponding CHECK section in the test file
        # 
        # But this is complex. Simpler approach: just update the specific patterns.
        
        # Let's first check what the new output looks like for key patterns
        # and do targeted replacements.
        
        print(f"  Output has {len(asm_output)} chars, {asm_output.count('.globl')} globals")
        
        # For now, just show the diff for a sample
        if test_name in ["saf.ll", "stack-frame.ll", "no-identity-moves.ll", "fibonacci.ll"]:
            old_result = subprocess.run(
                [LLC, "-march=etca", "-mcpu=generic", filepath],
                capture_output=True, text=True
            )
            print(f"  === NEW OUTPUT SAMPLE ===")
            for line in old_result.stdout.split('\n')[:30]:
                print(f"  {line}")

if __name__ == "__main__":
    main()

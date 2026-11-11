#!/usr/bin/env python3
"""Compile the PCI read hooks from kern_start.cpp and test them in userspace."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

source = Path(__file__).resolve().parents[1] / 'src/RembrandtGfx'
code = (source / 'kern_start.cpp').read_text()
start = code.index('static mach_vm_address_t orgPciRead8,')
end = code.index('static void routePciDevid(', start)
with tempfile.TemporaryDirectory(prefix='rembrandt-pci-test-') as directory:
    build = Path(directory)
    (build / 'actual-pci-hooks.inc').write_text(code[start:end])
    compiler = shlex.split(os.environ.get('CXX', 'clang++'))
    subprocess.run(compiler + ['-std=c++17', '-Wall', '-Wextra', '-Werror',
                              '-I' + str(build), str(source / 'tests/pci-identity.cpp'),
                              '-o', str(build / 'pci-identity-test')], check=True)
    subprocess.run([str(build / 'pci-identity-test')], check=True)

#!/usr/bin/env python3
"""Compile the independent POSIX dialog test and run only its fake Zenity backend."""
import argparse
from pathlib import Path
import subprocess
import tempfile

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--work',type=Path,required=True)
args=parser.parse_args()
repository=Path(__file__).resolve().parents[3]
work=args.work.resolve();work.mkdir(parents=True,exist_ok=True)
executable=work/'native-file-dialog-tests'
subprocess.run(['c++','-std=c++17','-Wall','-Wextra','-Werror',str(repository/'UICreator/Utils/NativeFileDialog.cpp'),str(Path(__file__).with_name('Dialog.cpp')),'-o',str(executable)],check=True)
with tempfile.TemporaryDirectory(prefix='fake-dialog-',dir=work) as fixture:
    subprocess.run([str(executable),fixture],cwd=fixture,check=True,timeout=20)

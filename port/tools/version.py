"""Gibt die Programmversion aus res/version.rc aus (für den Dateinamen der Release-EXE)."""
import os
import re

rc = open(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'res', 'version.rc'), encoding='utf-8').read()
print(re.search(r'#define MADTV_VERSION_STR "([^"]+)"', rc).group(1))

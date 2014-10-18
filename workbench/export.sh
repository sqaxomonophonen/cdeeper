#!/bin/sh
cd "$( dirname "${BASH_SOURCE[0]}" )"
blender -noaudio -b $1 -P export.py

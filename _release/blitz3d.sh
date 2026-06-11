#!/bin/bash

# Blitz3D NG IDE Launcher for Linux

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
export blitzpath="$SCRIPT_DIR"
export PATH="$blitzpath/bin:$PATH"

exec "$blitzpath/bin/ide2" "$@"

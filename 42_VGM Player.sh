#!/bin/bash
# SPDX-License-Identifier: GPL-2.0-or-later
# GameShell launcher entry: VGM / VGZ music player (libvgm engine)
# Shift+X previous | Shift+B next | Shift+Y play/pause | Shift+A info
# SELECT quit (A/B/X/Y unshifted and D-pad left/right also work)
export DISPLAY=:0
export HOME=/home/cpi
export XAUTHORITY=/home/cpi/.Xauthority
cd /home/cpi/games/VGMPlayer
exec ./vgm_player

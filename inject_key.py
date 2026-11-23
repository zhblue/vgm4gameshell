#!/usr/bin/env python2
# SPDX-License-Identifier: GPL-2.0-or-later
# Inject an X11 key press by keycode into the focused window.
# Usage: inject_key.py <keycode>   or   inject_key.py --lookup
import sys
import time

from Xlib import X, display, XK
from Xlib.ext import xtest

d = display.Display()

if len(sys.argv) > 1 and sys.argv[1] == '--lookup':
    for name in ('Return', 'space', 'i', 'j', 'y', 'u', 'h', 'l', 'o',
                 'Up', 'Down', 'Left', 'Right'):
        ks = XK.string_to_keysym(name)
        print('%-8s keysym=0x%x keycode=%d' % (name, ks, d.keysym_to_keycode(ks)))
    sys.exit(0)

kc = int(sys.argv[1])
xtest.fake_input(d, X.KeyPress, kc)
d.sync()
time.sleep(0.05)
xtest.fake_input(d, X.KeyRelease, kc)
d.sync()

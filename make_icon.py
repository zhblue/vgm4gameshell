#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Generate the GameShell 80x80 menu icon for the VGM player."""
from PIL import Image, ImageDraw

S = 80
img = Image.new("RGBA", (S, S), (0, 0, 0, 0))
d = ImageDraw.Draw(img)

# rounded dark tile with cyan border
d.rounded_rectangle((2, 2, 77, 77), radius=12, fill=(16, 26, 40, 255),
                    outline=(70, 190, 210, 255), width=2)

# equalizer bars behind the note
bars = [(10, 44, 6), (20, 30, 6), (30, 38, 6), (50, 34, 6), (60, 46, 6), (70, 28, 5)]
for x, h, w in bars:
    d.rectangle((x, 62 - h, x + w, 62), fill=(40, 120, 145, 255))

# musical note: two heads + stems + beam
gold = (255, 215, 95, 255)
d.ellipse((24, 50, 38, 62), fill=gold)
d.ellipse((48, 44, 62, 56), fill=gold)
d.rectangle((35, 18, 38, 56), fill=gold)
d.rectangle((59, 14, 62, 50), fill=gold)
d.polygon([(35, 18), (62, 14), (62, 21), (35, 25)], fill=gold)

# small "VGM" caption plate
d.rounded_rectangle((14, 66, 66, 76), radius=4, fill=(10, 18, 28, 235))
d.text((27, 67), "VGM", fill=(140, 225, 235, 255))

img.save("C:/Users/zhblu/AppData/Local/Temp/gs/vgmplayer/VGM Player.png")
print("saved VGM Player.png")

from PIL import Image, ImageDraw

GRAY = (70, 70, 70, 255)
BLACK = (0, 0, 0, 255)
TRANSPARENT = (0, 0, 0, 0)

# ── LUKE (36×72) ──────────────────────────────────────────────────────────────
luke = Image.new("RGBA", (36, 72), TRANSPARENT)
d = ImageDraw.Draw(luke)

# Head
d.ellipse([(12, 0), (24, 13)], fill=GRAY)
# Neck
d.rectangle([(15, 13), (21, 17)], fill=GRAY)
# Right arm raised (viewer left)
d.polygon([(12, 17), (8, 17), (2, 2), (6, 2)], fill=GRAY)
# Left arm hanging (viewer right)
d.polygon([(24, 17), (30, 17), (32, 40), (27, 40)], fill=GRAY)
# Torso
d.polygon([(8, 17), (28, 17), (26, 46), (10, 46)], fill=GRAY)
# Left leg
d.polygon([(10, 46), (18, 46), (17, 72), (8, 72)], fill=GRAY)
# Right leg
d.polygon([(18, 46), (26, 46), (28, 72), (19, 72)], fill=GRAY)

luke.save("luke.png", "PNG")
print(f"luke.png  size={luke.size}  mode={luke.mode}")

# ── VADER HELMET (36×44) ──────────────────────────────────────────────────────
vader = Image.new("RGBA", (36, 44), TRANSPARENT)
d = ImageDraw.Draw(vader)

# Dome
d.ellipse([(2, 0), (34, 26)], fill=GRAY)
# Jaw flare
d.polygon([(0, 20), (36, 20), (34, 44), (2, 44)], fill=GRAY)

# Black T-visor details
d.rectangle([(4, 8), (32, 14)], fill=BLACK)    # visor bar
d.rectangle([(16, 14), (20, 24)], fill=BLACK)  # nose bridge
d.rectangle([(6, 27), (30, 29)], fill=BLACK)   # grille bar 1
d.rectangle([(6, 31), (30, 33)], fill=BLACK)   # grille bar 2
d.rectangle([(6, 35), (30, 37)], fill=BLACK)   # grille bar 3

vader.save("vader.png", "PNG")
print(f"vader.png size={vader.size}  mode={vader.mode}")

from PIL import Image
import os

# Path to the rocks folder
folder = "."

files = [
    "Rock1.png", # 64 x 64
    "Rock2.png", # 64 x 64
    "Rock3.png", # 64 x 64
    "Rock4.png"  # 64 x 64
]

# Load images
images = [Image.open(f"{folder}/{f}") for f in files]

# Calculate total size (Width is 64, Height is 64 * 4 = 256)
w = images[0].width
total_h = sum(img.height for img in images)

# Create transparent canvas
combined = Image.new("RGBA", (w, total_h))

# Paste one under the other
y_offset = 0
for img in images:
    combined.paste(img, (0, y_offset))
    y_offset += img.height

# Save Atlas
combined.save(f"{folder}/RocksAtlas.png")

print(f"✅ RocksAtlas.png successfully created! Size: {w}x{total_h}")
from PIL import Image
import os

# Path to the bushes folder
folder = "."

files = [
    "Bushe1.png", # 1024 x 128
    "Bushe2.png", # 1024 x 128
    "Bushe3.png", # 1024 x 128
    "Bushe4.png"  # 1024 x 128
]

# Load images
images = [Image.open(f"{folder}/{f}") for f in files]

# Calculate total size (Width is 1024, Height is 128 * 4 = 512)
w = images[0].width
total_h = sum(img.height for img in images)

# Create massive transparent canvas
combined = Image.new("RGBA", (w, total_h))

# Paste one under the other
y_offset = 0
for img in images:
    combined.paste(img, (0, y_offset))
    y_offset += img.height

# Save Atlas
combined.save(f"{folder}/BushesAtlas.png")

print(f"✅ BushesAtlas.png successfully created! Size: {w}x{total_h}")
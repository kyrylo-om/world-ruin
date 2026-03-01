from PIL import Image

# File paths
folder = "."

files = [
    "Tilemap_color1.png",
    "Tilemap_color2.png",
    "Tilemap_color3.png",
    "Tilemap_color5.png"
]

# Load images
images = [Image.open(f"{folder}/{f}") for f in files]

# Assume all same size
w, h = images[0].size

# Create big canvas (2x2)
combined = Image.new("RGBA", (w * 2, h * 2))

# Paste images in order
combined.paste(images[0], (0, 0))       # 1 → top-left
combined.paste(images[1], (w, 0))       # 2 → top-right
combined.paste(images[2], (0, h))       # 3 → bottom-left
combined.paste(images[3], (w, h))       # 5 → bottom-right

# Save result
combined.save(f"{folder}/Tilemap_combined.png")

print("✅ Tilemaps combined successfully!")
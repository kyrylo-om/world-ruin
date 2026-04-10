from PIL import Image
import os

base_path = "."
trees_path = f"{base_path}/Resources/Wood/Trees/TreesAtlas.png"
bushes_path = f"{base_path}/Decorations/Bushes/BushesAtlas.png"
rocks_path = f"{base_path}/Decorations/Rocks/RocksAtlas.png"

files = [trees_path, bushes_path, rocks_path]

images = [Image.open(f) for f in files]

w = max(img.width for img in images)
total_h = sum(img.height for img in images)

combined = Image.new("RGBA", (w, total_h))

y_offset = 0
for img, filepath in zip(images, files):
    combined.paste(img, (0, y_offset))
    name = os.path.basename(filepath)
    print(f"Pasted {name:<15} at Y-Offset: {y_offset}")
    y_offset += img.height

out_path = f"{base_path}/ResourcesAtlas.png"
combined.save(out_path)

print(f"ResourcesAtlas.png successfully created! Size: {w}x{total_h}")

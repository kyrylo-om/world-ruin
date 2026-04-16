from PIL import Image
import os

# Шлях до ваших дерев
folder = "."

files = [
    "Tree1.png", # 1536 x 256
    "Tree2.png", # 1536 x 256
    "Tree3.png", # 1536 x 192
    "Tree4.png"  # 1536 x 192
]

# Завантажуємо зображення
images = [Image.open(f"{folder}/{f}") for f in files]

# Знаходимо ширину (всі 1536) та загальну висоту (256+256+192+192 = 896)
w = images[0].width
total_h = sum(img.height for img in images)

# Створюємо велике полотно (RGBA для прозорості)
combined = Image.new("RGBA", (w, total_h))

# Вставляємо зображення одне під одним
y_offset = 0
for img in images:
    combined.paste(img, (0, y_offset))
    y_offset += img.height

# Зберігаємо атлас
combined.save(f"{folder}/TreesAtlas.png")

print(f"✅ TreesAtlas.png успішно створено! Розмір: {w}x{total_h}")
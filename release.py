import os
import shutil

def copy_item(src, dst):
    """Helper to copy a file or directory safely."""
    if not os.path.exists(src):
        print(f"[Warning] '{src}' does not exist. Skipping.")
        return

    if os.path.isdir(src):
        shutil.copytree(src, dst)
    else:
        shutil.copy2(src, dst)

def main():
    current_dir = os.path.abspath(os.getcwd())
    release_folder_name = "WorldRuin Simulator"
    zip_name = "WorldRuin_Simulator"

    target_path = os.path.join(current_dir, release_folder_name)

    if os.path.exists(target_path):
        print(f"Removing old '{release_folder_name}/' folder...")
        shutil.rmtree(target_path)
    if os.path.exists(f"{zip_name}.zip"):
        print(f"Removing old '{zip_name}.zip'...")
        os.remove(f"{zip_name}.zip")

    os.makedirs(target_path)
    print(f"Created '{release_folder_name}/' folder.")

    release_scripts_dir = os.path.join(current_dir, "release-scripts")
    if os.path.exists(release_scripts_dir):
        print("Copying contents of 'release-scripts'...")
        for item in os.listdir(release_scripts_dir):
            src_item = os.path.join(release_scripts_dir, item)
            dst_item = os.path.join(target_path, item)
            copy_item(src_item, dst_item)
    else:
        print("[Warning] 'release-scripts' folder not found.")

    print("Copying 'assets' and 'pack' folders...")
    copy_item(os.path.join(current_dir, "assets"), os.path.join(target_path, "assets"))
    copy_item(os.path.join(current_dir, "pack"), os.path.join(target_path, "pack"))

    print("Copying 'controls.md'...")
    copy_item(os.path.join(current_dir, "controls.md"), os.path.join(target_path, "controls.md"))

    print("Copying executable...")
    exe_src = os.path.join(current_dir, "cmake-build-release", "WorldRuin.exe")
    exe_dst = os.path.join(target_path, "WorldRuin.exe")
    copy_item(exe_src, exe_dst)

    print(f"Zipping '{release_folder_name}/' into '{zip_name}.zip'...")
    shutil.make_archive(zip_name, 'zip', root_dir=current_dir, base_dir=release_folder_name)

    print("Cleaning up temporary release folder...")
    shutil.rmtree(target_path)

    print(f"\nDone! Release package ready: {zip_name}.zip")

if __name__ == "__main__":
    main()
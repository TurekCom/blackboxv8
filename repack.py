import os
import shutil
import zipfile

ADDON_NAME = "blackbox_v8"
BUILD_DIR = "build_addon"
DIST_DIR = "dist"

def pack():
    if os.path.exists(BUILD_DIR): shutil.rmtree(BUILD_DIR)
    # NVDA oczekuje folderu synthDrivers (camelCase).
    os.makedirs(os.path.join(BUILD_DIR, "synthDrivers"))
    with open(os.path.join(BUILD_DIR, "synthDrivers", "__init__.py"), "w", encoding="utf-8") as f:
        f.write("# synthDrivers package for NVDA addon\n")
    
    # Kopiujemy sterownik oraz moduły runtime/core.
    shutil.copy("blackbox_v8_driver.py", os.path.join(BUILD_DIR, "synthDrivers", "blackbox_v8.py"))
    shutil.copy("blackbox_core.py", os.path.join(BUILD_DIR, "synthDrivers", "blackbox_core.py"))
    shutil.copy("blackbox_runtime.py", os.path.join(BUILD_DIR, "synthDrivers", "blackbox_runtime.py"))
    shutil.copy("blackbox_rom.py", os.path.join(BUILD_DIR, "synthDrivers", "blackbox_rom.py"))
    shutil.copy("manifest.ini", BUILD_DIR)
    
    if not os.path.exists(DIST_DIR): os.makedirs(DIST_DIR)
    addon_file = os.path.join(DIST_DIR, f"{ADDON_NAME}.nvda-addon")
    with zipfile.ZipFile(addon_file, 'w', zipfile.ZIP_DEFLATED) as z:
        for r, d, files in os.walk(BUILD_DIR):
            for f in files:
                fp = os.path.join(r, f)
                z.write(fp, os.path.relpath(fp, BUILD_DIR))
    print(f"Utworzono: {addon_file}")

if __name__ == "__main__":
    pack()

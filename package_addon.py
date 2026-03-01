import os
import sys
import shutil
import subprocess
import zipfile

# Konfiguracja
ADDON_NAME = "blackbox_v8"
BUILD_DIR = "build_addon"
DIST_DIR = "dist"
LIBS_DIR = os.path.join(BUILD_DIR, "lib")

def prepare_structure():
    print(f"Przygotowywanie struktury w {BUILD_DIR}...")
    if os.path.exists(BUILD_DIR):
        shutil.rmtree(BUILD_DIR)
    os.makedirs(os.path.join(BUILD_DIR, "synthDrivers"))
    os.makedirs(LIBS_DIR)
    with open(os.path.join(BUILD_DIR, "synthDrivers", "__init__.py"), "w", encoding="utf-8") as f:
        f.write("# synthDrivers package for NVDA addon\n")

    # Kopiowanie plików źródłowych
    shutil.copy("blackbox_core.py", os.path.join(BUILD_DIR, "synthDrivers", "blackbox_core.py"))
    shutil.copy("blackbox_runtime.py", os.path.join(BUILD_DIR, "synthDrivers", "blackbox_runtime.py"))
    shutil.copy("blackbox_rom.py", os.path.join(BUILD_DIR, "synthDrivers", "blackbox_rom.py"))
    shutil.copy("blackbox_v8_driver.py", os.path.join(BUILD_DIR, "synthDrivers", "blackbox_v8.py"))
    shutil.copy("manifest.ini", BUILD_DIR)
    shutil.copy("__init__.py", BUILD_DIR)

def install_dependencies():
    print("Instalowanie lekkich zalezności (num2words)...")
    # numpy/scipy USUNIĘTE - teraz synteza jest w czystym Pythonie
    try:
        subprocess.check_call([
            sys.executable, "-m", "pip", "install", 
            "num2words",
            "--target", LIBS_DIR,
            "--no-compile",
            "--quiet"
        ])
    except Exception as e:
        print(f"Błąd podczas instalacji zalezności: {e}")

def update_driver_path():
    print("Aktualizacja ściezek w sterowniku...")
    driver_path = os.path.join(BUILD_DIR, "synthDrivers", "blackbox_v8.py")
    with open(driver_path, "r", encoding="utf-8") as f:
        content = f.read()
    
    path_fix = """import sys
import os
import os.path
# Dodanie folderu lib do ściezki
lib_path = os.path.join(os.path.dirname(os.path.dirname(__file__)), 'lib')
if lib_path not in sys.path:
    sys.path.insert(0, lib_path)
"""
    with open(driver_path, "w", encoding="utf-8") as f:
        f.write(path_fix + content)

def create_addon_file():
    print(f"Tworzenie pliku {ADDON_NAME}.nvda-addon...")
    if not os.path.exists(DIST_DIR):
        os.makedirs(DIST_DIR)
    
    addon_path = os.path.join(DIST_DIR, f"{ADDON_NAME}.nvda-addon")
    with zipfile.ZipFile(addon_path, 'w', zipfile.ZIP_DEFLATED) as zipf:
        for root, dirs, files in os.walk(BUILD_DIR):
            for file in files:
                rel_path = os.path.relpath(os.path.join(root, file), BUILD_DIR)
                zipf.write(os.path.join(root, file), rel_path)
    
    print(f"Gotowe! Plik znajduje się w: {addon_path}")

if __name__ == "__main__":
    prepare_structure()
    # Obecny silnik jest czysty Python, więc nie wymaga dodatkowych bibliotek.
    create_addon_file()

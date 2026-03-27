import os
import shutil
import subprocess
import zipfile
from pathlib import Path

ADDON_NAME = "blackbox_v8"
BUILD_DIR = "build_addon"
DIST_DIR = "dist"
CPP_CORE_DIR = "cpp_core"
EMOJI_TSV = Path("android") / "app" / "src" / "main" / "assets" / "emoji" / "emoji_pl_cldr.tsv"
NATIVE_BUILDS = {
    "x86": {"generator_arch": "Win32", "build_dir": os.path.join(CPP_CORE_DIR, "build-nvda-x86")},
    "x64": {"generator_arch": "x64", "build_dir": os.path.join(CPP_CORE_DIR, "build-nvda-x64")},
}

def build_native_backend():
    artifacts = {}
    for arch, data in NATIVE_BUILDS.items():
        build_dir = data["build_dir"]
        print(f"Budowanie natywnego backendu NVDA ({arch})...")
        subprocess.check_call([
            "cmake", "-S", CPP_CORE_DIR, "-B", build_dir, "-A", data["generator_arch"]
        ])
        subprocess.check_call([
            "cmake", "--build", build_dir, "--config", "Release", "--target", "blackbox_nvda_native"
        ])
        dll_path = os.path.join(build_dir, "Release", "blackbox_nvda_native.dll")
        if not os.path.exists(dll_path):
            raise FileNotFoundError(f"Nie znaleziono DLL po buildzie: {dll_path}")
        artifacts[arch] = dll_path
    return artifacts

def prepare_structure():
    print(f"Przygotowywanie struktury w {BUILD_DIR}...")
    if os.path.exists(BUILD_DIR):
        shutil.rmtree(BUILD_DIR)
    os.makedirs(os.path.join(BUILD_DIR, "synthDrivers"))
    os.makedirs(os.path.join(BUILD_DIR, "synthDrivers", "bin", "x86"))
    os.makedirs(os.path.join(BUILD_DIR, "synthDrivers", "bin", "x64"))
    os.makedirs(os.path.join(BUILD_DIR, "synthDrivers", "data", "emoji"))

    shutil.copy("blackbox_v8_driver.py", os.path.join(BUILD_DIR, "synthDrivers", "blackbox_v8.py"))
    shutil.copy("manifest.ini", BUILD_DIR)
    shutil.copy(EMOJI_TSV, os.path.join(BUILD_DIR, "synthDrivers", "data", "emoji", "emoji_pl_cldr.tsv"))

def copy_native_backend(artifacts):
    for arch, dll_path in artifacts.items():
        dst = os.path.join(BUILD_DIR, "synthDrivers", "bin", arch, "blackbox_nvda_native.dll")
        shutil.copy(dll_path, dst)

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
    native_artifacts = build_native_backend()
    prepare_structure()
    copy_native_backend(native_artifacts)
    create_addon_file()

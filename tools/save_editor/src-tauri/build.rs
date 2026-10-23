fn main() {
    let helper = std::env::var_os("MOON_SAVE_CATALOG")
        .map(std::path::PathBuf::from)
        .unwrap_or_else(|| {
            std::path::PathBuf::from("../../../build/Release/moon_save_catalog.exe")
        });
    println!("cargo:rerun-if-env-changed=MOON_SAVE_CATALOG");
    println!("cargo:rerun-if-changed={}", helper.display());
    std::fs::copy(
        &helper,
        std::path::PathBuf::from(std::env::var_os("OUT_DIR").unwrap())
            .join("moon_save_catalog.exe"),
    )
    .expect("Build the moon_save_catalog CMake target first (or set MOON_SAVE_CATALOG)");
    let vehicle = std::env::var_os("MOON_SAVE_VEHICLE")
        .map(std::path::PathBuf::from)
        .unwrap_or_else(|| {
            std::path::PathBuf::from("../../../build/Release/moon_save_vehicle.exe")
        });
    println!("cargo:rerun-if-env-changed=MOON_SAVE_VEHICLE");
    println!("cargo:rerun-if-changed={}", vehicle.display());
    std::fs::copy(
        vehicle,
        std::path::PathBuf::from(std::env::var_os("OUT_DIR").unwrap())
            .join("moon_save_vehicle.exe"),
    )
    .expect("Build the moon_save_vehicle CMake target first");
    // Neutral document glyph, generated from code; no game or branding assets.
    let path = std::path::PathBuf::from(std::env::var_os("OUT_DIR").unwrap()).join("document.ico");
    let size = 40 + 32 * 32 * 4 + 32 * 4;
    let mut b = vec![0, 0, 1, 0, 1, 0, 32, 32, 0, 0, 1, 0, 32, 0];
    b.extend((size as u32).to_le_bytes());
    b.extend(22u32.to_le_bytes());
    for n in [40u32, 32, 64, 0x00200001, 0, 4096, 0, 0, 0, 0] {
        b.extend(n.to_le_bytes());
    }
    for y in 0..32 {
        for x in 0..32 {
            let inside = (8..24).contains(&x) && (4..28).contains(&y);
            let edge = inside
                && (x == 8
                    || x == 23
                    || y == 4
                    || y == 27
                    || ((y == 12 || y == 18) && (12..20).contains(&x)));
            let c = if edge { 220 } else { 27 };
            b.extend([c, c, c, if inside { 255 } else { 0 }]);
        }
    }
    b.extend([0u8; 128]);
    std::fs::write(&path, b).unwrap();
    tauri_build::try_build(
        tauri_build::Attributes::new()
            .windows_attributes(tauri_build::WindowsAttributes::new().window_icon_path(path)),
    )
    .expect("build metadata");
}

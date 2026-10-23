#![cfg_attr(not(debug_assertions), windows_subsystem = "windows")]
mod format;
use format::Node;
use serde::Serialize;
use std::{
    fs,
    io::Write,
    path::{Path, PathBuf},
    sync::Mutex,
    time::{SystemTime, UNIX_EPOCH},
};
#[derive(Default)]
struct Document {
    path: Option<PathBuf>,
    original: Vec<u8>,
}
#[derive(Serialize)]
struct Opened {
    path: String,
    root: Node,
    timestamp: u64,
}
fn error(e: impl std::fmt::Display) -> String {
    e.to_string()
}
fn app_data() -> Result<PathBuf, String> {
    std::env::var_os("APPDATA")
        .map(|p| PathBuf::from(p).join("MoonSaveEditor"))
        .ok_or("APPDATA is unavailable".into())
}
#[tauri::command]
fn saved_catalog() -> Result<Option<serde_json::Value>, String> {
    let path = app_data()?.join("catalog.json");
    if !path.exists() {
        return Ok(None);
    }
    serde_json::from_slice(&fs::read(path).map_err(error)?)
        .map(Some)
        .map_err(error)
}
#[tauri::command]
async fn vehicle_action(
    root: Node,
    action: String,
    args: Vec<String>,
) -> Result<serde_json::Value, String> {
    tauri::async_runtime::spawn_blocking(move || {
        if !["add", "delete", "select", "stats", "tune", "refresh"].contains(&action.as_str()) {
            return Err("Unknown vehicle action".into());
        }
        let catalog =
            saved_catalog()?.ok_or("Load game data before using the native vehicle tools.")?;
        let disc = catalog["disc"]
            .as_str()
            .ok_or("Reload game data to choose the source disc.")?;
        let cache = app_data()?.join("cache");
        fs::create_dir_all(&cache).map_err(error)?;
        let helper = include_bytes!(concat!(env!("OUT_DIR"), "/moon_save_vehicle.exe"));
        let exe = cache.join(format!("vehicle-{:08x}.exe", crc32fast::hash(helper)));
        if !exe.exists() {
            fs::write(&exe, helper).map_err(error)?;
        }
        let unique = SystemTime::now()
            .duration_since(UNIX_EPOCH)
            .map_err(error)?
            .as_nanos();
        let folder = cache.join(format!("operation-{}-{unique}", std::process::id()));
        fs::create_dir(&folder).map_err(error)?;
        let result = (|| -> Result<serde_json::Value, String> {
            let input = folder.join("input.ogt4");
            let output = folder.join("output.ogt4");
            fs::write(&input, format::pack(&root, 0)?).map_err(error)?;
            let mut cmd = std::process::Command::new(exe);
            cmd.arg(disc)
                .arg(input)
                .arg(&output)
                .arg(&action)
                .args(args);
            #[cfg(windows)]
            {
                use std::os::windows::process::CommandExt;
                cmd.creation_flags(0x08000000);
            }
            let done = cmd.output().map_err(error)?;
            if !done.status.success() {
                let s = String::from_utf8_lossy(&done.stderr);
                return Err(if s.is_empty() {
                    "Native vehicle tool rejected this record; no changes were applied.".into()
                } else {
                    s.into_owned()
                });
            }
            let stats = if done.stdout.is_empty() {
                serde_json::Value::Null
            } else {
                serde_json::from_slice(&done.stdout).map_err(error)?
            };
            let mut next = root;
            if action != "stats" {
                let (edited, _) = format::unpack(&fs::read(output).map_err(error)?)?;
                if let (Node::Module(old), Node::Module(new)) = (&mut next, edited) {
                    let garage = new
                        .into_iter()
                        .find(|(k, _)| k == "garage")
                        .ok_or("Missing native garage result")?;
                    if let Some(pair) = old.iter_mut().find(|(k, _)| k == "garage") {
                        *pair = garage;
                    } else {
                        old.push(garage);
                    }
                }
            }
            Ok(serde_json::json!({"root":next,"stats":stats}))
        })();
        let _ = fs::remove_file(folder.join("input.ogt4"));
        let _ = fs::remove_file(folder.join("output.ogt4"));
        let _ = fs::remove_dir(&folder);
        result
    })
    .await
    .map_err(error)?
}
#[tauri::command]
async fn load_catalog() -> Result<Option<serde_json::Value>, String> {
    tauri::async_runtime::spawn_blocking(|| {
        let Some(disc) = rfd::FileDialog::new()
            .set_title("Choose your GT4 US disc to load car names and licenses")
            .add_filter("GT4 disc image", &["iso"])
            .pick_file()
        else {
            return Ok(None);
        };
        let cache = app_data()?.join("cache");
        fs::create_dir_all(&cache).map_err(error)?;
        let helper = include_bytes!(concat!(env!("OUT_DIR"), "/moon_save_catalog.exe"));
        let path = cache.join(format!("catalog-{:08x}.exe", crc32fast::hash(helper)));
        if !path.exists() {
            fs::write(&path, helper).map_err(error)?;
        }
        let mut cmd = std::process::Command::new(&path);
        cmd.arg(&disc);
        #[cfg(windows)]
        {
            use std::os::windows::process::CommandExt;
            cmd.creation_flags(0x08000000);
        }
        let output = cmd.output().map_err(error)?;
        if !output.status.success() {
            return Err(String::from_utf8_lossy(&output.stderr).into_owned());
        }
        let mut data: serde_json::Value = serde_json::from_slice(&output.stdout).map_err(error)?;
        data["disc"] = serde_json::json!(disc.to_string_lossy());
        fs::write(
            app_data()?.join("catalog.json"),
            serde_json::to_vec(&data).map_err(error)?,
        )
        .map_err(error)?;
        Ok(Some(data))
    })
    .await
    .map_err(error)?
}
#[tauri::command]
fn open_profile(state: tauri::State<Mutex<Document>>) -> Result<Option<Opened>, String> {
    let current = state.lock().map_err(error)?.path.clone();
    let directory = current
        .and_then(|p| p.parent().map(Path::to_path_buf))
        .or_else(|| {
            std::env::var_os("APPDATA")
                .map(|p| PathBuf::from(p).join("MoonEngine/MoonEngine"))
                .filter(|p| p.is_dir())
        });
    let mut dialog = rfd::FileDialog::new().add_filter("Moon Engine profile", &["ogt4", "bak"]);
    if let Some(directory) = directory {
        dialog = dialog.set_directory(directory);
    }
    let Some(path) = dialog.pick_file() else {
        return Ok(None);
    };
    let b = fs::read(&path).map_err(error)?;
    let (root, timestamp) = format::unpack(&b)?;
    let mut doc = state.lock().map_err(error)?;
    doc.path = Some(path.clone());
    doc.original = b;
    Ok(Some(Opened {
        path: path.to_string_lossy().into(),
        root,
        timestamp,
    }))
}
fn write_copy(path: &Path, bytes: &[u8]) -> Result<(), String> {
    // A new file is committed without truncating the destination on failure.
    let temp = path.with_extension(format!("ogt4.tmp.{}", std::process::id()));
    let mut file = fs::OpenOptions::new()
        .write(true)
        .create_new(true)
        .open(&temp)
        .map_err(error)?;
    let result = (|| {
        file.write_all(bytes).map_err(error)?;
        file.sync_all().map_err(error)?;
        drop(file);
        // Save Copy deliberately requires a new name; commit_new also closes the race.
        if path.exists() {
            return Err("Destination already exists. Choose a new filename.".into());
        }
        commit_new(&temp, path)?;
        Ok(())
    })();
    let _ = fs::remove_file(temp);
    result
}
#[cfg(windows)]
fn commit_new(temp: &Path, path: &Path) -> Result<(), String> {
    use std::os::windows::ffi::OsStrExt;
    #[link(name = "kernel32")]
    extern "system" {
        fn MoveFileExW(from: *const u16, to: *const u16, flags: u32) -> i32;
    }
    let from: Vec<u16> = temp.as_os_str().encode_wide().chain(Some(0)).collect();
    let to: Vec<u16> = path.as_os_str().encode_wide().chain(Some(0)).collect();
    // WRITE_THROUGH, deliberately without REPLACE_EXISTING. Also supports FAT SD cards.
    if unsafe { MoveFileExW(from.as_ptr(), to.as_ptr(), 8) } == 0 {
        return Err(error(std::io::Error::last_os_error()));
    }
    Ok(())
}
#[cfg(not(windows))]
fn commit_new(temp: &Path, path: &Path) -> Result<(), String> {
    fs::hard_link(temp, path).map_err(error)
}
#[tauri::command]
fn save_copy(root: Node, state: tauri::State<Mutex<Document>>) -> Result<Option<String>, String> {
    format::validate(&root)?;
    let Some(path) = rfd::FileDialog::new()
        .add_filter("Moon Engine profile", &["ogt4"])
        .set_file_name("edited-profile.ogt4")
        .save_file()
    else {
        return Ok(None);
    };
    let doc = state.lock().map_err(error)?;
    let backup_dir = std::env::var_os("APPDATA")
        .map(PathBuf::from)
        .ok_or("APPDATA is unavailable")?
        .join("MoonSaveEditor/backups");
    save_document(&root, &doc, &path, &backup_dir).map(Some)
}
fn save_document(
    root: &Node,
    doc: &Document,
    path: &Path,
    backup_dir: &Path,
) -> Result<String, String> {
    let now = SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .map_err(error)?;
    let bytes = format::pack(root, now.as_secs())?;
    let source = doc.path.as_ref().ok_or("Open a profile first")?;
    if fs::read(source).map_err(error)? != doc.original {
        return Err("The source changed on disk. Reopen it before saving.".into());
    }
    if path.exists() {
        return Err("Choose a new filename. Existing profiles are protected.".into());
    }
    fs::create_dir_all(&backup_dir).map_err(error)?;
    let backup = backup_dir.join(format!(
        "{}-{}-{}.ogt4",
        source.file_stem().unwrap_or_default().to_string_lossy(),
        now.as_secs(),
        now.subsec_nanos()
    ));
    let mut bf = fs::OpenOptions::new()
        .create_new(true)
        .write(true)
        .open(&backup)
        .map_err(error)?;
    bf.write_all(&doc.original).map_err(error)?;
    bf.sync_all().map_err(error)?;
    write_copy(&path, &bytes)?;
    Ok(format!(
        "Saved {}\nOriginal backed up to {}",
        path.display(),
        backup.display()
    ))
}
#[tauri::command]
fn validate_profile(root: Node) -> Result<(), String> {
    format::validate(&root).map(|_| ())
}
fn main() {
    // Focused interoperability probe: no window, no access to live player data.
    let args: Vec<String> = std::env::args().collect();
    if args.get(1).map(String::as_str) == Some("--apply-json") && args.len() == 4 {
        let result = (|| -> Result<(), String> {
            let root: Node =
                serde_json::from_slice(&fs::read(&args[2]).map_err(error)?).map_err(error)?;
            write_copy(Path::new(&args[3]), &format::pack(&root, 0)?)
        })();
        if let Err(e) = result {
            eprintln!("{e}");
            std::process::exit(1);
        }
        return;
    }
    if args.get(1).map(String::as_str) == Some("--inspect") && args.len() == 3 {
        let result = fs::read(&args[2])
            .map_err(error)
            .and_then(|b| format::unpack(&b));
        match result {
            Ok((root, _)) => println!("{}", serde_json::to_string(&root).unwrap()),
            Err(e) => {
                eprintln!("{e}");
                std::process::exit(1);
            }
        }
        return;
    }
    if args.get(1).map(String::as_str) == Some("--roundtrip") {
        let result = (|| -> Result<(), String> {
            if args.len() != 4 {
                return Err("Usage: --roundtrip INPUT OUTPUT".into());
            }
            let bytes = fs::read(&args[2]).map_err(error)?;
            let (root, stamp) = format::unpack(&bytes)?;
            write_copy(Path::new(&args[3]), &format::pack(&root, stamp)?)
        })();
        if let Err(e) = result {
            eprintln!("{e}");
            std::process::exit(1);
        }
        return;
    }
    tauri::Builder::default()
        .manage(Mutex::new(Document::default()))
        .invoke_handler(tauri::generate_handler![
            saved_catalog,
            load_catalog,
            vehicle_action,
            open_profile,
            save_copy,
            validate_profile
        ])
        .run(tauri::generate_context!())
        .expect("Could not open Moon Save Editor");
}
#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn write_preserves_existing_files_and_creates_complete_copy() {
        let dir = std::env::temp_dir().join(format!("moon-editor-test-{}", std::process::id()));
        fs::create_dir_all(&dir).unwrap();
        let p = dir.join("copy.ogt4");
        let _ = fs::remove_file(&p);
        write_copy(&p, b"original").unwrap();
        assert!(write_copy(&p, b"bad").is_err());
        assert_eq!(fs::read(&p).unwrap(), b"original");
        fs::remove_file(p).unwrap();
        fs::remove_dir(dir).unwrap();
    }
    #[test]
    fn backup_matches_source_and_external_changes_are_refused() {
        let dir =
            std::env::temp_dir().join(format!("moon-editor-backup-test-{}", std::process::id()));
        fs::create_dir_all(&dir).unwrap();
        let root = Node::Module(vec![("money".into(), Node::Int("42".into()))]);
        let source = dir.join("source.ogt4");
        let bytes = format::pack(&root, 100).unwrap();
        fs::write(&source, &bytes).unwrap();
        let doc = Document {
            path: Some(source.clone()),
            original: bytes.clone(),
        };
        let dest = dir.join("edited.ogt4");
        let backups = dir.join("backups");
        let _ = fs::remove_file(&dest);
        save_document(&root, &doc, &dest, &backups).unwrap();
        assert_eq!(fs::read(&source).unwrap(), bytes);
        let paths: Vec<_> = fs::read_dir(&backups)
            .unwrap()
            .map(|e| e.unwrap().path())
            .collect();
        assert_eq!(paths.len(), 1);
        assert_eq!(fs::read(&paths[0]).unwrap(), bytes);
        assert!(save_document(&root, &doc, &dest, &backups).is_err());
        fs::write(&source, b"externally changed").unwrap();
        assert!(save_document(&root, &doc, &dir.join("refused.ogt4"), &backups).is_err());
        assert!(!dir.join("refused.ogt4").exists());
        fs::remove_file(source).unwrap();
        fs::remove_file(dest).unwrap();
        fs::remove_file(&paths[0]).unwrap();
        fs::remove_dir(backups).unwrap();
        fs::remove_dir(dir).unwrap();
    }
}

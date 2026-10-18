"""Private package contents, local binary identity, and invalid-input negatives."""
import hashlib
import importlib.util
import json
import tempfile
import unittest
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location("package_switch", ROOT / "tools/package_switch.py")
package = importlib.util.module_from_spec(spec)
spec.loader.exec_module(package)


class SwitchPackage(unittest.TestCase):
    def test_package_and_source_exclude_player_data(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            nro = root / "test.nro"
            payload = b"\0" * 16 + b"NRO0" + b"\0" * 12
            nro.write_bytes(payload)
            exe = root / "host.exe"
            exe.write_bytes(b"MZ" + b"\0" * 62)
            output = root / "MoonEngine-Switch-0.2.0-private"
            result = package.build_package(nro, output, "0.2.0-private", setup_exe=exe, nxlink=exe)
            self.assertEqual(result.name, output.name + ".zip")
            with zipfile.ZipFile(result) as archive:
                self.assertIsNone(archive.testzip())
                self.assertEqual(archive.read("switch/moonengine/moonengine.nro"), payload)
                manifest = json.loads(archive.read("manifest.json"))
                self.assertEqual(manifest["nro_sha256"], hashlib.sha256(payload).hexdigest())
                self.assertEqual(manifest["disc_serial"], "SCUS-97328")
                self.assertNotIn("disc_sha1",manifest)
                self.assertTrue(manifest["private"])
                self.assertFalse(manifest["usb_hardware_validated"])
                names = archive.namelist()
                self.assertIn("MoonEngine-Switch-Setup.exe", names)
                self.assertIn("installer/nxlink.exe", names)
                self.assertEqual(manifest["nxlink_sha256"], hashlib.sha256(exe.read_bytes()).hexdigest())
                self.assertFalse(any(n.endswith((".cmd", ".ps1")) for n in names))
                self.assertFalse(any(n.endswith((".iso", ".ogt4", ".keys", "config.ini")) for n in names))
            with zipfile.ZipFile(output / "source.zip") as archive:
                self.assertIsNone(archive.testzip())
                names = archive.namelist()
                self.assertIn("src/app/switch_startup.c", names)
                self.assertFalse(any(n.startswith(("assets/", "build/", "captures/", ".git/")) for n in names))
                self.assertFalse(any("/" not in n and n.endswith(".md") and n != "README.md" for n in names))
            with self.assertRaises(FileExistsError):
                package.build_package(nro, output, "0.2.0-private", setup_exe=exe, nxlink=exe)

    def test_rejects_non_nro_and_path_label(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            nro = root / "wrong.nro"
            nro.write_bytes(b"not a binary")
            with self.assertRaises(ValueError):
                package.build_package(nro, root / "out", "private")
            with self.assertRaises(ValueError):
                package.build_package(nro, root / "out", "../../elsewhere")
            self.assertFalse((root / "out").exists())


if __name__ == "__main__":
    unittest.main()

#!/usr/bin/env python3
"""dbt — Delphus Build Tool.

One command for setup, firmware build/flash/debug and FAP plugins on
Windows, macOS and Linux. No shell scripts, no manual ESP-IDF export.

Usage:
    python dbt.py                      detect the OS and install everything needed
    python dbt.py build [board]        build firmware into build/<board>/
    python dbt.py flash [board]        build + flash (port auto-detected)
    python dbt.py debug [board]        build + flash + serial monitor
    python dbt.py fap <name>           build one app from applications_user/
    python dbt.py faps                 build every app from applications_user/

Options:
    --port COM3        force a serial port for flash/debug
    --yes              do not ask before installing system/Python packages
"""

from __future__ import annotations

import argparse
import json
import os
import platform
import re
import shutil
import subprocess
import sys
import sysconfig
import tempfile
import zipfile
from pathlib import Path
from typing import Dict, Iterable, List, NoReturn, Optional

VERSION = "1.0.0"
IDF_VERSION = "v5.5.5"
IDF_REPO = "https://github.com/espressif/esp-idf.git"

# ESP-IDF 5.5.5's Windows requirements (notably windows-curses) have no wheels
# for Python 3.13+. 3.11 is the safe choice; dbt will find or download it.
IDF_PYTHON_MIN = (3, 9)
IDF_PYTHON_MAX = (3, 11)
IDF_PYTHON_PREFERRED = "3.11"

ROOT = Path(__file__).resolve().parent
BUILD_ROOT = ROOT / "build"
USER_APPS = ROOT / "applications_user"

# Only one board remains in this firmware. Aliases are accepted for convenience.
BOARDS: Dict[str, dict] = {
    "delphus": {
        "board": "delphus",
        "target": "esp32s3",
        "sdkconfig_defaults": "sdkconfig.defaults",
    },
}
ALIASES = {
    "st7567": "delphus",
    "esp32s3_st7567": "delphus",
    "delphus_st7567": "delphus",
}

# Ports we know are ESP32 boards / common USB-UART bridges.
KNOWN_USB_VIDS = {0x303A, 0x1A86, 0x10C4, 0x0403, 0x067B}


def c(text: str, code: str) -> str:
    """Colour helper that degrades gracefully on Windows cmd."""
    if os.environ.get("NO_COLOR") or (os.name == "nt" and not os.environ.get("WT_SESSION")):
        return text
    return f"\033[{code}m{text}\033[0m"


def info(msg: str) -> None:
    print(c("[dbt] ", "36") + msg, flush=True)


def ok(msg: str) -> None:
    print(c("[ ok ] ", "32") + msg, flush=True)


def warn(msg: str) -> None:
    print(c("[warn] ", "33") + msg, flush=True)


def fail(msg: str) -> NoReturn:
    print(c("[fail] ", "31") + msg, file=sys.stderr, flush=True)
    raise SystemExit(1)


def confirm(question: str, assume_yes: bool) -> bool:
    if assume_yes:
        return True
    try:
        answer = input(f"{question} [y/N]: ").strip().lower()
    except EOFError:
        return False
    return answer in ("y", "yes")


class DBT:
    def __init__(self, assume_yes: bool = False, port: Optional[str] = None):
        self.assume_yes = assume_yes
        self.forced_port = port
        self.is_windows = os.name == "nt"
        self.env: Dict[str, str] = os.environ.copy()
        self._env_loaded = False
        self.system = platform.system()
        self.distro = self._detect_distro()
        self.idf_path = self._find_idf_path()
        self.idf_tools = Path(
            os.environ.get("IDF_TOOLS_PATH")
            or (Path.home() / ".espressif")
        ).expanduser()
        self.python_exe: str = sys.executable

    # ------------------------------------------------------------------ setup
    @staticmethod
    def _detect_distro() -> str:
        if platform.system() != "Linux":
            return ""
        try:
            text = Path("/etc/os-release").read_text(encoding="utf-8", errors="ignore")
            match = re.search(r'^ID=[\"]?([^\"\n]+)', text, re.MULTILINE)
            return match.group(1).strip().lower() if match else "unknown"
        except OSError:
            return "unknown"

    def _find_idf_path(self) -> Path:
        for key in ("IDF_PATH", "ESP_IDF_DIR", "IDF_DIR"):
            value = os.environ.get(key)
            if value and (Path(value) / "tools" / "idf.py").is_file():
                return Path(value).expanduser()
        default = Path.home() / ".espressif" / f"esp-idf-{IDF_VERSION}"
        return default.expanduser()

    @property
    def export_script(self) -> Path:
        name = "export.bat" if self.is_windows else "export.sh"
        return self.idf_path / name

    @property
    def install_script(self) -> Path:
        name = "install.bat" if self.is_windows else "install.sh"
        return self.idf_path / name

    def _run(
        self,
        cmd,
        *,
        env: Optional[dict] = None,
        cwd: Optional[Path] = None,
        check: bool = True,
        capture: bool = False,
        shell: bool = False,
    ) -> subprocess.CompletedProcess:
        if isinstance(cmd, (list, tuple)):
            printable = " ".join(str(x) for x in cmd)
        else:
            printable = str(cmd)
        info(f"$ {printable}")
        result = subprocess.run(
            cmd,
            env=env or self.env,
            cwd=str(cwd) if cwd else None,
            shell=shell,
            text=True,
            errors="replace",
            stdout=subprocess.PIPE if capture else None,
            stderr=subprocess.STDOUT if capture else None,
        )
        if check and result.returncode != 0:
            fail(f"command failed ({result.returncode}): {printable}")
        return result

    def detect_environment(self) -> None:
        print()
        info("=== Environment ===")
        info(f"OS          : {self.system} ({platform.release()})")
        info(f"Arch        : {platform.machine()}")
        info(f"Python      : {platform.python_version()} ({sys.executable})")
        if self.distro:
            info(f"Distro      : {self.distro}")
        info(f"Repo        : {ROOT}")
        info(f"Build dir   : {BUILD_ROOT}")
        info(f"ESP-IDF     : {self.idf_path}  [{'found' if self.export_script.is_file() else 'missing'}]")
        info(f"IDF tools   : {self.idf_tools}")
        info(f"Board(s)    : {', '.join(BOARDS)}")
        print()

    def system_deps_command(self) -> Optional[List[str]]:
        """Best-effort system package install command for the current OS."""
        if self.is_windows:
            return None  # Git + Python are expected to be installed by the user.
        if self.system == "Darwin":
            return ["brew", "install", "git", "python", "cmake", "ninja"]
        pkgs = "git wget flex bison gperf python3 python3-pip cmake ninja-build libffi-dev libssl-dev libusb-1.0-0-dev libudev-dev"
        mapping = {
            "debian": ["apt-get", "install", "-y"] + pkgs.split(),
            "ubuntu": ["apt-get", "install", "-y"] + pkgs.split(),
            "linuxmint": ["apt-get", "install", "-y"] + pkgs.split(),
            "pop": ["apt-get", "install", "-y"] + pkgs.split(),
            "fedora": ["dnf", "install", "-y", "git", "wget", "flex", "bison", "gperf", "python3", "python3-pip", "cmake", "ninja-build", "libffi-devel", "openssl-devel", "libusb-devel", "systemd-devel"],
            "rhel": ["dnf", "install", "-y", "git", "wget", "flex", "bison", "gperf", "python3", "python3-pip", "cmake", "ninja-build", "libffi-devel", "openssl-devel", "libusb-devel", "systemd-devel"],
            "arch": ["pacman", "-S", "--noconfirm", "git", "wget", "flex", "bison", "gperf", "python", "python-pip", "cmake", "ninja", "libffi", "openssl", "libusb"],
            "manjaro": ["pacman", "-S", "--noconfirm", "git", "wget", "flex", "bison", "gperf", "python", "python-pip", "cmake", "ninja", "libffi", "openssl", "libusb"],
            "alpine": ["apk", "add", "git", "wget", "flex", "bison", "gperf", "python3", "py3-pip", "cmake", "ninja", "libffi-dev", "openssl-dev", "libusb-dev"],
        }
        return mapping.get(self.distro)

    def ensure_prerequisites(self) -> None:
        info("Checking prerequisites...")
        if not shutil.which("git"):
            if self.is_windows:
                warn("git not found. Install it: winget install --id Git.Git -e")
            else:
                cmd = self.system_deps_command()
                if cmd and confirm("git is missing. Install system packages now?", self.assume_yes):
                    self._install_system(cmd)
                else:
                    warn("git is required to download ESP-IDF.")
        else:
            ok("git found")

        if not self.is_windows:
            syscmd = self.system_deps_command()
            if syscmd:
                info("System packages for ESP-IDF: " + " ".join(syscmd))
                if shutil.which("sudo"):
                    info("(prefix the command above with 'sudo' when not running as root)")

        self.ensure_python()

    def _python_version(self, exe: str) -> Optional[tuple]:
        try:
            r = subprocess.run(
                [exe, "-c", "import sys;print('%d.%d' % sys.version_info[:2])"],
                capture_output=True, text=True, timeout=10,
            )
            m = re.match(r"(\d+)\.(\d+)", r.stdout.strip())
            return (int(m.group(1)), int(m.group(2))) if m else None
        except (OSError, subprocess.SubprocessError):
            return None

    @staticmethod
    def _python_supported(v: tuple) -> bool:
        return IDF_PYTHON_MIN <= v <= IDF_PYTHON_MAX

    def _uv_command(self) -> Optional[List[str]]:
        uv = shutil.which("uv")
        if uv:
            return [uv]
        scripts = Path(sysconfig.get_path("scripts"))
        for name in ("uv.exe", "uv"):
            candidate = scripts / name
            if candidate.is_file():
                return [str(candidate)]
        try:
            import uv  # noqa: F401
            return [sys.executable, "-m", "uv"]
        except ImportError:
            return None

    def ensure_python(self) -> None:
        """ESP-IDF 5.5 needs Python <=3.11 on Windows (windows-curses wheels)."""
        v = self._python_version(self.python_exe)
        if v and self._python_supported(v):
            ok(f"Python {v[0]}.{v[1]} OK")
            return
        if v:
            warn(
                f"Python {v[0]}.{v[1]} is not supported by ESP-IDF {IDF_VERSION} "
                f"(need {IDF_PYTHON_MIN[0]}.{IDF_PYTHON_MIN[1]}-{IDF_PYTHON_MAX[0]}.{IDF_PYTHON_MAX[1]})"
            )

        # 1) Already installed compatible interpreter?
        candidates: List[tuple] = []
        uv = self._uv_command()
        if uv:
            found = self._run(
                [*uv, "python", "find", IDF_PYTHON_PREFERRED],
                env=os.environ.copy(), capture=True, check=False,
            )
            path = (found.stdout or "").strip().splitlines()
            if path:
                ver = self._python_version(path[-1].strip())
                if ver and self._python_supported(ver):
                    candidates.append((ver, path[-1].strip()))
        if self.is_windows and shutil.which("py"):
            listing = subprocess.run(["py", "-0p"], capture_output=True, text=True)
            for m in re.finditer(
                r"-V:(\d+)\.(\d+)[^\r\n]*?\s(.+?\.exe)\s*$", listing.stdout, re.MULTILINE
            ):
                ver = (int(m.group(1)), int(m.group(2)))
                if self._python_supported(ver):
                    candidates.append((ver, m.group(3).strip()))
        for name in ("python3.11", "python3.10", "python3.9"):
            found = shutil.which(name)
            if found:
                ver = self._python_version(found)
                if ver and self._python_supported(ver):
                    candidates.append((ver, found))
        if candidates:
            candidates.sort(key=lambda item: item[0], reverse=True)
            ver, exe = candidates[0]
            self.python_exe = exe
            ok(f"Using Python {ver[0]}.{ver[1]}: {exe}")
            return

        # 2) Fetch one via uv (python-build-standalone) — works on all OSes.
        info(f"No compatible Python found; downloading Python {IDF_PYTHON_PREFERRED} via uv ...")
        if not self._uv_command():
            self._run(
                [sys.executable, "-m", "pip", "install", "uv"],
                env=os.environ.copy(), check=False,
            )
        uv = self._uv_command()
        if not uv:
            fail("Could not install uv. Install Python 3.11 manually and re-run `python dbt.py`.")
        self._run(
            [*uv, "python", "install", IDF_PYTHON_PREFERRED],
            env=os.environ.copy(), check=False,
        )
        found = self._run(
            [*uv, "python", "find", IDF_PYTHON_PREFERRED],
            env=os.environ.copy(), capture=True, check=False,
        )
        lines = [line.strip() for line in (found.stdout or "").splitlines() if line.strip()]
        if not lines:
            fail("uv could not provide a Python 3.11 interpreter")
        self.python_exe = lines[-1]
        ok(f"Using downloaded Python: {self.python_exe}")

    def _clean_stale_python_envs(self) -> None:
        """Drop ESP-IDF venvs built with an unsupported Python (e.g. 3.15)."""
        root = self.idf_tools / "python_env"
        if not root.is_dir():
            return
        for d in root.glob("idf*_env"):
            exe = d / ("Scripts/python.exe" if self.is_windows else "bin/python")
            if not exe.is_file():
                continue
            ver = self._python_version(str(exe))
            if ver and not self._python_supported(ver):
                info(f"Removing stale ESP-IDF venv {d.name} (Python {ver[0]}.{ver[1]})")
                shutil.rmtree(d, ignore_errors=True)

    def _install_system(self, cmd: List[str]) -> None:
        full = cmd
        if not self.is_windows and os.geteuid() != 0 and shutil.which("sudo"):
            full = ["sudo"] + cmd
        try:
            self._run(full, env=os.environ.copy())
        except SystemExit:
            warn("Automatic install failed; run the command above manually.")

    def ensure_idf(self) -> None:
        if self.export_script.is_file():
            ok(f"ESP-IDF already installed: {self.idf_path}")
            version_file = self.idf_path / "tools" / "cmake" / "version.cmake"
            if version_file.is_file():
                vtxt = version_file.read_text(encoding="utf-8", errors="ignore")
                major = re.search(r"IDF_VERSION_MAJOR\s+(\d+)", vtxt)
                minor = re.search(r"IDF_VERSION_MINOR\s+(\d+)", vtxt)
                patch = re.search(r"IDF_VERSION_PATCH\s+(\d+)", vtxt)
                found = ".".join(m.group(1) for m in (major, minor, patch) if m)
                if found and found != IDF_VERSION.lstrip("v"):
                    warn(f"This ESP-IDF is {found}, expected {IDF_VERSION}; set IDF_PATH accordingly")
        else:
            if not shutil.which("git"):
                fail("git is required. Run `python dbt.py` first.")
            info(f"Cloning ESP-IDF {IDF_VERSION} into {self.idf_path} ...")
            self.idf_path.parent.mkdir(parents=True, exist_ok=True)
            self._run(
                [
                    "git", "clone", "--depth", "1", "--recurse-submodules",
                    "--shallow-submodules", "-b", IDF_VERSION, IDF_REPO,
                    str(self.idf_path),
                ],
                env=os.environ.copy(),
            )
            ok("ESP-IDF cloned")

        env = os.environ.copy()
        env["IDF_PATH"] = str(self.idf_path)
        env["IDF_TOOLS_PATH"] = str(self.idf_tools)

        self._clean_stale_python_envs()
        python_env = self.idf_tools / "python_env"
        venv_exe_name = "Scripts/python.exe" if self.is_windows else "bin/python"

        def find_venv() -> Optional[Path]:
            for d in python_env.glob("idf*_env"):
                exe = d / venv_exe_name
                if not exe.is_file():
                    continue
                ver = self._python_version(str(exe))
                if ver and self._python_supported(ver):
                    return exe
            return None

        venv_exe = find_venv()
        if not venv_exe:
            info("Installing ESP-IDF tools and Python environment (this can take a while)...")
            tools_py = self.idf_path / "tools" / "idf_tools.py"
            self._run([self.python_exe, str(tools_py), "install"], env=env)
            self._run([self.python_exe, str(tools_py), "install-python-env"], env=env)
            venv_exe = find_venv()
            ok("ESP-IDF tools installed")
        else:
            ok("ESP-IDF tools already installed")
        if venv_exe:
            self.python_exe = str(venv_exe)

        self.load_idf_env()

    def load_idf_env(self) -> None:
        """Run ESP-IDF export script once and reuse the resulting environment."""
        if self._env_loaded:
            return
        if not self.export_script.is_file():
            fail(f"ESP-IDF not installed at {self.idf_path}; run `python dbt.py` first")

        dumper = f'"{self.python_exe}" -c "import os,json;print(json.dumps(dict(os.environ)))"'
        # export.bat requires a `python` on PATH; the IDF venv we just created is
        # the reliable one to expose (the system Python may be too new).
        run_env = os.environ.copy()
        run_env["PATH"] = str(Path(self.python_exe).parent) + os.pathsep + run_env.get("PATH", "")
        if self.is_windows:
            # cmd /c with an inline string mangles the many embedded quotes;
            # a temporary .bat keeps the quoting intact.
            bat = Path(tempfile.gettempdir()) / "dbt_export_env.bat"
            bat.write_text(
                "@echo off\r\n"
                f'set "IDF_PATH={self.idf_path}"\r\n'
                f'set "IDF_TOOLS_PATH={self.idf_tools}"\r\n'
                f'call "{self.export_script}" >nul 2>&1\r\n'
                f"{dumper}\r\n",
                encoding="utf-8",
            )
            try:
                result = subprocess.run(
                    ["cmd", "/c", str(bat)], capture_output=True, text=True,
                    errors="replace", env=run_env
                )
            finally:
                bat.unlink(missing_ok=True)
        else:
            script = (
                f'export IDF_PATH="{self.idf_path}"; '
                f'export IDF_TOOLS_PATH="{self.idf_tools}"; '
                f'. "{self.export_script}" >/dev/null 2>&1; {dumper}'
            )
            result = subprocess.run(
                ["bash", "-c", script], capture_output=True, text=True,
                errors="replace", env=run_env
            )

        payload = ""
        for line in result.stdout.splitlines():
            line = line.strip()
            if line.startswith("{"):
                payload = line
                break
        if payload:
            try:
                env = json.loads(payload)
                same_path = os.path.normcase(os.path.realpath(env.get("IDF_PATH", ""))) == \
                    os.path.normcase(os.path.realpath(str(self.idf_path)))
                if same_path and self._python_from(env):
                    self.env = env
                    self._env_loaded = True
                    ok("ESP-IDF environment loaded")
                    return
            except json.JSONDecodeError:
                pass
        warn("Could not capture the exported environment; using a minimal fallback")
        self.env = os.environ.copy()
        self.env["IDF_PATH"] = str(self.idf_path)
        self.env["IDF_TOOLS_PATH"] = str(self.idf_tools)
        self.env["PATH"] = str(self.idf_tools) + os.pathsep + self.env.get("PATH", "")
        self._env_loaded = True

    def _python_from(self, env: dict) -> str:
        return shutil.which("python", path=env.get("PATH", "")) or self.python_exe

    def _python(self) -> str:
        """Python from the exported IDF environment (has pyserial, etc.)."""
        return self._python_from(self.env)

    def _idf(
        self,
        args: Iterable[str],
        build_dir: Path,
        *,
        options: Iterable[str] = (),
        capture: bool = False,
    ):
        cmd = [self._python(), str(self.idf_path / "tools" / "idf.py"), "-B", str(build_dir)]
        cmd.extend(str(o) for o in options)
        cmd.extend(str(a) for a in args)
        return self._run(cmd, capture=capture)

    # ------------------------------------------------------------------ boards
    @staticmethod
    def resolve_board(name: Optional[str]) -> str:
        if not name:
            return "delphus"
        key = name.strip().lower()
        key = ALIASES.get(key, key)
        if key not in BOARDS:
            fail(f"unknown board {name!r}; available: {', '.join(BOARDS)}")
        return key

    def board_build_dir(self, name: str) -> Path:
        return BUILD_ROOT / name

    def ensure_setup(self) -> None:
        self.ensure_prerequisites()
        self.ensure_idf()

    def build_board(self, name: str) -> Path:
        name = self.resolve_board(name)
        cfg = BOARDS[name]
        build_dir = self.board_build_dir(name)
        build_dir.mkdir(parents=True, exist_ok=True)
        self.load_idf_env()

        info(f"Building {name} -> {build_dir}")
        common = [
            f"-DFLIPPER_BOARD={cfg['board']}",
            f"-DSDKCONFIG_DEFAULTS={cfg['sdkconfig_defaults']}",
        ]
        if not (build_dir / "config" / "sdkconfig.h").is_file():
            self._idf(["set-target", cfg["target"]], build_dir, options=common)
        self._idf(["reconfigure", "build"], build_dir, options=common)

        # Name the merged image after the board so builds are self-describing.
        merged = build_dir / f"{name}_firmware.bin"
        try:
            self._idf(["merge-bin", "-o", str(merged)], build_dir)
            ok(f"Firmware image: {merged}")
        except SystemExit:
            warn("merge-bin failed (the separate binaries in the build dir are still valid)")
        ok(f"Build finished: {build_dir}")
        return build_dir

    def find_port(self) -> str:
        if self.forced_port:
            return self.forced_port
        info("Looking for a serial port...")
        ports: List[dict] = []
        code = (
            "import serial.tools.list_ports,json;print(json.dumps(["
            "{'device':p.device,'vid':p.vid,'pid':p.pid,'description':p.description}"
            " for p in serial.tools.list_ports.comports()]))"
        )
        try:
            result = self._run([self._python(), "-c", code], capture=True, check=False)
            if result.returncode == 0 and result.stdout.strip():
                ports = json.loads(result.stdout.strip().splitlines()[-1])
        except (json.JSONDecodeError, IndexError):
            pass

        if not ports:
            # pyserial unavailable — fall back to filesystem globs.
            patterns = ["/dev/ttyUSB*", "/dev/ttyACM*", "/dev/cu.usbmodem*", "/dev/cu.usbserial*"]
            devices: List[str] = []
            for pattern in patterns:
                devices.extend(str(p) for p in Path("/").glob(pattern.lstrip("/")))
            ports = [{"device": d, "vid": None, "pid": None, "description": ""} for d in devices]

        ports = [p for p in ports if p.get("device")]
        preferred = [p for p in ports if p.get("vid") in KNOWN_USB_VIDS]
        candidates = preferred or ports
        if not candidates:
            fail("No serial port found. Connect the board or pass --port COM3")
        if len(candidates) == 1:
            chosen = candidates[0]["device"]
        else:
            print("Several ports found:")
            for i, p in enumerate(candidates, 1):
                print(f"  {i}) {p['device']}  {p.get('description', '')}")
            if self.assume_yes:
                chosen = candidates[0]["device"]
            else:
                raw = input("Select port number [1]: ").strip() or "1"
                chosen = candidates[int(raw) - 1]["device"]
        ok(f"Using port {chosen}")
        return chosen

    def flash_board(self, name: Optional[str]) -> None:
        build_dir = self.build_board(name)
        port = self.find_port()
        info(f"Flashing {build_dir} to {port}")
        self._idf(["flash"], build_dir, options=["-p", port])
        ok("Flash complete")

    def debug_board(self, name: Optional[str]) -> None:
        build_dir = self.build_board(name)
        port = self.find_port()
        info(f"Flashing {build_dir} to {port}")
        self._idf(["flash"], build_dir, options=["-p", port])
        info("Opening serial monitor (Ctrl+] to exit)")
        self._idf(["monitor"], build_dir, options=["-p", port])

    # -------------------------------------------------------------------- faps
    @staticmethod
    def _fam_value(text: str, key: str) -> str:
        m = re.search(rf'\b{re.escape(key)}\s*=\s*"([^"]*)"', text)
        return m.group(1) if m else ""

    def _fap_includes(self) -> List[str]:
        comp = ROOT / "components"
        paths = [
            comp, ROOT, comp / "furi", comp / "furi" / "core", comp / "mlib",
            comp / "toolbox", comp / "toolbox" / "stream",
            ROOT / "lib" / "toolbox" / "protocols", ROOT / "lib" / "toolbox" / "pulse_protocols",
            comp / "storage", comp / "input", comp / "gui", comp / "gui" / "modules",
            comp / "gui" / "modules" / "widget_elements", comp / "notification",
            comp / "assets", comp / "loader", ROOT / "applications" / "services",
            comp / "flipper_application", comp / "flipper_format", comp / "dialogs",
            comp / "locale", comp / "u8g2", comp / "furi_hal", comp / "furi_hal" / "boards",
            comp / "furi_ble", comp / "bt", comp / "btshim", comp / "subghz",
            comp / "bit_lib", comp / "archive", comp / "nfc", comp / "infrared",
            comp / "lfrfid", comp / "ble_serial", ROOT / "targets", ROOT / "lib" / "subghz",
        ]
        return [f"-I{p}" for p in paths]

    def _fap_idf_includes(self, target: str, build_dir: Path) -> List[str]:
        idfc = self.idf_path / "components"
        common = [
            "newlib/platform_include", "esp_hw_support/include", "esp_hw_support/include/soc",
            "heap/include", "log/include", "esp_rom/include", "esp_common/include",
            "esp_system/include", "esp_timer/include", "esp_event/include",
            "esp_driver_gpio/include", "esp_driver_rmt/include", "esp_driver_spi/include",
            "esp_driver_i2c/include", "esp_driver_uart/include", "esp_ringbuf/include",
            "esp_partition/include", "fatfs/diskio", "fatfs/src", "fatfs/vfs", "vfs/include",
            "wear_levelling/include", "sdmmc/include", "nvs_flash/include",
            "bt/include/esp32c3/include", "bt/host/bluedroid/api/include/api", "lwip/include",
            "lwip/lwip/src/include", "lwip/port/include", "lwip/port/freertos/include",
            "lwip/port/esp32xx/include", "driver/deprecated", "driver/i2c/include",
            "esp_driver_i2s/include", "esp_adc/include", "mbedtls/port/include",
            "mbedtls/mbedtls/include", "esp_lcd/include", "esp_lcd/interface",
            "esp_lcd/rgb/include", "esp_lcd/priv_include",
            "freertos/config/xtensa/include", "freertos/config/include",
            "freertos/config/include/freertos", "freertos/FreeRTOS-Kernel/include",
            "freertos/FreeRTOS-Kernel/portable/xtensa/include",
            "freertos/FreeRTOS-Kernel/portable/xtensa/include/freertos",
            "freertos/esp_additions/include", f"xtensa/{target}/include", "xtensa/include",
            "xtensa/deprecated_include", f"soc/{target}/include", f"soc/{target}/register",
            f"soc/{target}", "soc/include", f"hal/{target}/include", "hal/include",
            "hal/platform_port/include", f"esp_hw_support/port/{target}/.",
            f"esp_hw_support/port/{target}/include", f"esp_rom/esp32s3/include",
            f"esp_rom/include/{target}", f"esp_rom/{target}",
        ]
        paths = [idfc / p for p in common]
        paths.append(build_dir / "config")
        return [f"-I{p}" for p in paths if p.exists()]

    def build_fap(self, app: str) -> Path:
        self.load_idf_env()
        name = self.resolve_board("delphus")
        build_dir = self.board_build_dir(name)
        if not (build_dir / "config" / "sdkconfig.h").is_file():
            info("Firmware not built yet — building it first")
            self.build_board(name)

        app_path = Path(app).expanduser()
        if not app_path.is_absolute() and not app_path.exists():
            app_path = USER_APPS / app
        if not app_path.is_dir():
            fail(f"application directory not found: {app}")

        fam_file = app_path / "application.fam"
        fam = fam_file.read_text(encoding="utf-8", errors="ignore") if fam_file.is_file() else ""
        app_id = self._fam_value(fam, "appid") or app_path.name
        app_name = self._fam_value(fam, "name") or app_id
        entry = os.environ.get("FAP_ENTRY_OVERRIDE") or self._fam_value(fam, "entry_point") or "main"
        icon = app_path / self._fam_value(fam, "fap_icon") if self._fam_value(fam, "fap_icon") else None
        assets = app_path / self._fam_value(fam, "fap_icon_assets") if self._fam_value(fam, "fap_icon_assets") else None
        stack = 16384
        stack_match = re.search(r"\bstack_size\s*=\s*([\d\s*]+)", fam)
        if stack_match:
            expr = stack_match.group(1).replace(" ", "")
            if re.fullmatch(r"\d+(\*\d+)?", expr):
                parts = [int(x) for x in expr.split("*")]
                stack = max(stack, parts[0] * (parts[1] if len(parts) > 1 else 1))
        stack = int(os.environ.get("FAP_STACK_OVERRIDE", stack))
        filename = os.environ.get("FAP_OUTPUT_NAME", f"{app_id}.fap")

        out_dir = BUILD_ROOT / "fap"
        work = out_dir / "obj" / app_id
        work.mkdir(parents=True, exist_ok=True)
        output = out_dir / filename
        info(f"Building FAP {app_name} ({app_id}) -> {output}")

        target = BOARDS[name]["target"]
        prefix = "xtensa-esp32s3-elf"
        cc = shutil.which(f"{prefix}-gcc", path=self.env.get("PATH", ""))
        if not cc:
            fail("Xtensa toolchain not found; run `python dbt.py` first")
        ld = shutil.which(f"{prefix}-ld", path=self.env.get("PATH", ""))
        objcopy = shutil.which(f"{prefix}-objcopy", path=self.env.get("PATH", ""))
        nm = shutil.which(f"{prefix}-nm", path=self.env.get("PATH", ""))

        includes = self._fap_includes() + self._fap_idf_includes(target, build_dir)
        app_includes = [
            f"-I{app_path}", f"-I{app_path / 'helpers'}", f"-I{app_path / 'scenes'}",
            f"-I{app_path / 'views'}", f"-I{app_path / 'protocols'}", f"-I{app_path / 'app'}",
            f"-I{app_path / 'lib'}",
        ]
        if (app_path / "lib").is_dir():
            app_includes.extend(f"-I{p}" for p in (app_path / "lib").iterdir() if p.is_dir())
        for inc in os.environ.get("FAP_EXTRA_INCLUDES", "").split():
            p = Path(inc)
            app_includes.append(f"-I{p if p.is_absolute() else ROOT / inc}")

        # Icon assets
        if assets and assets.is_dir():
            stem = app_id + "_icons"
            for source in app_path.rglob("*"):
                if source.is_file() and source.suffix in (".c", ".h", ".cpp"):
                    m = re.search(r'#include\s+"([^"]+_icons)\.h"', source.read_text(errors="ignore"))
                    if m:
                        stem = m.group(1)
                        break
            gen = work / "icons"
            gen.mkdir(parents=True, exist_ok=True)
            self._run(
                [self._python(), str(ROOT / "tools" / "fam" / "compile_icons.py"),
                 "icons", "--filename", stem, str(assets), str(gen)],
                check=False,
            )
            if (gen / f"{stem}.h").is_file():
                app_includes.append(f"-I{gen}")

        c_flags = [
            "-D_GNU_SOURCE", "-fno-common", "-ffunction-sections", "-fdata-sections",
            "-fno-builtin", "-fno-jump-tables", "-fno-tree-switch-conversion",
            "-std=gnu17", "-Wall", "-Wno-unused-parameter", "-Wno-sign-compare",
            "-Os", "-g", "-DESP_PLATFORM", '-DIDF_VER="v5.5.5"',
            "-DSOC_MMU_PAGE_SIZE=CONFIG_MMU_PAGE_SIZE", "-DSOC_XTAL_FREQ_MHZ=CONFIG_XTAL_FREQ",
            "-mlongcalls", f'-DBOARD_INCLUDE="board_{name}.h"', '-DFAP_VERSION="1.0"',
        ]
        cxx_flags = [f for f in c_flags if f != "-std=gnu17"] + ["-std=gnu++17", "-fno-exceptions", "-fno-rtti"]
        c_flags += os.environ.get("FAP_CDEFINES", "").split()
        cxx_flags += os.environ.get("FAP_CDEFINES", "").split()

        if os.environ.get("FAP_SOURCES"):
            sources = [ROOT / s for s in os.environ["FAP_SOURCES"].split()]
        elif os.environ.get("FAP_SINGLE_SOURCE"):
            sources = [ROOT / os.environ["FAP_SINGLE_SOURCE"]]
        else:
            sources = sorted(app_path.rglob("*.c")) + sorted(app_path.rglob("*.cpp"))
        objects = []
        for src in sources:
            stem = str(src).replace("/", "_").replace("\\", "_").replace(":", "_")
            obj = work / (stem.rsplit(".", 1)[0] + ".o")
            flags = cxx_flags if src.suffix == ".cpp" else c_flags
            compiler = shutil.which(f"{prefix}-g++", path=self.env.get("PATH", "")) if src.suffix == ".cpp" else cc
            self._run([compiler, *flags, *includes, *app_includes, "-c", str(src), "-o", str(obj)])
            objects.append(str(obj))

        if not objects:
            fail(f"no sources found in {app_path}")

        elf = work / "app.elf"
        self._run([ld, "-r", "-T", str(ROOT / "tools" / "fap.ld"), "--entry", entry, "-o", str(elf), *objects])
        manifest = work / "manifest.bin"
        manifest_cmd = [
            self._python(), str(ROOT / "tools" / "fap_manifest.py"),
            "--name", app_name, "--api-major", "1", "--api-minor", "0",
            "--target", "32", "--stack-size", str(stack), "--app-version", "1",
            "--output", str(manifest),
        ]
        if icon and icon.is_file():
            manifest_cmd.extend(["--icon", str(icon)])
        self._run(manifest_cmd)
        self._run([
            objcopy, "--add-section", f".fapmeta={manifest}",
            "--set-section-flags", ".fapmeta=contents,readonly",
            "--strip-debug", str(elf), str(output),
        ])

        undef = work / "undef_syms.txt"
        nm_res = self._run([nm, "-u", str(output)], check=False, capture=True)
        undef.write_text(nm_res.stdout or "", encoding="utf-8")
        self._run(
            [self._python(), str(ROOT / "tools" / "check_fap_symbols.py"),
             str(ROOT / "components" / "flipper_application" / "flipper_application" / "firmware_api.c"),
             str(undef)],
            check=False,
        )
        ok(f"FAP built: {output} ({output.stat().st_size} bytes)")
        return output

    def resources(self) -> None:
        """Assemble the SD-card file tree (and a zip) from app resources."""
        out = BUILD_ROOT / "sd_card"
        if out.exists():
            shutil.rmtree(out)

        # Each applications/main/<app>/resources/ folder already mirrors the
        # SD-card layout (subghz/assets, infrared/assets, badusb/...).
        sources = sorted((ROOT / "applications" / "main").glob("*/resources"))
        for src in sources:
            if not src.is_dir():
                continue
            for path in src.rglob("*"):
                if not path.is_file():
                    continue
                rel = path.relative_to(src)
                dst = out / rel
                dst.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(path, dst)

        # Prebuilt resources (e.g. the fixed-key KeeLoq keystore) override
        # anything copied from application resources above.
        sd_src = ROOT / "sd_resources"
        if sd_src.is_dir():
            for path in sd_src.rglob("*"):
                if not path.is_file():
                    continue
                dst = out / path.relative_to(sd_src)
                dst.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(path, dst)

        # JS example scripts go to /ext/apps/Scripts on the card.
        js_src = ROOT / "applications" / "system" / "js_app" / "examples" / "apps" / "Scripts"
        if js_src.is_dir():
            for path in js_src.rglob("*"):
                if not path.is_file():
                    continue
                dst = out / "apps" / "Scripts" / path.relative_to(js_src)
                dst.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(path, dst)

        # The desktop only checks for the presence of this file.
        manifest = out / "Manifest"
        manifest.parent.mkdir(parents=True, exist_ok=True)
        manifest.write_text("Delphus firmware SD-card resources\n", encoding="utf-8")

        files = [p for p in out.rglob("*") if p.is_file()]
        size = sum(p.stat().st_size for p in files)
        ok(f"SD-card tree: {out} ({len(files)} files, {size / 1024 / 1024:.2f} MB)")

        archive = BUILD_ROOT / "sd_card.zip"
        if archive.exists():
            archive.unlink()
        BUILD_ROOT.mkdir(parents=True, exist_ok=True)
        with zipfile.ZipFile(archive, "w", zipfile.ZIP_DEFLATED) as zf:
            for path in files:
                zf.write(path, path.relative_to(out).as_posix())
        ok(f"SD-card archive: {archive} ({archive.stat().st_size / 1024 / 1024:.2f} MB)")

    def clean(self) -> None:
        """Remove all build outputs (firmware images and FAP plugins)."""
        if not BUILD_ROOT.exists():
            info("Nothing to clean: build/ does not exist")
            return
        size = sum(
            p.stat().st_size for p in BUILD_ROOT.rglob("*") if p.is_file()
        )
        info(f"Removing {BUILD_ROOT} ({size / 1024 / 1024:.1f} MB)")
        shutil.rmtree(BUILD_ROOT, ignore_errors=True)
        ok("Build directory cleaned")

    def build_all_faps(self) -> None:
        if not USER_APPS.is_dir():
            fail(f"{USER_APPS} not found")
        apps = sorted(p for p in USER_APPS.iterdir() if (p / "application.fam").is_file())
        if not apps:
            warn("No applications with application.fam found")
            return
        info(f"Building {len(apps)} user applications")
        built, failed = [], []
        for app in apps:
            try:
                built.append(self.build_fap(app.name))
            except SystemExit:
                failed.append(app.name)
        print()
        info(f"FAP summary: {len(built)} built, {len(failed)} failed")
        if failed:
            warn("Failed: " + ", ".join(failed))
            raise SystemExit(1)


def build_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(
        prog="dbt.py",
        description="Delphus Build Tool — setup, build, flash, debug and FAP build.",
        epilog=(
            "Examples:\n"
            "  python dbt.py                  install ESP-IDF and all dependencies\n"
            "  python dbt.py build            build every board into build/\n"
            "  python dbt.py build delphus    build one board\n"
            "  python dbt.py flash delphus    build + flash (auto port)\n"
            "  python dbt.py debug delphus    build + flash + monitor\n"
            "  python dbt.py fap protopirate  build applications_user/protopirate\n"
            "  python dbt.py faps             build all applications_user apps\n"
            "  python dbt.py clean            remove all built firmware/FAPs from build/\n"
            "  python dbt.py resources        build the SD-card tree and build/sd_card.zip\n"
        ),
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    p.add_argument("--port", help="force serial port for flash/debug")
    p.add_argument("--yes", action="store_true", help="answer yes to install prompts")
    sub = p.add_subparsers(dest="command")
    for cmd in ("build", "flash", "debug"):
        sp = sub.add_parser(cmd)
        sp.add_argument("board", nargs="?")
        if cmd != "build":
            sp.add_argument("--port", default=argparse.SUPPRESS)
        sp.add_argument("--yes", action="store_true", default=argparse.SUPPRESS)
    sub.add_parser("faps").add_argument("--yes", action="store_true", default=argparse.SUPPRESS)
    sub.add_parser("clean")
    sub.add_parser("resources")
    sp = sub.add_parser("fap")
    sp.add_argument("name", help="directory name under applications_user/ or a path")
    sp.add_argument("--yes", action="store_true", default=argparse.SUPPRESS)
    return p


def main() -> int:
    args = build_parser().parse_args()
    assume_yes = args.yes
    port = getattr(args, "port", None)
    tool = DBT(assume_yes=assume_yes, port=port)
    tool.detect_environment()

    if args.command is None:
        tool.ensure_setup()
        ok("Setup complete. Run `python dbt.py build` to build the firmware.")
        return 0

    if args.command == "clean":
        tool.clean()
        return 0

    if args.command == "resources":
        tool.resources()
        return 0

    tool.ensure_setup()

    if args.command == "build":
        names = list(BOARDS) if not args.board else [tool.resolve_board(args.board)]
        if len(names) > 1:
            info(f"Building all boards: {', '.join(names)}")
        for name in names:
            tool.build_board(name)
    elif args.command == "flash":
        tool.flash_board(args.board)
    elif args.command == "debug":
        tool.debug_board(args.board)
    elif args.command == "fap":
        tool.build_fap(args.name)
    elif args.command == "faps":
        tool.build_all_faps()
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except KeyboardInterrupt:
        print("\nInterrupted.", file=sys.stderr)
        sys.exit(130)

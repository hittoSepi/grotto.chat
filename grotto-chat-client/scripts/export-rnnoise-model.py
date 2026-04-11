#!/usr/bin/env python3
from __future__ import annotations

import argparse
import shutil
import subprocess
import sys
import tempfile
from datetime import datetime
from pathlib import Path


def parse_args() -> argparse.Namespace:
    repo_root = Path(__file__).resolve().parent.parent
    parser = argparse.ArgumentParser(
        description=(
            "Export an RNNoise-compatible .pth checkpoint into rnnoise_data.c/.h "
            "for grotto-chat-client."
        )
    )
    parser.add_argument(
        "checkpoint",
        type=Path,
        help="Path to an RNNoise-compatible .pth checkpoint",
    )
    parser.add_argument(
        "--build-dir",
        type=Path,
        default=repo_root / "build",
        help="CMake build directory that already fetched rnnoise-src (default: %(default)s)",
    )
    parser.add_argument(
        "--rnnoise-src",
        type=Path,
        default=None,
        help="Path to the RNNoise source tree; overrides --build-dir/_deps/rnnoise-src",
    )
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=repo_root / "third_party" / "rnnoise-model" / "src",
        help="Destination directory for rnnoise_data.c/.h (default: %(default)s)",
    )
    parser.add_argument(
        "--python",
        dest="python_exe",
        default=sys.executable,
        help="Python executable with PyTorch installed (default: current interpreter)",
    )
    parser.add_argument(
        "--export-filename",
        default="rnnoise_data",
        help="Base filename to export from RNNoise tooling (default: %(default)s)",
    )
    parser.add_argument(
        "--quantize",
        action="store_true",
        help="Pass --quantize to RNNoise's dump_rnnoise_weights.py",
    )
    parser.add_argument(
        "--no-backup",
        action="store_true",
        help="Do not back up existing rnnoise_data.c/.h before replacing them",
    )
    return parser.parse_args()


def resolve_rnnoise_src(args: argparse.Namespace, repo_root: Path) -> Path:
    if args.rnnoise_src is not None:
        return args.rnnoise_src.resolve()

    candidates = [
        args.build_dir.resolve() / "_deps" / "rnnoise-src",
        repo_root / "build-codex-check" / "_deps" / "rnnoise-src",
        repo_root / "build" / "_deps" / "rnnoise-src",
        repo_root / "build-ninja-check" / "_deps" / "rnnoise-src",
    ]
    for candidate in candidates:
        if candidate.exists():
            return candidate
    raise FileNotFoundError(
        "RNNoise source tree was not found. Run CMake once so FetchContent downloads it, "
        "or pass --rnnoise-src explicitly."
    )


def backup_existing_files(output_dir: Path, backup_root: Path) -> None:
    backup_root.mkdir(parents=True, exist_ok=True)
    for name in ("rnnoise_data.c", "rnnoise_data.h"):
        source = output_dir / name
        if source.exists():
            shutil.copy2(source, backup_root / name)


def copy_exported_files(export_dir: Path, output_dir: Path) -> None:
    output_dir.mkdir(parents=True, exist_ok=True)
    for name in ("rnnoise_data.c", "rnnoise_data.h"):
        source = export_dir / name
        if not source.exists():
            raise FileNotFoundError(f"Expected exported file was not created: {source}")
        shutil.copy2(source, output_dir / name)


def main() -> int:
    args = parse_args()
    repo_root = Path(__file__).resolve().parent.parent
    checkpoint = args.checkpoint.expanduser().resolve()
    output_dir = args.output_dir.expanduser().resolve()

    if checkpoint.suffix.lower() != ".pth":
        print(
            f"warning: expected an RNNoise-style .pth checkpoint, got {checkpoint.name}",
            file=sys.stderr,
        )
    if not checkpoint.exists():
        raise FileNotFoundError(f"Checkpoint was not found: {checkpoint}")

    rnnoise_src = resolve_rnnoise_src(args, repo_root)
    exporter = rnnoise_src / "torch" / "rnnoise" / "dump_rnnoise_weights.py"
    if not exporter.exists():
        raise FileNotFoundError(f"RNNoise exporter script was not found: {exporter}")

    command = [
        args.python_exe,
        str(exporter),
        str(checkpoint),
    ]

    with tempfile.TemporaryDirectory(prefix="grotto-rnnoise-export-") as temp_dir_str:
        temp_dir = Path(temp_dir_str)
        command.append(str(temp_dir))
        command.extend(["--export-filename", args.export_filename])
        if args.quantize:
            command.append("--quantize")

        print(f"checkpoint: {checkpoint}")
        print(f"rnnoise src: {rnnoise_src}")
        print(f"output dir : {output_dir}")
        print(f"python     : {args.python_exe}")
        print(f"export cmd : {' '.join(command)}")

        result = subprocess.run(command, check=False)
        if result.returncode != 0:
            print(
                "RNNoise export failed. Ensure that the selected Python has PyTorch installed "
                "and that the checkpoint is a real RNNoise .pth file.",
                file=sys.stderr,
            )
            return result.returncode

        exported_dir = temp_dir
        if args.export_filename != "rnnoise_data":
            exported_files_dir = temp_dir
        else:
            exported_files_dir = exported_dir

        if not args.no_backup:
            timestamp = datetime.now().strftime("%Y%m%d-%H%M%S")
            backup_dir = repo_root / "third_party" / "rnnoise-model" / "backups" / timestamp
            backup_existing_files(output_dir, backup_dir)
            if backup_dir.exists():
                print(f"backup     : {backup_dir}")

        copy_exported_files(exported_files_dir, output_dir)

    print("export completed")
    print(f"updated    : {output_dir / 'rnnoise_data.c'}")
    print(f"updated    : {output_dir / 'rnnoise_data.h'}")
    print("next step  : rebuild grotto-client so the new RNNoise weights are linked in")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

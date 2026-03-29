#!/usr/bin/env bash
set -euo pipefail

build_dir="build"
backup_dir="${build_dir}-old"

if [[ -d "$build_dir" ]]; then
  idx=1
  while [[ -e "$backup_dir" ]]; do
    backup_dir="${build_dir}-old-${idx}"
    ((idx++))
  done

  echo "Renaming '$build_dir' to '$backup_dir'..."
  mv "$build_dir" "$backup_dir"
fi

echo "Configuring CMake..."
cmake -S . -B "$build_dir" -DCMAKE_BUILD_TYPE=Release

echo "Building..."
cmake --build "$build_dir"

echo "Build completed successfully."

# Custom RNNoise Model Workflow

`grotto-chat-client` links RNNoise weights from:

- [third_party/rnnoise-model/src/rnnoise_data.c](../third_party/rnnoise-model/src/rnnoise_data.c)
- [third_party/rnnoise-model/src/rnnoise_data.h](../third_party/rnnoise-model/src/rnnoise_data.h)

That means a custom model only works if it can be exported into the same RNNoise C-weight format.

## What Works

Supported input is an RNNoise-style PyTorch checkpoint:

- file type: `.pth`
- format: `torch.load(...)` dictionary
- contains:
  - `model_args`
  - `model_kwargs`
  - `state_dict`
- architecture matches RNNoise's own PyTorch model:
  - `conv1`
  - `conv2`
  - `gru1`
  - `gru2`
  - `gru3`
  - `dense_out`
  - `vad_dense`

Example:

- `rnnoise8Za_25.pth` from the upstream RNNoise model tarball

## What Does Not Work

These files cannot be dropped into the current C runtime:

- TorchScript traced `.pt` models
- non-RNNoise denoisers with different layer layouts
- generic CNN/U-Net/LSTM denoisers unless you add a separate runtime backend

Example:

- `traced_denoiser_model.pt` with `LightweightDenoiseCNN`

## Export Script

Use:

```bash
python scripts/export-rnnoise-model.py /path/to/custom-rnnoise-model.pth
```

On Windows with a specific Python environment:

```powershell
python .\scripts\export-rnnoise-model.py C:\models\custom-rnnoise.pth --build-dir .\build
```

The script:

1. Locates the fetched RNNoise source tree from your build directory
2. Runs RNNoise's `dump_rnnoise_weights.py`
3. Backs up the current `rnnoise_data.c/.h`
4. Replaces the vendored model sources in `third_party/rnnoise-model/src`

Backups are written to:

```text
third_party/rnnoise-model/backups/<timestamp>/
```

## Requirements

Before running the export:

1. Run CMake once so `rnnoise-src` exists under `_deps`
2. Use a Python environment with PyTorch installed
3. Make sure the checkpoint is RNNoise-compatible

Example build prep:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT\scripts\buildsystems\vcpkg.cmake"
```

## Rebuild After Export

After replacing `rnnoise_data.c/.h`, rebuild the client:

```powershell
cmake --build build --config Release --target grotto-client
```

Or on Ninja/Linux:

```bash
cmake --build build
```

## Notes

- The current default model in this repo is the stock upstream RNNoise model.
- Exporting the stock `rnnoise8Za_25.pth` produces the same effective model already in use.
- If your custom checkpoint sounds wrong, the problem is most likely the model training data or training regime, not the C integration path.

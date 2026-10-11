# Strata engine (local LLM inference)

[English](strata.md) | [Русский](strata_RU.md)

Location: `/home/nexonsu/.strata/`

**Strata** is a local LLM inference engine (custom C++/CUDA build,
GGUF/llama.cpp format compatible) running entirely on local hardware. The
server exposes an OpenAI-compatible API on port `8080`, which the agent
(OpenCode) connects to.

## Launch

Entry point — `run-iq3_xxs.sh` (copy in this directory):

```sh
#!/bin/sh
cd "/home/nexonsu/.strata"
exec "/home/nexonsu/.strata/.venv/bin/python" "/home/nexonsu/.strata/serve/server.py" \
  --engine strata --config /home/nexonsu/.strata/strata-iq3_xxs.json --port 8080 --open
```

The Python wrapper `serve/server.py` starts the native `engine/strata`
binary with parameters from `strata-iq3_xxs.json`.

## Model configuration (`strata-iq3_xxs.json`)

| Parameter | Value | Purpose |
|---|---|---|
| `--native` | `Qwen3.8-Flash-Next-GSQ-RCO-IQ3_XXS-00001-of-00002.gguf` (+ second part) | model weights, **IQ3_XXS** quantization |
| `--pack` | `Strata-data/packs/iq3_xxs` | quantization pack + tokenizer |
| `--ple-gguf` | second GGUF part | per-layer embeddings stored separately |
| `--expert-profile` / `--expert-cache auto` | `data/expert-profile.bin` | MoE expert profiling and caching |
| `--spec 4`, `--spec-min-p 0.5`, `--mtp` | speculative decoding | MTP model (`Strata-data/mtp/rt`), up to 4 tokens per pass |
| `--max-context 131072` | 128K context | model window |
| `--kv int8`, `--kv-resident 32768` | int8 KV cache | VRAM savings |
| `--vision` | enabled | image support (the agent inspected game screenshots) |
| `--vram-reserve-mib 700` | VRAM reserve | kept for the desktop |
| `lib_dirs` | `/opt/cuda/...` | CUDA runtime |

Model name exposed via the API: **`qwen3.8-flash-next-iq3_xxs`**.

## Hardware

- **GPU:** NVIDIA GeForce RTX 5090 (GB202) — runs the model
- **CPU:** AMD Ryzen 9 9950X3D (16 cores / 32 threads)
- **RAM:** 123 GiB
- **OS:** CachyOS x86_64 (Arch-based), kernel 7.2.9-2-cachyos, KDE Plasma 6.7.5 (KWin/Wayland)

Full system snapshot: [`fastfetch.txt`](fastfetch.txt).

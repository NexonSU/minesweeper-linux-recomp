# Движок Strata (локальный инференс LLM)

[English](strata.md) | [Русский](strata_RU.md)

Расположение: `/home/nexonsu/.strata/`

**Strata** — локальный движок инференса LLM (собственная сборка, C++/CUDA,
совместимый с форматом GGUF/llama.cpp), запущенный полностью на локальном
железе. Сервер поднимает OpenAI-совместимый API на порту `8080`, к которому
подключается агент (OpenCode).

## Запуск

Точка входа — `run-iq3_xxs.sh` (копия в этом каталоге):

```sh
#!/bin/sh
cd "/home/nexonsu/.strata"
exec "/home/nexonsu/.strata/.venv/bin/python" "/home/nexonsu/.strata/serve/server.py" \
  --engine strata --config /home/nexonsu/.strata/strata-iq3_xxs.json --port 8080 --open
```

Python-обёртка `serve/server.py` стартует нативный бинарь `engine/strata`
с параметрами из `strata-iq3_xxs.json`.

## Конфигурация модели (`strata-iq3_xxs.json`)

| Параметр | Значение | Назначение |
|---|---|---|
| `--native` | `Qwen3.8-Flash-Next-GSQ-RCO-IQ3_XXS-00001-of-00002.gguf` (+ вторая часть) | веса модели, квантование **IQ3_XXS** |
| `--pack` | `Strata-data/packs/iq3_xxs` | пакет квантования + токенизатор |
| `--ple-gguf` | вторая часть GGUF | отдельные слои (per-layer embeddings) |
| `--expert-profile` / `--expert-cache auto` | `data/expert-profile.bin` | профилирование и кэш экспертов MoE |
| `--spec 4`, `--spec-min-p 0.5`, `--mtp` | спекулятивное декодирование | MTP-модель (`Strata-data/mtp/rt`), до 4 токенов за проход |
| `--max-context 131072` | 128K контекст | окно модели |
| `--kv int8`, `--kv-resident 32768` | KV-кэш в int8 | экономия VRAM |
| `--vision` | включён | поддержка изображений (агент анализировал скриншоты игры) |
| `--vram-reserve-mib 700` | резерв VRAM | под нужды рабочего стола |
| `lib_dirs` | `/opt/cuda/...` | CUDA-рантайм |

Имя модели в API: **`qwen3.8-flash-next-iq3_xxs`**.

## Железо

- **GPU:** NVIDIA GeForce RTX 5090 (GB202) — на ней крутится модель
- **CPU:** AMD Ryzen 9 9950X3D (16 ядер / 32 потока)
- **RAM:** 123 GiB
- **ОС:** CachyOS x86_64 (Arch-based), ядро 7.2.9-2-cachyos, KDE Plasma 6.7.5 (KWin/Wayland)

Полный снимок системы: [`fastfetch.txt`](fastfetch.txt).

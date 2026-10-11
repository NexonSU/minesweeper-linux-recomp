# Minesweeper Linux Recomp

[English](README.md) | [Русский](README_RU.md)

**A PoC of decompiling and porting the game Minesweeper (`WINMINE.EXE`,
Windows XP era) to Linux by a locally running LLM, with no external
services involved.**

The goal of the experiment: can a locally hosted model fully dissect a
native Windows binary, reconstruct its logic, and produce an independent
implementation that reproduces the original — including behavior, assets,
and even its quirks.

> **Unofficial fan port.** This project is not affiliated with or endorsed
> by Microsoft. "Minesweeper" is a trademark of Microsoft Corporation.
> File-status disclaimer: see [`NOTICE`](NOTICE).

## First run of the port

![First run](docs/first-run.png)

First-run screenshot: OpenCode session on the left, the port's window on
the right (Expert 30×16/99, Game menu open) next to the original
`WINMINE.EXE`.

## How it was made

1. **Reverse engineering.** Through the REA MCP server (`rea-agents`) the
   model decompiled `WINMINE.EXE` in Ghidra: ~150 functions recovered into
   pseudocode, the EXE's resources parsed (cell/digit/face sprites,
   dialogs, menus, WAV sounds), original assets extracted into `assets/`.
2. **Data-model reconstruction.** From the pseudocode: the
   `DAT_01005340[32*27]` grid with its border ring, cell states (flag,
   question, mine, exploded), counters, first-click-is-safe logic,
   flood-fill through a 100-slot ring queue (exactly like the original),
   chords, pause, the XYZZY code.
3. **Porting.** Independent code written in C11 + SDL2: custom menu bar,
   Win95-style modal dialogs (Custom Field, Best Times, About, name entry),
   persistence in `~/.config/winmine/winmine.cfg`.  PNG sprites are decoded
   with vendored `stb_image`; menu/dialog text is drawn from a pre-rendered
   DejaVuSans glyph atlas (dumped offline with FreeType, pixel-identical to
   SDL2_ttf's output) embedded in the binary — no SDL2_image/SDL2_ttf or
   font files are needed at runtime.
4. **Verification.** Port behavior compared against the original running
   under wine: dialog screenshots, click/counter tracing, quirk checks
   (F4 does not persist sound, Custom changes difficulty even on Cancel,
   Marks checkmarks, flag cycle with marks disabled).

## Environment

| | |
|---|---|
| **OS** | CachyOS x86_64 (Arch-based), Linux kernel 7.2.9-2-cachyos |
| **Desktop** | KDE Plasma 6.7.5, KWin (Wayland) |
| **CPU** | AMD Ryzen 9 9950X3D (16C/32T, up to 5.76 GHz) |
| **GPU** | NVIDIA GeForce RTX 5090 (discrete, GB202) + AMD Radeon 610M (integrated) |
| **RAM** | 123.45 GiB |
| **Disks** | btrfs: `/` 931 GiB, `/home` 2.73 TiB |
| **Monitors** | 3840×2160 ViewSonic PJ + PG32UCDP 32" 240 Hz |
| **Model** | `qwen3.8-flash-next-iq3_xxs` — Qwen3.8-Flash-Next, IQ3_XXS quantization (GGUF, MoE, MTP speculation, vision) |
| **Engine** | [Strata](docs/strata.md) — local inference server (`/home/nexonsu/.strata/`, launched via `run-iq3_xxs.sh`, OpenAI-compatible API on :8080) |
| **Agent** | [OpenCode](docs/opencode.md) + MCP REA (Ghidra) |

Full system snapshot: [`docs/fastfetch.txt`](docs/fastfetch.txt).

All of the work — decompilation, code writing, building, debugging and
visual verification — was performed autonomously by the model inside an
OpenCode session.

## Build

The default build bundles all dependencies into the binary: it statically
links SDL2 2.32.10 built from source, decodes PNGs with vendored `stb_image`
and renders text from the embedded glyph atlas.  The resulting `winmine`
needs no SDL/font packages installed (X11/Wayland and ALSA/PulseAudio
backends are loaded dynamically at runtime).

```sh
make deps     # fetch + build static SDL2 2.32.10 (needs curl, cmake, X11 dev headers)
make          # -> ./winmine (~2.5 MB, statically linked)
./winmine
```

Quick alternative linked against the system shared SDL2 (only the `sdl2`
pkg-config package required):

```sh
make sys
```

`assets/` must sit next to the binary (resolved via `/proc/self/exe`, else
`./assets` relative to cwd); `WINMINE.HLP` lives there too, used by the Help
menu entry.

## Layout

```
src/          port sources (C11 + SDL2)
tools/        offline glyph-atlas dumper (FreeType), DejaVu subset + license
third_party/  vendored stb_image (SDL2 source is fetched by `make deps`)
assets/       sprites, WAVs, icon, HLP extracted from the original
docs/         Strata engine and OpenCode configuration descriptions, screenshots
Makefile      build
```

## License

The project's original code (`src/`, `Makefile`, documentation) is under
[MIT](LICENSE). Game mechanics were reproduced independently (clean-room);
the original `WINMINE.EXE` and its code are not present in this repository.

Files in `assets/` are extracted from `WINMINE.EXE`, remain Microsoft
property and are **not** licensed under MIT — see [`NOTICE`](NOTICE).

Third-party components: `third_party/stb/` (stb_image, public domain);
`tools/DejaVuSans-subset.ttf` (DejaVu Sans subset under the Bitstream Vera
license, see `tools/DejaVuSans-LICENSE.txt`) whose pre-rendered glyphs are
embedded into the binary as `src/font_atlas.h`; SDL2 (zlib license) is
fetched and built locally by `make deps` and is not part of the repository.

# OpenCode configuration

[English](opencode.md) | [Русский](opencode_RU.md)

File: `/home/nexonsu/.config/opencode/opencode.json` (copy in this directory)

**OpenCode** is the agent environment (TUI) in which the local model did
all the decompilation and porting work.

## The `Strata` provider

```json
"Strata": {
  "npm": "@ai-sdk/openai-compatible",
  "name": "Strata (Local)",
  "options": { "baseURL": "http://127.0.0.1:8080/v1" },
  "models": {
    "qwen3.8-flash-next-iq3_xxs": {
      "limit": { "context": 100000, "output": 50000 }
    }
  }
}
```

- Connects to the local Strata engine through an OpenAI-compatible API
  (the machine's LAN address, port 8080).
- Working model: `qwen3.8-flash-next-iq3_xxs` (100K input context,
  50K output tokens). `orcarouter-...-uncensored-iq3_xxs` is also declared.

## Context compaction

```json
"compaction": { "auto": true, "keep": { "tokens": 10000 }, "buffer": 10000 }
```

Automatic history compaction for long sessions (the porting session ran
for hours and was compacted several times).

## The `rea` MCP server — reverse-engineering tools

```json
"mcp": {
  "rea": {
    "type": "local",
    "command": ["npx", "-y", "rea-agents@6.3.0", "mcp"],
    "environment": {
      "GHIDRA_INSTALL_DIR": "/opt/ghidra",
      "JAVA_HOME": "/usr/lib/jvm/java-27-openjdk"
    }
  }
}
```

REA (Reverse Engineering Agents) gave the model direct access to
**Ghidra**: decompiling `WINMINE.EXE` into pseudocode, parsing resources
(bitmaps, dialogs, menus), extracting sprites and WAVs. All reverse-engineering
notes this port is built on came from it.

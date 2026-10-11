# Конфигурация OpenCode

[English](opencode.md) | [Русский](opencode_RU.md)

Файл: `/home/nexonsu/.config/opencode/opencode.json` (копия в этом каталоге)

**OpenCode** — агентная среда (TUI), в которой локальная модель вела всю
работу по декомпиляции и портированию.

## Провайдер `Strata`

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

- Подключение к локальному движку Strata по OpenAI-совместимому API
  (адрес машины в локальной сети, порт 8080).
- Рабочая модель: `qwen3.8-flash-next-iq3_xxs` (100K входного контекста,
  50K выходных токенов). Также объявлена `orcarouter-...-uncensored-iq3_xxs`.

## Компакция контекста

```json
"compaction": { "auto": true, "keep": { "tokens": 10000 }, "buffer": 10000 }
```

Автоматическое сжатие истории длинных сессий (сессия портирования шла
часами и несколько раз компактировалась).

## MCP-сервер `rea` — инструменты реверс-инжиниринга

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

REA (Reverse Engineering Agents) дала агенту прямой доступ к **Ghidra**:
декомпиляция `WINMINE.EXE` в псевдокод, разбор ресурсов (битмапы, диалоги,
меню), извлечение спрайтов и WAV. Именно через REA были получены все
заметки реверса, на которых построен этот порт.

#!/bin/sh
cd "/home/nexonsu/.strata"
exec "/home/nexonsu/.strata/.venv/bin/python" "/home/nexonsu/.strata/serve/server.py" "--engine" "strata" "--config" "/home/nexonsu/.strata/strata-iq3_xxs.json" "--port" "8080" "--open"

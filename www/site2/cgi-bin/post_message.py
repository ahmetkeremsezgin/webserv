#!/usr/bin/env python3
import sys, os, json, time

print("Content-Type: application/json")
print("Access-Control-Allow-Origin: *")
print()

path = "./www/site2/messages.json"

# stdin'den gelen body'yi oku (CONTENT_LENGTH kadar)
length = int(os.environ.get("CONTENT_LENGTH", 0))
body = sys.stdin.read(length) if length > 0 else ""

try:
    msg = json.loads(body)
    text = msg.get("text", "").strip()
except Exception:
    text = ""

if text:
    try:
        with open(path, "r") as f:
            content = f.read().strip()
        messages = json.loads(content) if content else []
    except Exception:
        messages = []

    messages.append({
        "text": text,
        "time": time.strftime("%H:%M")
    })

    with open(path, "w") as f:
        json.dump(messages, f)

    sys.stdout.write(json.dumps({"status": "ok"}))
else:
    sys.stdout.write(json.dumps({"status": "empty"}))

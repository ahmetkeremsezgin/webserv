#!/usr/bin/env python3
import sys

print("Content-Type: application/json")
print("Access-Control-Allow-Origin: *")
print()

try:
    with open("./www/site2/messages.json", "r") as f:
        data = f.read()
    if not data.strip():
        data = "[]"
    sys.stdout.write(data)
except Exception:
    sys.stdout.write("[]")

import sys
import os
import json

if len(sys.argv) != 3:
    print(f"Usage: {sys.argv[0]} <input.json> <output.c>")
    sys.exit(1)

input_path = sys.argv[1]
output_path = sys.argv[2]

with open(input_path, 'r', encoding='utf-8') as f:
    data = f.read()

# Optionally, minify JSON to save space
try:
    minified = json.dumps(json.loads(data), separators=(",", ":"))
except Exception:
    minified = data

c_array = ','.join(str(ord(c)) for c in minified)

header = "#include \"diagnostic_schema_embed.h\"\n\n"
array_decl = f"const char ZT_DIAGNOSTIC_SCHEMA_JSON[] = \"{minified.replace('\\', '\\\\').replace('"', '\\"').replace(chr(10), '\\n').replace(chr(13), '')}\";\n"
len_decl = f"const unsigned int ZT_DIAGNOSTIC_SCHEMA_JSON_LEN = sizeof(ZT_DIAGNOSTIC_SCHEMA_JSON) - 1;\n"

with open(output_path, 'w', encoding='utf-8') as out:
    out.write(header)
    out.write(array_decl)
    out.write(len_decl) 
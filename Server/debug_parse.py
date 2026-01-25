import sys
import os

filepath = r"c:\Users\forme\OneDrive\Desktop\Kriti 2026\test_case1.txt"

print(f"File exists: {os.path.exists(filepath)}")
print(f"File size: {os.path.getsize(filepath)} bytes")

with open(filepath, 'r', encoding='utf-8') as f:
    content = f.read()

print(f"Read {len(content)} chars")
print(f"\n=== Content preview (first 500 chars) ===")
print(repr(content[:500]))

sections = content.split('\n\n')
print(f"\n=== Found {len(sections)} sections ===")

for i, section_text in enumerate(sections):
    lines = section_text.strip().split('\n')
    if lines and lines[0].strip():
        header = lines[0].strip().split('\t')
        print(f"\nSection {i+1}:")
        print(f"  First header field: '{header[0]}'")
        print(f"  Header length: {len(header)}")
        print(f"  Number of data lines: {len(lines)-1}")
        if len(header) > 1:
            print(f"  Second field: '{header[1]}'")

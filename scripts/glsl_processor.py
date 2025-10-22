# Copyright (c) 2025 Le Juez Victor
#
# This software is provided "as-is", without any express or implied warranty. In no event
# will the authors be held liable for any damages arising from the use of this software.
#
# Permission is granted to anyone to use this software for any purpose, including commercial
# applications, and to alter it and redistribute it freely, subject to the following restrictions:
#
#   1. The origin of this software must not be misrepresented; you must not claim that you
#   wrote the original software. If you use this software in a product, an acknowledgment
#   in the product documentation would be appreciated but is not required.
#
#   2. Altered source versions must be plainly marked as such, and must not be misrepresented
#   as being the original software.
#
#   3. This notice may not be removed or altered from any source distribution.

#!/usr/bin/env python3

import sys, re, zlib, struct, argparse
from pathlib import Path

# === Processing Passes === #

def process_includes(shader_content, base_path, included_files=None):
    """Recursively resolve #include directives in GLSL shader code."""
    if included_files is None:
        included_files = set()

    base_path = Path(base_path)
    include_pattern = re.compile(r'^\s*#include\s+"([^"]+)"', re.MULTILINE)

    def replacer(match):
        file_path = (base_path / match.group(1)).resolve()

        if file_path in included_files:
            return ""

        if not file_path.is_file():
            print(f"Include not found: {file_path}", file=sys.stderr)
            return ""

        included_files.add(file_path)
        content = file_path.read_text(encoding="utf-8")
        return process_includes(content, file_path.parent, included_files) + "\n"

    return include_pattern.sub(replacer, shader_content)

def remove_comments(shader_content):
    """Remove C-style comments"""
    shader_content = re.sub(r'/\*.*?\*/', '', shader_content, flags=re.DOTALL)
    shader_content = re.sub(r'//.*?(?=\n|$)', '', shader_content)
    return shader_content

def remove_newlines(shader_content):
    """Remove unnecessary newlines while preserving preprocessor directive spacing"""
    lines = [line for line in shader_content.splitlines() if line.strip()]
    if not lines:
        return ""

    result = []
    for i, line in enumerate(lines):
        is_directive = line.lstrip().startswith('#')
        prev_directive = i > 0 and lines[i-1].lstrip().startswith('#')
        next_directive = i < len(lines)-1 and lines[i+1].lstrip().startswith('#')
        if i > 0 and (is_directive or prev_directive or next_directive):
            result.append("\n")
        result.append(line)

    return "".join(result)

def normalize_spaces(shader_content):
    """Remove redundant spaces around operators and symbols, excluding preprocessor directives"""
    lines = shader_content.split('\n')
    processed_lines = []

    symbols = [
        ',', '.', '(', ')', '{', '}', ';', ':',
        '+', '-', '*', '/', '=', '<', '>',
        '!', '?', '|', '&'
    ]

    for line in lines:
        if line.lstrip().startswith('#'):
            line = re.sub(r'^\s*#', '#', line)      # Remove spaces before the '#'
            line = re.sub(r'[ \t]+', ' ', line)     # Replace consecutive spaces to one
            processed_lines.append(line)
        else:
            # Apply normalization to other lines
            processed_line = line
            for symbol in symbols:
                escaped = re.escape(symbol)
                processed_line = re.sub(rf'[ \t]+{escaped}', symbol, processed_line)
                processed_line = re.sub(rf'{escaped}[ \t]+', symbol, processed_line)

            processed_line = re.sub(r'[ \t]+', ' ', processed_line)
            processed_lines.append(processed_line)

    return '\n'.join(processed_lines)

def optimize_float_literals(shader_content):
    """Optimize float literal notation"""
    shader_content = re.sub(r'\b(\d+)\.0+(?!\d)', r'\1.', shader_content)   # 1.000 -> 1.
    shader_content = re.sub(r'\b0\.([1-9]\d*)\b', r'.\1', shader_content)   # 0.5 -> .5  (but no 0.0 -> .0)
    return shader_content

# === Main === #

def process_shader(filepath):
    """Process a shader file through all optimization passes"""
    filepath = Path(filepath)

    try:
        with open(filepath, 'r', encoding='utf-8') as f:
            shader_content = f.read()
    except FileNotFoundError:
        print(f"Error: File not found: {filepath}", file=sys.stderr)
        sys.exit(1)
    except Exception as e:
        print(f"Error reading file: {e}", file=sys.stderr)
        sys.exit(1)

    shader_content = process_includes(shader_content, filepath.parent)
    shader_content = remove_comments(shader_content)
    shader_content = remove_newlines(shader_content)
    shader_content = normalize_spaces(shader_content)
    shader_content = optimize_float_literals(shader_content)

    return shader_content

def compress_shader(shader_content):
    """Compresses the content with zlib (DEFLATE) then encodes in base64"""
    shader_bytes = shader_content.encode('utf-8')
    uncompressed_size = len(shader_bytes)
    shader_compressed = zlib.compress(shader_bytes)
    header = struct.pack('<Q', uncompressed_size)
    return header + shader_compressed

def main():
    parser = argparse.ArgumentParser(description="Process and optionally compress a GLSL shader file.")
    parser.add_argument("shader_path", help="Path to the input shader file")
    parser.add_argument("output_file", nargs="?", help="Output file path (optional, defaults to stdout)")
    parser.add_argument("--compress", "-c", action="store_true", help="Compress the shader output (binary mode)")
    args = parser.parse_args()

    if args.compress and not args.output_file:
        sys.exit("Error: Cannot output compressed data to stdout. Please specify an output file.")

    formatted_shader = process_shader(args.shader_path)

    if args.compress:
        formatted_shader = compress_shader(formatted_shader)

    try:
        if args.output_file:
            mode = 'wb' if args.compress else 'w'
            with open(args.output_file, mode, encoding=None if args.compress else 'utf-8') as f:
                f.write(formatted_shader)
        else:
            print(formatted_shader, end="")
    except OSError as e:
        sys.exit(f"Error writing to output: {e}")

if __name__ == "__main__":
    main()

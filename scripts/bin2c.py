# Copyright (c) 2025-2026 Le Juez Victor
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

import sys, os, argparse

def to_identifier(name):
    """Convert filename to valid C identifier"""
    return name.upper().replace('.', '_').replace('-', '_')

def escape_c_string(text):
    """Escape special characters for C string literals"""
    text = text.replace('\\', '\\\\')  # Double backslashes
    text = text.replace('"', '\\"')    # Escape quotes
    text = text.replace('\n', '\\n')   # Convert actual newlines to \n
    text = text.replace('\r', '\\r')   # Convert carriage returns
    text = text.replace('\t', '\\t')   # Convert tabs
    return text

def write_header_from_file(file_path, out_path, mode='binary', custom_name=None):
    """Convert a file into a C header"""
    if custom_name:
        array_name = to_identifier(custom_name)
        guard = array_name + '_H'
    else:
        name = os.path.basename(file_path)
        guard = to_identifier(name) + '_H'
        array_name = to_identifier(name)

    if mode == 'text':
        with open(file_path, 'r', encoding='utf-8') as f:
            text = f.read()
        data = text.encode('utf-8')
    else:
        with open(file_path, 'rb') as f:
            data = f.read()

    write_data_to_header(data, out_path, guard, array_name, mode)

def write_header_from_string(input_string, array_name, out_path, mode='binary'):
    """Convert a string into a C header"""
    guard = to_identifier(array_name) + '_H'
    array_name = to_identifier(array_name)

    if mode == 'text':
        try:
            interpreted_string = input_string.encode().decode('unicode_escape')
            data = interpreted_string.encode('utf-8')
        except UnicodeDecodeError:
            data = input_string.encode('utf-8')
    else:
        data = input_string.encode('utf-8')

    write_data_to_header(data, out_path, guard, array_name, mode)

def write_header_from_stdin(array_name, out_path, mode='binary'):
    """Convert stdin input into a C header"""
    guard = to_identifier(array_name) + '_H'
    array_name = to_identifier(array_name)

    input_string = sys.stdin.read()

    if mode == 'text':
        try:
            interpreted_string = input_string.encode().decode('unicode_escape')
            data = interpreted_string.encode('utf-8')
        except UnicodeDecodeError:
            data = input_string.encode('utf-8')
    else:
        data = input_string.encode('utf-8')

    write_data_to_header(data, out_path, guard, array_name, mode)

def write_data_to_header(data, out_path, guard, array_name, mode='binary'):
    """Write data to C header in text or binary format"""

    with open(out_path, 'w') as f:
        f.write(f"#ifndef {guard}\n")
        f.write(f"#define {guard}\n\n")

        f.write("#ifdef __cplusplus\n")
        f.write("extern \"C\" {\n")
        f.write("#endif\n\n")

        if mode == 'text':
            write_text_array(f, data, array_name)
        else:
            write_binary_array(f, data, array_name)

        f.write("#ifdef __cplusplus\n")
        f.write("}\n")
        f.write("#endif\n\n")

        f.write(f"#endif // {guard}\n")

def write_text_array(f, data, array_name):
    """Write data as single-line C string literal"""
    try:
        text = data.decode('utf-8')
    except UnicodeDecodeError:
        write_binary_array(f, data, array_name)
        return

    text_size = len(data)
    escaped_text = escape_c_string(text)

    f.write(f'static const char {array_name}[] = "{escaped_text}";\n\n')
    f.write(f"#define {array_name}_SIZE {text_size}\n\n")

def write_binary_array(f, data, array_name):
    """Write data as binary byte array"""
    if not data.endswith(b'\0'):
        data = data + b'\0'

    data_size = len(data) - 1

    f.write(f"static const unsigned char {array_name}[] =\n")
    f.write("{\n")

    for i in range(0, len(data), 16):
        chunk = data[i:i+16]
        f.write("    ")
        hex_values = [f"0x{byte:02X}" for byte in chunk]
        f.write(", ".join(hex_values))
        if i + 16 < len(data):
            f.write(",")
        f.write("\n")

    f.write("};\n\n")
    f.write(f"#define {array_name}_SIZE {data_size}\n\n")

def main():
    parser = argparse.ArgumentParser(
        description='Convert a file, string, or stdin to a C array header'
    )
    parser.add_argument('output', help='Output header file (.h)')

    input_group = parser.add_mutually_exclusive_group(required=True)
    input_group.add_argument('-f', '--file', help='Input file to convert')
    input_group.add_argument('-s', '--string', help='String to convert')
    input_group.add_argument('--stdin', action='store_true', help='Read from stdin')

    parser.add_argument('-n', '--name', help='Array name (overrides filename-based name when using --file)')
    parser.add_argument('-m', '--mode', choices=['text', 'binary'],
                       default='binary',
                       help='Output mode: text for string literals, binary for byte arrays (default: binary)')

    args = parser.parse_args()

    if (args.string or args.stdin) and not args.name:
        parser.error("--name is required when using --string or --stdin")

    if args.file:
        write_header_from_file(args.file, args.output, args.mode, args.name)
    elif args.string:
        write_header_from_string(args.string, args.name, args.output, args.mode)
    elif args.stdin:
        write_header_from_stdin(args.name, args.output, args.mode)

if __name__ == "__main__":
    main()

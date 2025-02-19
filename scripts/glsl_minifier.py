# Copyright (c) 2024-2025 Le Juez Victor
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

import sys, re, string, itertools

glsl_keywords = {
    # Preprocessor directives
    'core',

    # Data types
    'void', 'bool', 'uint', 'int', 'float', 
    'ivec2', 'vec2', 'ivec3', 'vec3', 'ivec4', 'vec4',
    'mat2', 'mat3', 'mat4', 'struct', 'double', 'dvec2', 'dvec3', 'dvec4', 
    'dmat2', 'dmat3', 'dmat4',

    # Attributes and uniforms
    'uniform', 'attribute', 'varying', 'const', 'in', 'out', 'inout', 'layout', 
    'binding', 'location', 'centroid', 'sample', 'pixel', 'patch', 'vertex', 
    'instance', 'nonuniform', 'subroutine', 'invariant', 'precise', 'shared',
    'lowp', 'mediump', 'highp', 'flat', 'smooth', 'noperspective'
    
    # Programming keywords
    'for', 'while', 'if', 'else', 'return', 'main', 
    'true', 'false', 'break', 'continue', 'discard', 'do',

    # Mathematical functions
    'sin', 'cos', 'tan', 'min', 'max', 'mix', 'smoothstep', 'step', 'length', 
    'distance', 'dot', 'cross', 'normalize', 'reflect', 'refract', 'clamp', 
    'fract', 'ceil', 'floor', 'abs', 'sign', 'pow', 'exp', 'log', 'exp2', 
    'log2', 'sqrt', 'inversesqrt', 'matrixCompMult', 'transpose', 'inverse', 
    'determinant', 'mod', 'modf', 'isnan', 'isinf', 'ldexp',

    # Texture operators and functions
    'texture2D', 'textureCube', 'texture2DArray', 'sampler2D', 'samplerCube', 
    'sampler2DArray', 'samplerCubeArray', 'texture1D', 'sampler1D', 'texture1DArray', 
    'sampler1DArray', 'dFdx', 'dFdy', 'fwidth',

    # Image data types
    'image2D', 'image3D', 'imageCube', 'image2DArray', 'image3DArray', 'imageCubeArray', 
    'image1D', 'image1DArray', 'image2DRect', 'image2DMS', 'image3DRect', 'image2DArrayMS', 
    'image3DArrayMS', 'image2DShadow', 'imageCubeShadow', 'image2DArrayShadow', 
    'imageCubeArrayShadow',

    # Primitives and geometry
    'primitive', 'point', 'line', 'triangle', 'line_strip', 'triangle_strip', 'triangle_fan',
    
    # Global variables and coordinates
    'gl_Position', 'gl_GlobalInvocationID', 'gl_LocalInvocationID', 'gl_WorkGroupID', 
    'gl_WorkGroupSize', 'gl_NumWorkGroups', 'gl_InvocationID', 'gl_PrimitiveID', 
    'gl_TessCoord', 'gl_FragCoord', 'gl_FrontFacing', 'gl_SampleID', 'gl_SamplePosition', 
    'gl_FragDepth', 'gl_FragStencilRef', 'gl_TexCoord', 'gl_VertexID', 'gl_InstanceID',

    # Tessellation and compute shaders
    'tessellation', 'subpass', 'workgroup',

    # Atomic counters
    'atomic_uint', 'atomic_int', 'atomic_float', 'atomic_counter',
}

def variable_renamer(input_string):
    """
    Renames all variables with short names (one letter, then two letters, etc.)
    while following these rules:
      - Do not modify variables starting with a lowercase letter followed by an uppercase letter
      - Do not modify names that are entirely uppercase
      - Do not modify definitions (#define)
      - Do not modify struct members
      - Do not modify function names
    """
    # Extract function declarations to preserve them
    function_pattern = r'\b(void|bool|int|float|vec\d|mat\d|[a-zA-Z_]\w*)\s+([a-zA-Z_]\w*)\s*\('
    function_matches = re.finditer(function_pattern, input_string)
    function_names = set(match.group(2) for match in function_matches)
    
    # Extract struct definitions and their members to preserve them
    struct_pattern = r'struct\s+(\w+)\s*\{([^}]+)\}'
    struct_matches = re.finditer(struct_pattern, input_string, re.DOTALL)
    
    struct_names = set()
    struct_members = set()
    for match in struct_matches:
        struct_names.add(match.group(1))
        struct_body = match.group(2)
        # Extract member names (after the type and before ; or ,)
        member_pattern = r'(?:[\w\[\]]+\s+)(\w+)(?:\s*[;,])'
        for member in re.finditer(member_pattern, struct_body):
            struct_members.add(member.group(1))
    
    # Retrieve all potential variable names using an improved regex (to capture variable-length identifiers)
    potential_vars = set(re.findall(r'(?<![\.#])\b([a-zA-Z_]\w*)\b(?!\s*\()', input_string))
    
    # Filter according to the rules
    variables_to_rename = []
    for var in potential_vars:
        # Exclude: variables starting with a lowercase letter followed by an uppercase letter
        if re.match(r'^[a-z][A-Z]', var):
            continue
        # Exclude: variables that are entirely uppercase
        if var.isupper():
            continue
        # Exclude struct members and struct names
        if var in struct_members or var in struct_names:
            continue
        # Exclude function names
        if var in function_names:
            continue
        # Exclude GLSL keywords
        if var in glsl_keywords:
            continue
        # Optionally exclude macro definitions (#define) if necessary (not handled here)
        variables_to_rename.append(var)
    
    # Unique short name generator
    def name_generator():
        letters = string.ascii_lowercase
        for length in itertools.count(1):
            for name_tuple in itertools.product(letters, repeat=length):
                yield ''.join(name_tuple)
    
    gen = name_generator()
    new_names = {}
    # Sort variables to rename for deterministic order (helps debugging)
    for var in sorted(variables_to_rename):
        new_names[var] = next(gen)
    
    # Compile a regex to match ALL variables to rename, ensuring only full identifiers are replaced
    pattern = re.compile(
        r'(?<![\.#\w])(' + '|'.join(re.escape(var) for var in new_names.keys()) + r')\b(?!\s*\()'
    )
    
    # Replacement function for re.sub
    def replace_var(match):
        var = match.group(0)
        return new_names.get(var, var)
    
    modified_code = pattern.sub(replace_var, input_string)
    return modified_code

def format_shader(input_string):
    """
    Minifies GLSL shader code by removing comments, extra whitespace, and unnecessary line breaks.
    Preserves preprocessor directives (#define, #version, etc.) with proper formatting.
    Also removes spaces after semicolons.

    Args:
        input_string (str): The GLSL shader source code as a single string.

    Returns:
        str: Minified shader code where comments are removed, code lines are compacted,
             and spaces after semicolons are eliminated.
    """
    # Remove multiline comments (/* ... */) using regex with the DOTALL flag to match across multiple lines
    input_string = re.sub(r"/\*.*?\*/", "", input_string, flags=re.S)
    
    # Remove single-line comments (// ...) and trim whitespace from each line
    lines = [re.split("//", line, 1)[0].strip() for line in input_string.splitlines()]
    
    # Remove empty lines resulting from comment removal or whitespace trimming
    lines = [line for line in lines if line]

    # Renommer les variables avant la minification
    input_string = "\n".join(lines)
    input_string = variable_renamer(input_string)
    
    # Reprendre le processus de minification
    lines = input_string.splitlines()
    
    output = []
    buffer = ""

    for line in lines:
        # Preserve preprocessor directives (lines starting with #)
        if line.startswith("#"):
            # If there's accumulated code in the buffer, add it to output before processing the directive
            if buffer:
                output.append(buffer)
                buffer = ""
            output.append(line)  # Preprocessor directives remain on their own lines
        else:
            # Concatenate non-directive lines into a single compact string
            buffer += line + " "

    # Add any remaining code in the buffer to the output
    if buffer:
        output.append(buffer)

    # Join all lines into a single string with explicit newline characters
    minified_code = "\\n".join(output).strip()
    
    # Remove unnecessary spaces around all specified characters
    minified_code = re.sub(r"\s*(;|,|\+|-|\*|/|\(|\)|{|}|\=)\s*", r"\1", minified_code)

    return minified_code

def main():
    """
    Main entry point for the script. Reads a GLSL shader file, processes it using format_shader,
    and outputs the minified shader code to the standard output.
    """
    if len(sys.argv) < 2:
        print("Usage: python glsl_minifier.py <path_to_shader>")
        return

    filepath = sys.argv[1]
    try:
        with open(filepath, "r") as file:
            input_shader = file.read()

        formatted_shader = format_shader(input_shader)
        print(formatted_shader, end="")  # Avoids trailing newlines
    except FileNotFoundError:
        print(f"Error: File not found [{filepath}]")
    except Exception as e:
        print(f"An error occurred: {e}")

if __name__ == "__main__":
    main()
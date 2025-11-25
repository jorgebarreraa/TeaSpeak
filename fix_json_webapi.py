#!/usr/bin/env python3
"""
Fix JsonCpp string_view compatibility issues in WebAPI.cpp
Replaces all Json::Value["literal"] with Json::Value[std::string("literal")]
"""

import re
import sys

def fix_json_access(content):
    """
    Replace patterns like:
    - obj["key"]
    - obj["key"]["nested"]
    With:
    - obj[std::string("key")]
    - obj[std::string("key")][std::string("nested")]
    """
    # Pattern to match: identifier or closing bracket followed by ["string literal"]
    # This handles both simple and nested cases
    pattern = r'([\w_]+|\])\["([^"]*)"\]'

    def replacer(match):
        prefix = match.group(1)
        key = match.group(2)
        return f'{prefix}[std::string("{key}")]'

    # Apply the replacement iteratively until no more matches
    # This handles nested cases like response["a"]["b"]["c"]
    prev_content = None
    while prev_content != content:
        prev_content = content
        content = re.sub(pattern, replacer, content)

    return content

def main():
    input_file = '/home/user/TeaSpeak/Server/Server/license/server/WebAPI.cpp'
    output_file = input_file

    # Read the file
    with open(input_file, 'r') as f:
        content = f.read()

    # Apply the fix
    fixed_content = fix_json_access(content)

    # Write back
    with open(output_file, 'w') as f:
        f.write(fixed_content)

    print(f"Fixed {input_file}")
    print("Applied std::string() conversion to all Json::Value accesses")

if __name__ == '__main__':
    main()

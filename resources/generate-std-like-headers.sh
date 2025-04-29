#!/bin/bash

# Default parameters
GCC_PATH=gcc
MINGW_STD_THREADS_PATH="/external/mingw-std-threads"
INSTALL_PREFIX="/src/build/x86_64-w64-mingw32"

# Create the destination folder within the install prefix
DESTINATION_FOLDER="$INSTALL_PREFIX/include"
mkdir -p "$DESTINATION_FOLDER"

# Retrieve system header search paths
echo "Retrieving system header search paths..."
INCLUDE_PATHS=($($GCC_PATH -xc++ -E -v - < /dev/null 2>&1 | sed -n '/#include <...> search starts here:/, /End of search list./p' | grep -vE '(#include|End of search list.)'))

echo "Include paths found:"
printf '%s\n' "${INCLUDE_PATHS[@]}"

# Headers to be generated
headers=("condition_variable" "future" "mutex" "shared_mutex" "thread")

# Generate headers
for header in "${headers[@]}"; do
    mingw_header="mingw.${header}.h"
    mingw_header_path="$MINGW_STD_THREADS_PATH/$mingw_header"

    if [ ! -f "$mingw_header_path" ]; then
        echo "Error: mingw-std-threads header not found: $mingw_header_path"
        exit 1
    fi

    for include_path in "${INCLUDE_PATHS[@]}"; do
        std_header_path="$include_path/$header"
        if [ -f "$std_header_path" ]; then
            echo "Found std header: $std_header_path"
            destination="$DESTINATION_FOLDER/$header" 
            mkdir -p "$(dirname "$destination")" # Ensure parent directory exists
            echo "#pragma once" > "$destination"
            echo "#include \"$std_header_path\"" >> "$destination"
            echo "#include \"$mingw_header_path\"" >> "$destination"
            echo "Generated <$header> at $destination"
            break
        fi
    done
done

echo "Successfully generated std-like headers. Use them by adding -I$DESTINATION_FOLDER to your compiler command line parameters."

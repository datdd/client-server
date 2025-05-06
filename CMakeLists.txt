cmake_minimum_required(VERSION 3.10)
project(ChatApp CXX)

# Set C++ Standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_EXTENSIONS False)

# Add subdirectories
add_subdirectory(common)
add_subdirectory(server)
add_subdirectory(client)
add_subdirectory(third_party)

# Enable Google Style formatting (optional, requires clang-format)
find_program(CLANG_FORMAT clang-format)
if(CLANG_FORMAT)
    message(STATUS "Found clang-format: ${CLANG_FORMAT}")

    # Use file(GLOB_RECURSE) to find all relevant source and header files recursively
    # This pattern should find all .h and .cc files in the source directory and its subdirectories
    file(GLOB_RECURSE FORMAT_FILES
        "${CMAKE_SOURCE_DIR}/**/*.h"
        "${CMAKE_SOURCE_DIR}/**/*.cpp"
    )

    # --- Debugging Message ---
    # Add this line to print the value of FORMAT_FILES
    # message(STATUS "FORMAT_FILES found: ${FORMAT_FILES}")
    # -------------------------

    # Add custom target for formatting
    add_custom_target(format
        COMMAND ${CLANG_FORMAT} -i ${FORMAT_FILES}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Formatting C++ source files with clang-format"
    )

    # Add custom target for checking format
    add_custom_target(check-format
        COMMAND ${CLANG_FORMAT} --Werror --dry-run ${FORMAT_FILES}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Checking C++ source file format with clang-format"
    )
else()
    message(WARNING "clang-format not found. Formatting targets will not be available.")
    add_custom_target(format
        COMMAND echo "clang-format not found. Skipping formatting."
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
    add_custom_target(check-format
        COMMAND echo "clang-format not found. Skipping format check."
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
endif()

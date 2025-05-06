# Add the server executable
add_executable(server_app
    server_main.cpp
    src/Server.cpp
    src/ClientHandler.cpp
    src/BroadcastMessageHandler.cpp
    src/FileTransferHandler.cpp
    src/CompositeMessageHandler.cpp
)

# Link the common library
target_link_libraries(server_app PRIVATE
    common_lib
)

# Specify include directories for the server
target_include_directories(server_app PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Link necessary system libraries (for threads and filesystem)
find_package(Threads REQUIRED)
target_link_libraries(server_app PRIVATE Threads::Threads)

# Link filesystem library (required by FileTransferHandler for creating directories)
# Check if filesystem is available as a separate library (e.g., on older systems)
# If not found, it might be included in the standard library for C++17+
find_package(PkgConfig QUIET)
pkg_check_modules(LIBSTDCPP_FILESYSTEM QUIET libstdc++fs)

if(LIBSTDCPP_FILESYSTEM_FOUND)
  target_link_libraries(server_app PRIVATE ${LIBSTDCPP_FILESYSTEM_LIBRARIES})
elseif(CMAKE_CXX_STANDARD GREATER_EQUAL 17)
  # Filesystem should be part of the standard library with C++17 or later
  # No explicit link needed, but ensure C++17 is set (already done in root CMakeLists.txt)
else()
  message(WARNING "Filesystem library not found and C++ standard is less than 17. File transfer may not work.")
endif()


# Link socket library based on platform (POSIX)
# This is now handled by common_lib, but keeping this here as a reminder for platform-specific links if needed elsewhere.
# if(UNIX)
#     target_link_libraries(server_app PRIVATE rt) # For POSIX sockets
# endif()

# If you add Windows support later, you'd add a similar block for Winsock
# if(MSVC)
#     target_link_libraries(server_app PRIVATE ws2_32)
# endif()

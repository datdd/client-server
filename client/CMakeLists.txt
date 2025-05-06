# Add the client executable
add_executable(client_app
    client_main.cpp
    src/Client.cpp
    src/ClientFileTransferHandler.cpp
)

# Link the common library
target_link_libraries(client_app PRIVATE
    common_lib
)

# Specify include directories for the client
target_include_directories(client_app PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Link necessary system libraries (for threads and filesystem)
find_package(Threads REQUIRED)
target_link_libraries(client_app PRIVATE Threads::Threads)

# Filesystem library is part of the C++17 standard library and is typically
# linked automatically when CMAKE_CXX_STANDARD is set to 17 or higher.

# If you add Windows support later, you'd add a similar block for Winsock
# if(MSVC)
#     target_link_libraries(client_app PRIVATE ws2_32)
# endif()

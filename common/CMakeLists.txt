# Define source files based on platform
set(COMMON_SOURCES
    include/ISocket.h
    include/IMessageHandler.h
    include/MessageType.h
    include/Message.h
    include/MessageSerialization.h
    src/MessageSerialization.cpp
)

if(UNIX)
    list(APPEND COMMON_SOURCES
        include/PosixSocket.h
        src/PosixSocket.cpp
    )
endif()

if(MSVC) # Check for MSVC compiler (typical on Windows)
    list(APPEND COMMON_SOURCES
        include/WinsockSocket.h
        src/WinsockSocket.cpp
    )
endif()

# Add the common library
add_library(common_lib STATIC ${COMMON_SOURCES})

# Specify include directories for this library
target_include_directories(common_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Link necessary system libraries based on platform
# Moved target_link_libraries to after add_library
if(UNIX)
    target_link_libraries(common_lib PUBLIC rt) # For POSIX sockets
endif()

if(MSVC) # Check for MSVC compiler (typical on Windows)
    target_link_libraries(common_lib PUBLIC ws2_32) # For Winsock
endif()

# Ensure Winsock is initialized and cleaned up (handled within WinsockSocket class)
# No direct CMake commands needed for this, but linking ws2_32 is crucial.

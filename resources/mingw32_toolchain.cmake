# Set the system name to Windows
set(CMAKE_SYSTEM_NAME Windows)

# ensure standard for c++ flags
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_COMPILER i686-w64-mingw32-g++)

# Ensure standard flags for C
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_COMPILER i686-w64-mingw32-gcc)

# Adjust the root path to point to the 32-bit MinGW location
set(CMAKE_FIND_ROOT_PATH /usr/i686-w64-mingw32)

# Specify the processor architecture as 32-bit
set(CMAKE_SYSTEM_PROCESSOR i686)

# Adjust the default behaviour of the FIND_ commands: search programs in the host environment
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)

# For libraries and headers in the target environment
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

# pthread flgas
set(CMAKE_THREAD_LIBS_INIT "-lpthread")
set(CMAKE_HAVE_THREADS_LIBRARY 1)
set(CMAKE_USE_WIN32_THREADS_INIT 0)
set(CMAKE_USE_PTHREADS_INIT 1)
set(THREADS_PREFER_PTHREAD_FLAG ON)

# cmake flags flgas
set(CMAKE_INCLUDE_PATH "/src/build/i686-w64-mingw32;${CMAKE_INCLUDE_PATH}")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DWIN32_LEAN_AND_MEAN")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -I/src/build/i686-w64-mingw32")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -I/src/build/i686-w64-mingw32/include")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -L/src/build/i686-w64-mingw32/lib")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DWIN32_LEAN_AND_MEAN")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -I/src/build/i686-w64-mingw32" CACHE STRING "" FORCE)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -I/src/build/i686-w64-mingw32/include")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -L/src/build/i686-w64-mingw32/lib")


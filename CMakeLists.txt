cmake_minimum_required(VERSION 2.8.9)
project (tealtree)
set(CMAKE_BUILD_TYPE Release)
if (CMAKE_SYSTEM_PROCESSOR MATCHES "(x86)|(X86)|(amd64)|(AMD64)")
    set (X86 TRUE)
else ()
    set (X86 FALSE)
endif ()

include(CheckCXXCompilerFlag)
CHECK_CXX_COMPILER_FLAG("-std=c++11" COMPILER_SUPPORTS_CXX11)
CHECK_CXX_COMPILER_FLAG("-std=c++14" COMPILER_SUPPORTS_CXX14)
if(NOT COMPILER_SUPPORTS_CXX11)
        message(FATAL_ERROR "C++11 compiler is required")
endif()

  if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
  # Clang
  if(NOT COMPILER_SUPPORTS_CXX14)
    message(FATAL_ERROR "Clang with C==14 support is required")
  endif()
  set (CMAKE_CXX_FLAGS "-std=c++14 ${CMAKE_CXX_FLAGS}")
  #-march=native 
  set (CMAKE_CXX_FLAGS "-Ofast -flto ${CMAKE_CXX_FLAGS}")
  set (CMAKE_EXE_LINKER_FLAGS "-Ofast -flto ${CMAKE_EXE_LINKER_FLAGS}")
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
        # GCC
  set (CMAKE_CXX_FLAGS "-std=c++11 ${CMAKE_CXX_FLAGS}")
  set (CMAKE_CXX_FLAGS "-O3 -flto ${CMAKE_CXX_FLAGS}")
  set (CMAKE_CXX_FLAGS "-march=native ${CMAKE_CXX_FLAGS}")
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Intel")
  # using Intel C++
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
  # using Visual Studio C++
endif()

#set(EXECUTABLE_OUTPUT_PATH "bin/")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

include_directories(include)
file(GLOB SOURCES "src/*.cpp")
add_definitions(-DNDEBUG)
add_definitions(-DGHEAP_CPP11)
add_executable(tealtree ${SOURCES})
#message(FATAL_ERROR "CMAKE_CXX_FLAGS : " ${CMAKE_CXX_FLAGS})

cmake_minimum_required(VERSION 3.7)
project(sandbox_fs)
include(FindPkgConfig)

set(CMAKE_CXX_STANDARD 17)
set(ENV{PKG_CONFIG_PATH} "$ENV{PKG_CONFIG_PATH}:/usr/local/opt/libarchive/lib/pkgconfig")

find_package(fmt CONFIG REQUIRED)
find_package(glog CONFIG REQUIRED)
find_package(folly CONFIG REQUIRED)
find_package(gflags CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(double-conversion CONFIG REQUIRED)

pkg_check_modules(FUSE REQUIRED fuse)
pkg_check_modules(JEMALLOC REQUIRED jemalloc)
pkg_check_modules(LIBARCHIVE REQUIRED libarchive)

if(${CMAKE_BUILD_TYPE} MATCHES "Release")
    add_compile_options(-flto=full)
endif()

add_compile_options(-pthread)
add_compile_options(-Wno-nullability-completeness)
add_compile_options(-DFUSE_USE_VERSION=26)
add_compile_options(${FUSE_CFLAGS})
add_compile_options(${FUSE_CFLAGS_OTHER})

link_directories(
    ${FUSE_LIBRARY_DIRS}
    ${JEMALLOC_LIBRARY_DIRS}
    ${LIBARCHIVE_LIBRARY_DIRS})

include_directories(
    ${FUSE_INCLUDE_DIRS}
    ${JEMALLOC_INCLUDE_DIRS}
    ${LIBARCHIVE_INCLUDE_DIRS}
    /usr/local/include)

add_executable(sandbox_fs
    backend.h
    byte_buffer.h
    control_interface.h
    file_backend.cpp
    file_backend.h
    file_node.cpp
    file_node.h
    fuse_error.h
    main.cpp
    sandbox_controller.cpp
    sandbox_controller.h
    sandbox_file.cpp
    sandbox_file.h
    sandbox_file_system.cpp
    sandbox_file_system.h
    timer.h
    utils.h)

target_link_libraries(sandbox_fs PRIVATE
    ${FUSE_LIBRARIES}
    ${JEMALLOC_LIBRARIES}
    ${LIBARCHIVE_LIBRARIES}
    /usr/local/lib/libfmt.a
    /usr/local/lib/libglog.a
    /usr/local/lib/libfolly.a
    /usr/local/lib/libgflags.a
    /usr/local/lib/libdouble-conversion.a)

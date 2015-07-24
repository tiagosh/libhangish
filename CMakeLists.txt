project(hangish)

cmake_minimum_required(VERSION 2.8)

set(HANGISH_VERSION_MAJOR 0)
set(HANGISH_VERSION_MINOR 1)
set(HANGISH_VERSION_PATCH 0)

set(QT_VERSION_MAJOR 5)

set(HANGISH_VERSION "${HANGISH_VERSION_MAJOR}.${HANGISH_VERSION_MINOR}.${HANGISH_VERSION_PATCH}")
set(HANGISH_ABI "${HANGISH_VERSION_MAJOR}.${HANGISH_VERSION_MINOR}")

find_package(Qt5 REQUIRED COMPONENTS Core Network Script Xml)
find_package(Protobuf REQUIRED)

include(GNUInstallDirs)

PROTOBUF_GENERATE_CPP(PROTO_SOURCES PROTO_HEADERS hangouts.proto)

set(hangish_SOURCES
    authenticator.cpp
    channel.cpp
    hangishclient.cpp
    utils.cpp
)

set(hangish_HEADERS
    authenticator.h
    channel.h
    hangishclient.h
    types.h
    utils.h
)

QT5_WRAP_CPP(hangish_SOURCES ${hangish_HEADERS})

include_directories(
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_BINARY_DIR}
    ${PROTOBUF_INCLUDE_DIRS}
)


add_library(hangish SHARED ${hangish_SOURCES} ${PROTO_SOURCES})

target_include_directories(hangish PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(hangish
    Qt5::Core
    Qt5::Script
    Qt5::Network
    Qt5::Xml
    ${PROTOBUF_LIBRARIES}
)

target_include_directories(hangish PRIVATE
    ${QT5_INCLUDES}
)

set(HANGISH_INCLUDE_DIR ${CMAKE_INSTALL_INCLUDEDIR}/hangish)
set(HANGISH_LIB_DIR ${CMAKE_INSTALL_LIBDIR})

# Set the correct version number
set_target_properties(
    hangish PROPERTIES
    SOVERSION ${HANGISH_ABI}
    VERSION ${HANGISH_VERSION}
)

install(
    TARGETS hangish
    LIBRARY DESTINATION ${HANGISH_LIB_DIR}
    ARCHIVE DESTINATION ${HANGISH_LIB_DIR}
    COMPONENT Library
)

install(
    FILES ${hangish_HEADERS} ${PROTO_HEADERS}
    DESTINATION ${HANGISH_INCLUDE_DIR}
    COMPONENT Devel
)

# CMake find package stuff
include(CMakePackageConfigHelpers)

configure_file(HangishConfig.cmake.in "${CMAKE_CURRENT_BINARY_DIR}/HangishConfig.cmake" @ONLY)
write_basic_package_version_file("${CMAKE_CURRENT_BINARY_DIR}/HangishConfigVersion.cmake"
                                    VERSION ${HANGISH_VERSION}
                                    COMPATIBILITY ExactVersion)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/HangishConfigVersion.cmake
              ${CMAKE_CURRENT_BINARY_DIR}/HangishConfig.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/hangish
    COMPONENT Devel
)

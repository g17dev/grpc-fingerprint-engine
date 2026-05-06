cmake_minimum_required(VERSION 3.5.1)
project(FingerPrint C CXX)
include("${CMAKE_SOURCE_DIR}/cmake/common.cmake")

get_filename_component(fingerprint_proto "${CMAKE_SOURCE_DIR}/../protos/fingerprint.proto" ABSOLUTE)
get_filename_component(fingerprint_proto_path "${fingerprint_proto}" PATH)

set(fingerprint_proto_srcs "${CMAKE_CURRENT_BINARY_DIR}/fingerprint.pb.cc")
set(fingerprint_proto_hdrs "${CMAKE_CURRENT_BINARY_DIR}/fingerprint.pb.h")
set(fingerprint_grpc_srcs "${CMAKE_CURRENT_BINARY_DIR}/fingerprint.grpc.pb.cc")
set(fingerprint_grpc_hdrs "${CMAKE_CURRENT_BINARY_DIR}/fingerprint.grpc.pb.h")

add_custom_command(
      OUTPUT "${fingerprint_proto_srcs}" "${fingerprint_proto_hdrs}" "${fingerprint_grpc_srcs}" "${fingerprint_grpc_hdrs}"
      COMMAND ${_PROTOBUF_PROTOC}
      ARGS --grpc_out "${CMAKE_CURRENT_BINARY_DIR}"
        --cpp_out "${CMAKE_CURRENT_BINARY_DIR}"
        -I "${fingerprint_proto_path}"
        --plugin=protoc-gen-grpc="${_GRPC_CPP_PLUGIN_EXECUTABLE}"
        "${fingerprint_proto}"
      DEPENDS "${fingerprint_proto}")

include_directories("${CMAKE_CURRENT_BINARY_DIR}")
include_directories("${CMAKE_SOURCE_DIR}")

set(my_base_64_src "${CMAKE_SOURCE_DIR}/base64.h")
set(my_base64_hdr "${CMAKE_SOURCE_DIR}/base64.cc")

add_library(fingerprint_grpc_proto
  ${fingerprint_grpc_srcs}
  ${fingerprint_grpc_hdrs}
  ${fingerprint_proto_srcs}
  ${fingerprint_proto_hdrs})

target_link_libraries(fingerprint_grpc_proto
  ${_REFLECTION}
  ${_GRPC_GRPCPP}
  ${_PROTOBUF_LIBPROTOBUF})

add_library(my_base64 ${my_base_64_src} ${my_base64_hdr})

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBFPRINT REQUIRED libfprint-2)
pkg_check_modules(GLIB REQUIRED glib-2.0)

add_executable(fingerprint_server
    fingerprint_server.cc
    libfprint_adapter.cpp
    libfprint_adapter.h)

target_link_libraries(fingerprint_server
    fingerprint_grpc_proto
    my_base64
    ${LIBFPRINT_LIBRARIES}
    ${GLIB_LIBRARIES}
    ${_REFLECTION}
    ${_GRPC_GRPCPP}
    ${_PROTOBUF_LIBPROTOBUF})

target_include_directories(fingerprint_server PRIVATE ${LIBFPRINT_INCLUDE_DIRS} ${GLIB_INCLUDE_DIRS})
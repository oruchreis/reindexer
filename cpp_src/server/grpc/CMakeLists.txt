cmake_minimum_required(VERSION 3.20.2) 

project(reindexer_grpc_library)
set (TARGET reindexer_grpc_library)


set(GENERATED_PROTO_DIR "${PROJECT_BINARY_DIR}")
set(GENERATED_TESTS_DIR "${REINDEXER_SOURCE_PATH}/../qa_tests/tests/grpc")

if (ENABLE_GRPC)
    find_package(Threads REQUIRED)

    #set(OPENSSL_USE_STATIC_LIBS TRUE)
    find_package(OpenSSL REQUIRED)
    #target_link_libraries(${TARGET} OpenSSL::Crypto)
    #target_link_libraries(${TARGET} OpenSSL::SSL)

    set(Protobuf_USE_STATIC_LIBS ON)
    set(protobuf_MODULE_COMPATIBLE TRUE)
    find_package(Protobuf REQUIRED CONFIG)
    find_library(UTF8_RANGE NAMES utf8_range libutf8_range.a utf8_range.lib)
    set(_PROTOBUF_LIBPROTOBUF protobuf::libprotobuf)
    find_program(PROTOBUF_PROTOC_EXECUTABLE protoc)    
    INCLUDE_DIRECTORIES(SYSTEM ${PROTOBUF_INCLUDE_DIR})
    list(APPEND GRPC_LIBRARIES ${PROTOBUF_LIBRARIES} ${_PROTOBUF_LIBPROTOBUF})
    message(STATUS "Using protobuf ${Protobuf_VERSION}")

    find_package(gRPC ${GRPC_PACKAGE_PROVIDER} REQUIRED)
    find_program(GRPC_CPP_PLUGIN grpc_cpp_plugin)
    find_program(PYTHON3 NAMES python3 py python.exe)
    set(_GRPC_GRPCPP_UNSECURE gRPC::grpc++)
    set(_REFLECTION gRPC::grpc++_reflection)
    list(APPEND GRPC_LIBRARIES ${_REFLECTION} ${_GRPC_GRPCPP_UNSECURE})
    message(STATUS "Using gRPC ${gRPC_VERSION}")

    file(GLOB PROTOBUF_SOURCE_FILES ${REINDEXER_SOURCE_PATH}/server/proto/*.proto)
    file(MAKE_DIRECTORY ${GENERATED_PROTO_DIR})
    file(MAKE_DIRECTORY ${GENERATED_TESTS_DIR})

    foreach(CUR_PROTO_FILE ${PROTOBUF_SOURCE_FILES})
        get_filename_component(PROTOBUF_INPUT_DIRECTORY "${CUR_PROTO_FILE}" DIRECTORY)
        get_filename_component(PROTOBUF_INPUT_DIRECTORY "${PROTOBUF_INPUT_DIRECTORY}" ABSOLUTE)
        get_filename_component(PROTO_FILENAME "${CUR_PROTO_FILE}" NAME)
        get_filename_component(CUR_PROTO_FILE "${CUR_PROTO_FILE}" ABSOLUTE)
        string(REGEX REPLACE ".proto" ".pb" CUR_PB_FILE_OUT "${PROTO_FILENAME}")
        string(REGEX REPLACE ".proto" ".grpc.pb" CUR_GRPC_FILE_OUT "${PROTO_FILENAME}")
        string(REGEX REPLACE ".proto" "_pb2" CUR_PB_PYTHON_FILE_OUT "${PROTO_FILENAME}")
        string(REGEX REPLACE ".proto" "_pb2_grpc" CUR_GRPC_PYTHON_FILE_OUT "${PROTO_FILENAME}")
        
        execute_process(COMMAND ${PROTOBUF_PROTOC_EXECUTABLE} --proto_path=${PROTOBUF_INPUT_DIRECTORY} --grpc_out=${GENERATED_PROTO_DIR} --plugin=protoc-gen-grpc=${GRPC_CPP_PLUGIN} --cpp_out=${GENERATED_PROTO_DIR}  ${CUR_PROTO_FILE}
            ERROR_VARIABLE GRPC_COMMAND_ERROR 
            OUTPUT_VARIABLE GRPC_COMMAND_ERROR 
            COMMAND_ECHO STDOUT)
        execute_process(COMMAND ${PYTHON3} -m grpc_tools.protoc -I${PROTOBUF_INPUT_DIRECTORY} --python_out=${GENERATED_TESTS_DIR} --grpc_python_out=${GENERATED_TESTS_DIR}  ${CUR_PROTO_FILE}
            ERROR_VARIABLE GRPC_COMMAND_ERROR
            OUTPUT_VARIABLE GRPC_COMMAND_ERROR
            COMMAND_ECHO STDOUT)
        if(!MSVC)
            set_source_files_properties(${GENERATED_PROTO_DIR}/${CUR_GRPC_FILE_OUT}.h ${GENERATED_PROTO_DIR}/${CUR_GRPC_FILE_OUT}.cc PROPERTIES COMPILE_FLAGS "-Wno-all -Wno-extra -Wno-error -Wno-old-style-cast")
            set_source_files_properties(${GENERATED_PROTO_DIR}/${CUR_PB_FILE_OUT}.h ${GENERATED_PROTO_DIR}/${CUR_PB_FILE_OUT}.cc PROPERTIES COMPILE_FLAGS "-Wno-all -Wno-extra -Wno-error -Wno-old-style-cast")
        endif()
        
        if(EXISTS "${GENERATED_PROTO_DIR}/${CUR_PB_FILE_OUT}.cc")
            message("gRpc success: ${GENERATED_PROTO_DIR}/${CUR_PB_FILE_OUT}.cc")            
        else()
            message(FATAL_ERROR "gRpc failed for ${GENERATED_PROTO_DIR}/${CUR_PB_FILE_OUT}.cc: ${GRPC_COMMAND_ERROR}")
        endif()
        if(EXISTS "${GENERATED_PROTO_DIR}/${CUR_GRPC_FILE_OUT}.cc")
            message("gRpc success: ${GENERATED_PROTO_DIR}/${CUR_GRPC_FILE_OUT}.cc")
        else()
            message(FATAL_ERROR "gRpc failed for ${GENERATED_PROTO_DIR}/${CUR_GRPC_FILE_OUT}.cc: ${GRPC_COMMAND_ERROR}")
        endif()
        if(EXISTS "${GENERATED_TESTS_DIR}/${CUR_PB_PYTHON_FILE_OUT}.py")
            message("gRpc success: ${GENERATED_TESTS_DIR}/${CUR_PB_PYTHON_FILE_OUT}.py")
        else()
            message(FATAL_ERROR "gRpc failed for ${GENERATED_TESTS_DIR}/${CUR_PB_PYTHON_FILE_OUT}.py: ${GRPC_COMMAND_ERROR}")
        endif()
        if(EXISTS "${GENERATED_TESTS_DIR}/${CUR_GRPC_PYTHON_FILE_OUT}.py")
            message("gRpc success: ${GENERATED_TESTS_DIR}/${CUR_GRPC_PYTHON_FILE_OUT}.py")
        else()
            message(FATAL_ERROR "gRpc failed for ${GENERATED_TESTS_DIR}/${CUR_GRPC_PYTHON_FILE_OUT}.py: ${GRPC_COMMAND_ERROR}")
        endif()
        message("python3 -m grpc_tools.protoc -I${PROTOBUF_INPUT_DIRECTORY} --python_out=${GENERATED_TESTS_DIR} --grpc_python_out=${GENERATED_TESTS_DIR} --experimental_allow_proto3_optional ${CUR_PROTO_FILE}")
    endforeach(CUR_PROTO_FILE)
    add_definitions(-DWITH_GRPC)
endif()
file(GLOB GRPC_SRCS ./*.cc ${GENERATED_PROTO_DIR}/*.cc)

if (APPLE)
   set (CMAKE_SHARED_LIBRARY_CREATE_CXX_FLAGS "${CMAKE_SHARED_LIBRARY_CREATE_CXX_FLAGS} -undefined dynamic_lookup")
else() #if(CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
   check_linker_flag (-gz cxx_linker_supports_gz)
   if(cxx_linker_supports_gz)
     set (CMAKE_SHARED_LIBRARY_CREATE_CXX_FLAGS "${CMAKE_SHARED_LIBRARY_CREATE_CXX_FLAGS} -gz")
   endif()
endif()

add_library(${TARGET} ${GRPC_SRCS}) #we need static lib. (oruchreis)
#add_library(${TARGET} SHARED ${GRPC_SRCS})
include_directories(${GENERATED_PROTO_DIR})
target_link_libraries(${TARGET} ${GRPC_LIBRARIES})

set_target_properties(${TARGET} PROPERTIES LIBRARY_OUTPUT_DIRECTORY "../../cmd/reindexer_server" )

install(TARGETS ${TARGET}
    LIBRARY DESTINATION "${CMAKE_INSTALL_PREFIX}/lib"
)

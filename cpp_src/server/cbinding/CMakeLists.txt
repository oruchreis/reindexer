cmake_minimum_required(VERSION 3.18)

project(reindexer_embedded_server)

set(TARGET reindexer_embedded_server)

add_library(${TARGET} SHARED
  "library_init.cc"
  "server_c.cc"
  "../../core/cbinding/reindexer_c.cc"
)

target_include_directories(${TARGET} PRIVATE
  ${REINDEXER_SOURCE_PATH}
  ${REINDEXER_SOURCE_PATH}/vendor
  ${REINDEXER_SOURCE_PATH}/vendor_subdirs
  ${REINDEXER_BINARY_PATH}
)

set_property(TARGET ${TARGET} PROPERTY POSITION_INDEPENDENT_CODE ON)
target_compile_definitions(${TARGET} PRIVATE REINDEXER_C_EXPORTS=1)

set(EMBEDDED_REINDEXER_LIBRARIES reindexer_server_library ${REINDEXER_LIBRARIES})
add_dependencies(${TARGET} reindexer_server_library reindexer)

if(ENABLE_GRPC)
  add_dependencies(${TARGET} reindexer_grpc_library)
  list(APPEND EMBEDDED_REINDEXER_LIBRARIES reindexer_grpc_library)
  target_compile_definitions(${TARGET} PRIVATE WITH_GRPC STATIC_GRPC_LIB=1)
endif()

if(MSVC)
  if(ENABLE_GRPC)
    set(_EXPORTS_DEF "${CMAKE_CURRENT_SOURCE_DIR}/exports_msvc.def")
  else()
    set(_EXPORTS_DEF "${CMAKE_CURRENT_SOURCE_DIR}/exports_msvc_no_grpc.def")
  endif()
  set_target_properties(${TARGET} PROPERTIES LINK_DEPENDS ${_EXPORTS_DEF})
  target_link_options(${TARGET} PRIVATE "/DEF:${_EXPORTS_DEF}" "/OPT:REF" "/OPT:ICF")
  set_property(TARGET ${TARGET} PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
  target_compile_options(${TARGET} PRIVATE "/bigobj")
  if(WITH_LTO)
    target_compile_options(${TARGET} PRIVATE "/GL")
    target_link_options(${TARGET} PRIVATE "/LTCG")
  endif()
elseif(APPLE)
  if(ENABLE_GRPC)
    set(_EXPORTS_LIST "${CMAKE_CURRENT_SOURCE_DIR}/exports_clang.list")
  else()
    set(_EXPORTS_LIST "${CMAKE_CURRENT_SOURCE_DIR}/exports_clang_no_grpc.list")
  endif()
  set_target_properties(${TARGET} PROPERTIES LINK_DEPENDS ${_EXPORTS_LIST})
  target_link_options(${TARGET} PRIVATE "-exported_symbols_list" "${_EXPORTS_LIST}")
  target_compile_options(${TARGET} PRIVATE "-fvisibility=hidden")
else()
  set_target_properties(${TARGET} PROPERTIES LINK_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/exports_gcc.map)
  target_link_options(${TARGET} PRIVATE
    "-Wl,-Bsymbolic"
    "-Wl,-Bsymbolic-functions"
    "-Wl,--exclude-libs,ALL"
    "-Wl,--gc-sections"
    "-Wl,--version-script,${CMAKE_CURRENT_SOURCE_DIR}/exports_gcc.map"
  )
  target_compile_options(${TARGET} PRIVATE "-fvisibility=hidden")
  if(REINDEXER_EMBEDDED_STATIC_DEPS)
    if(NOT MUSL)
      target_link_options(${TARGET} PRIVATE "-static-libgcc" "-static-libstdc++")
    endif()
  endif()
endif()

message(STATUS "'${TARGET}' target link libs: ${EMBEDDED_REINDEXER_LIBRARIES}")
target_link_libraries(${TARGET} PRIVATE ${EMBEDDED_REINDEXER_LIBRARIES})

install(TARGETS ${TARGET}
  RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
  LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
)

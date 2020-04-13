if(MSVC)
    cmake_minimum_required(VERSION 3.15)
    cmake_policy(SET CMP0091 NEW)
else()
    cmake_minimum_required(VERSION 2.8)
endif()
cmake_policy(SET CMP0069 NEW) 
set(CMAKE_POLICY_DEFAULT_CMP0069 NEW)

project(reindexer)

set(TARGET reindexer_embedded_server)

add_library(${TARGET} SHARED "server_c.cc" "../../core/cbinding/reindexer_c.cc")
#There is a ice bug in gcc when enabled lto, so disabled: https://gcc.gnu.org/bugzilla/show_bug.cgi?id=91288
#set_property(TARGET ${TARGET} PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
set_property(TARGET ${TARGET} PROPERTY POSITION_INDEPENDENT_CODE ON)
list(APPEND REINDEXER_LIBRARIES reindexer_server_library)
include_directories(${PROJECT_SOURCE_DIR})
add_dependencies(${TARGET} reindexer_server_library)

#if (NOT MSVC)
#    set_target_properties(${TARGET} PROPERTIES LINK_SEARCH_START_STATIC 1)
#    set_target_properties(${TARGET} PROPERTIES LINK_SEARCH_END_STATIC 1)
#endif()

message(STATUS "'${TARGET}' Target Link Libs: ${REINDEXER_LIBRARIES}")
target_link_libraries(${TARGET} PRIVATE ${REINDEXER_LIBRARIES})

if (WIN32)
  if (MSVC)
    set_property(TARGET ${TARGET} PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    target_compile_options(${TARGET} PRIVATE "/MT$<$<CONFIG:Debug>:Debug>" /GL)
    target_link_options(${TARGET} PUBLIC $<$<CXX_COMPILER_ID:MSVC>:/FORCE:MULTIPLE> /OPT:REF /OPT:ICF /LTCG)

    set(CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS_SKIP true)
    include(InstallRequiredSystemLibraries)
    install(FILES ${CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS} DESTINATION ${CMAKE_INSTALL_BINDIR})
  else()
    get_filename_component(MINGW_DLL_DIR ${CMAKE_CXX_COMPILER} PATH)
    install(FILES
      ${MINGW_DLL_DIR}/libstdc++-6.dll
      ${MINGW_DLL_DIR}/libwinpthread-1.dll
      DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
    if(EXISTS ${MINGW_DLL_DIR}/libgcc_s_seh-1.dll)
      install(FILES ${MINGW_DLL_DIR}/libgcc_s_seh-1.dll DESTINATION ${CMAKE_INSTALL_BINDIR})
    elseif(EXISTS ${MINGW_DLL_DIR}/libgcc_s_dw2-1.dll)
      install(FILES ${MINGW_DLL_DIR}/libgcc_s_dw2-1.dll DESTINATION ${CMAKE_INSTALL_BINDIR})
    else ()
      message (WARNING "Can't find MinGW runtime")
    endif()
  endif()
endif ()

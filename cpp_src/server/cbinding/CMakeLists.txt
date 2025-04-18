if(MSVC)
    cmake_minimum_required(VERSION 3.15)
    cmake_policy(SET CMP0091 NEW)
    cmake_policy(SET CMP0003 NEW)
else()
    cmake_minimum_required(VERSION 3.0)
endif()
cmake_policy(SET CMP0069 NEW) 
set(CMAKE_POLICY_DEFAULT_CMP0069 NEW)

project(reindexer)

set(TARGET reindexer_embedded_server)

add_library(${TARGET} SHARED "library_init.cc" "server_c.cc" "../../core/cbinding/reindexer_c.cc")
#There is a ice bug in gcc when enabled lto, so disabled: https://gcc.gnu.org/bugzilla/show_bug.cgi?id=91288
#if(NOT APPLE AND NOT MUSL)
    #set_property(TARGET ${TARGET} PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
#endif()

set_property(TARGET ${TARGET} PROPERTY POSITION_INDEPENDENT_CODE ON)
list(APPEND REINDEXER_LIBRARIES reindexer_server_library)
include_directories(${PROJECT_SOURCE_DIR})
add_dependencies(${TARGET} reindexer_server_library)
if (ENABLE_GRPC)
    add_dependencies(${TARGET} reindexer_grpc_library)
    list(APPEND REINDEXER_LIBRARIES reindexer_grpc_library)
    add_definitions(-DWITH_GRPC)
endif()

if (MSVC)     
    set_target_properties(${TARGET} PROPERTIES LINK_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/exports_msvc.def)
    target_link_options(${TARGET} PRIVATE "/OPT:REF" "/OPT:ICF" "/LTCG" "/DEF:${CMAKE_CURRENT_SOURCE_DIR}/exports_msvc.def")
    set_target_properties(${TARGET} PROPERTIES LINK_FLAGS "/DEF:${CMAKE_CURRENT_SOURCE_DIR}/exports_msvc.def")
    set (CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} \"/DEF:${CMAKE_CURRENT_SOURCE_DIR}/exports_msvc.def\"")
    set (CMAKE_LINK_DEF_FILE_FLAG "/DEF::${CMAKE_CURRENT_SOURCE_DIR}/exports_msvc.def")
elseif (APPLE)
    set_target_properties(${TARGET} PROPERTIES LINK_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/exports_clang.list)
    set_target_properties(${TARGET} PROPERTIES LINK_FLAGS "-exported_symbols_list ${CMAKE_CURRENT_SOURCE_DIR}/exports_clang.list")
    target_compile_options(${TARGET} PRIVATE "-fvisibility=hidden")
else()
    set_target_properties(${TARGET} PROPERTIES LINK_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/exports_gcc.map)
    target_link_options(${TARGET} PRIVATE -Wl,-Bsymbolic -Wl,-Bsymbolic-functions -Wl,--exclude-libs,ALL -ffunction-sections -Wl,--gc-sections -Wl,--version-script,${CMAKE_CURRENT_SOURCE_DIR}/exports_gcc.map --verbose)
    target_compile_options(${TARGET} PRIVATE "-fvisibility=hidden")
endif()

if (NOT MSVC 
    AND (ENABLE_TCMALLOC OR ENABLE_JEMALLOC)
    )
    target_link_libraries(${TARGET} PRIVATE 
      ${REINDEXER_LIBRARIES}       
      # -Bstatic -lstdc++ -Bdynamic
      )
    set_target_properties(${TARGET} PROPERTIES LINK_SEARCH_START_STATIC 1)
    set_target_properties(${TARGET} PROPERTIES LINK_SEARCH_END_STATIC 1)
    set(CMAKE_FIND_LIBRARY_SUFFIXES ".a")
    target_link_options(${TARGET} PRIVATE --verbose)
    if (NOT APPLE AND NOT MUSL)
        set(CMAKE_SHARED_LINKER_FLAGS "-static-libgcc -static-libstdc++")
        target_link_options(${TARGET} PRIVATE 
        -Wl,-Bsymbolic -Wl,-Bsymbolic-functions
        -Wl,--version-script,${CMAKE_CURRENT_SOURCE_DIR}/exports_gcc.map 
        -Wno-error -Wno-uninitialized -Wno-maybe-uninitialized #for -flto
        -static-libgcc
        -static-libstdc++ 
        --verbose)
    endif()    
endif()

message(STATUS "'${TARGET}' Target Link Libs: ${REINDEXER_LIBRARIES}")
target_link_libraries(${TARGET} PRIVATE ${REINDEXER_LIBRARIES})

# In order to reduce the size of the shared library, we only specify the exported functions. If there is a new export, it should be added to version_script.map file. (oruchreis)
# target_link_options(${TARGET} PUBLIC -Wl,-Bsymbolic -Wl,-Bsymbolic-functions -Wl,--version-script,${CMAKE_CURRENT_SOURCE_DIR}/version_script.map)

if (WIN32)
  if (MSVC)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SAFESEH:NO")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /SAFESEH:NO")
    set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} /SAFESEH:NO")
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

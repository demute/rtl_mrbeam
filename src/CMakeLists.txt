########################################################################
# Build libraries and executables
########################################################################
# commodity libraries
# consider -fvisibility=hidden
# Proper object library type was only introduced with CMake 2.8.8
add_library(r_mrbeam STATIC
    common.c
    compat_time.c
    optparse.c
    r_util.c
    sdr.c
    stream_buffer.c
)

if("${CMAKE_C_COMPILER_ID}" STREQUAL "GNU")
    set_source_files_properties(mongoose.c PROPERTIES COMPILE_FLAGS "-Wno-format")
endif()
if("${CMAKE_C_COMPILER_ID}" MATCHES "Clang")
    set_source_files_properties(mongoose.c PROPERTIES COMPILE_FLAGS "-Wno-format-pedantic -Wno-large-by-value-copy")
endif()

add_executable(rtl_mrbeam rtl_mrbeam.c)
target_link_libraries(rtl_mrbeam r_mrbeam)

if(MSVC)
    # needs CMake 3.1 but Windows builds should have that
    target_sources(rtl_mrbeam PRIVATE getopt/getopt.c)
endif()

target_link_libraries(rtl_mrbeam
    ${SDR_LIBRARIES}
)

set(INSTALL_TARGETS rtl_mrbeam)
if(UNIX)
target_link_libraries(rtl_mrbeam m)
endif()

# Explicitly say that we want C99
set_target_properties(rtl_mrbeam r_mrbeam PROPERTIES C_STANDARD 99)

########################################################################
# Install built library files & utilities
########################################################################
install(TARGETS ${INSTALL_TARGETS}
    RUNTIME DESTINATION bin              # .dll file
)

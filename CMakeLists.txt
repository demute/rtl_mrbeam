cmake_minimum_required(VERSION 3.10)

# set the project name
project(rtl_mrbeam VERSION "0.1")
configure_file (rtl_mrbeam_config.h.in rtl_mrbeam_config.h)

# add the executable
add_executable(rtl_mrbeam src/mrbeam.c src/stream_buffer.c src/common.c src/sdr.c)
include_directories(BEFORE [SYSTEM] include)


target_include_directories(rtl_mrbeam PUBLIC "${PROJECT_BINARY_DIR}")
target_link_libraries(rtl_mrbeam m)


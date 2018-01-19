cmake_minimum_required( VERSION 2.8.12 )
project( rpi_stream_server )
SET( SRCS )
aux_source_directory( "./common_util" SRCS )
aux_source_directory( "./app" SRCS )
aux_source_directory( "./udp_setup" SRCS )
aux_source_directory( "./openmax" SRCS)

add_executable( ${CMAKE_PROJECT_NAME} ${SRCS} )

set( GCC_COVERAGE_COMPILE_FLAGS -DSTANDALONE -D__STDC_CONSTANT_MACROS -D__STDC_LIMIT_MACROS -DTARGET_POSIX -D_LINUX -DPIC -D_REENTRANT -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64 -U_FORTIFY_SOURCE -DHAVE_LIBOPENMAX=2 -DOMX -DOMX_SKIP64BIT -ftree-vectorize -pipe -DUSE_EXTERNAL_OMX -DHAVE_LIBBCM_HOST -DUSE_EXTERNAL_LIBBCM_HOST -DUSE_VCHIQ_ARM -fPIC -ftree-vectorize -pipe -Werror -g -Wall )
set( GCC_COVERAGE_LINK_FLAGS -L/opt/vc/lib -lopenmaxil -lbcm_host -lvcos -lvchiq_arm -lpthread )
set( GCC_COVERAGE_INCLUDE_FLAGS /opt/vc/include /opt/vc/include/interface/vcos/pthreads /opt/vc/include/interface/vmcs_host/linux )

target_compile_options( ${CMAKE_PROJECT_NAME} PRIVATE ${GCC_COVERAGE_COMPILE_FLAGS} )
target_link_libraries( ${CMAKE_PROJECT_NAME} ${GCC_COVERAGE_LINK_FLAGS} )
target_include_directories( ${CMAKE_PROJECT_NAME} PRIVATE ${GCC_COVERAGE_INCLUDE_FLAGS} )

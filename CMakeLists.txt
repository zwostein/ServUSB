cmake_minimum_required( VERSION 2.8 )
project( ServUSB )

set( CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/CMakeModules" ${CMAKE_MODULE_PATH} )
set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -pedantic -std=c99" )

find_package( libusb-1.0 REQUIRED )
include_directories( ${LIBUSB_1_INCLUDE_DIRS} )
add_definitions( ${LIBUSB_1_DEFINITIONS} )

set( EXECUTABLE_NAME "servusb" )
add_executable( ${EXECUTABLE_NAME}
	src/main.c
)
target_link_libraries( ${EXECUTABLE_NAME} ${LIBUSB_1_LIBRARIES} )

install(TARGETS ${EXECUTABLE_NAME} DESTINATION bin)


################################################################
# Packaging

set( CPACK_PACKAGE_VERSION "0.1-snapshot" )

if( WIN32 )
	if( "${CMAKE_SIZEOF_VOID_P}" EQUAL "8" )
		set( CPACK_SYSTEM_NAME win64 )
	endif()
endif()

include( CPack )

################################################################

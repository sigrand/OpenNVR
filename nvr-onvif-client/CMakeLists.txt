project(nvr-onvif-client)
cmake_minimum_required(VERSION 2.8)

set(OSDK ${PROJECT_SOURCE_DIR}/../moment-onvif/onvif-sdk)
include_directories(${OSDK}/include)

if( CMAKE_SIZEOF_VOID_P EQUAL 8 )
link_directories(${OSDK}/lib/x64
                ".")
file(COPY ${OSDK}/lib/x64/libWsDiscovery.so DESTINATION ${PROJECT_BINARY_DIR} )
file(COPY ${OSDK}/lib/x64/libOnvifSDK.so DESTINATION ${PROJECT_BINARY_DIR} )
else()
link_directories(${OSDK}/lib/x86
                ".")
file(COPY ${OSDK}/lib/x86/libWsDiscovery.so DESTINATION ${PROJECT_BINARY_DIR} )
file(COPY ${OSDK}/lib/x86/libOnvifSDK.so DESTINATION ${PROJECT_BINARY_DIR} )
endif()

set (HEADERS ${OSDK}/include/OnvifSDK.h)
set (SOURCES ${PROJECT_SOURCE_DIR}/mainClient.cpp)

add_executable(nvr-onvif-client ${HEADERS} ${SOURCES} )
target_link_libraries(nvr-onvif-client OnvifSDK WsDiscovery)
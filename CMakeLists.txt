cmake_minimum_required(VERSION 3.0.0)
project(qjsWebRtcClient VERSION 0.1.0)

include_directories(${PROJECT_SOURCE_DIR}/src)
file(GLOB SRC_FILES ${PROJECT_SOURCE_DIR}/src/*.c)

find_library(DATACHANNELS_LIB datachannel)

add_library(qjsWebRtcClient SHARED ${SRC_FILES})

target_link_libraries(qjsWebRtcClient PRIVATE "${DATACHANNELS_LIB}")
install(TARGETS qjsWebRtcClient DESTINATION lib)
# install(FILES MathFunctions.h DESTINATION include)


project(IpDrv CXX)

set(SRC_FILES
  "Src/TcpLink.cpp"
  "Src/UdpLink.cpp"
  "Src/IpDrv.cpp"
  "Src/UnSocket.cpp"
)

add_library(${PROJECT_NAME} SHARED ${SRC_FILES})

target_include_directories(${PROJECT_NAME}
  PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/Inc
  PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/Src
)

target_link_libraries(${PROJECT_NAME} Engine Core)

if(TARGET_IS_WINDOWS)
  target_link_libraries(${PROJECT_NAME} ws2_32)
endif()

target_compile_definitions(${PROJECT_NAME} PRIVATE IPDRV_EXPORTS)

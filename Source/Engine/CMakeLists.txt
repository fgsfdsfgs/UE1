project(Engine CXX)

file(GLOB SRC_FILES "Src/*.cpp")

add_library(${PROJECT_NAME} SHARED ${SRC_FILES})

target_include_directories(${PROJECT_NAME}
  PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/Inc
  PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/Src
)

target_link_libraries(${PROJECT_NAME} Core)

target_compile_definitions(${PROJECT_NAME} PRIVATE ENGINE_EXPORTS)

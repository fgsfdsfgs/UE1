project(DCUtil)

set(SRC_FILES "Src/Main.cpp" "Src/Texture.cpp")

add_executable(${PROJECT_NAME} ${SRC_FILES})

target_include_directories(${PROJECT_NAME}
  PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/Inc
  PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/Src
)

target_link_libraries(${PROJECT_NAME} Engine Core)

target_compile_definitions(${PROJECT_NAME} PRIVATE DCUTIL_EXPORTS UPACKAGE_NAME=${PROJECT_NAME})

project(PSPOpenGLDrv C CXX)

if(NOT PSP)
  message(FATAL_ERROR "PSPOpenGLDrv only works on the PSP.")
endif()

set(SRC_FILES "PSPOpenGLDrv.cpp")

add_library(${PROJECT_NAME} ${LIB_TYPE} ${SRC_FILES})

target_include_directories(${PROJECT_NAME}
  PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/Inc
)

target_link_libraries(${PROJECT_NAME})

target_link_libraries(${PROJECT_NAME} Render Engine Core)

target_compile_definitions(${PROJECT_NAME} PRIVATE NOPENGLDRV_EXPORTS UPACKAGE_NAME=${PROJECT_NAME})

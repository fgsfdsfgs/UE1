project(Editor CXX)

set(SRC_FILES
  "Src/CoolBsp.cpp"
  "Src/UnBsp.cpp"
  "Src/UnEdAct.cpp"
  "Src/UnEdCam.cpp"
  "Src/UnEdClick.cpp"
  "Src/UnEdCnst.cpp"
  "Src/UnEdCsg.cpp"
  "Src/UnEdFact.cpp"
  "Src/UnEdRend.cpp"
  "Src/UnEdSrv.cpp"
  "Src/UnEdTran.cpp"
  "Src/UnEditor.cpp"
  "Src/UnMeshEd.cpp"
  "Src/UnScrCom.cpp"
  "Src/UnShadow.cpp"
  "Src/UnSyntax.cpp"
  "Src/UnTopics.cpp"
  "Src/UnVisi.cpp"
  "Src/Editor.cpp"
)

if(NOT TARGET_IS_WINDOWS OR NOT BUILD_WINDRV)
  message(STATUS "Building stripped down Editor.")
  list(APPEND SRC_FILES "Src/NullEdHook.cpp")
else()
  message(STATUS "Building Win32 GUI Editor.")
  list(APPEND SRC_FILES "Src/Win32EdHook.cpp")
endif()

add_library(${PROJECT_NAME} ${LIB_TYPE} ${SRC_FILES})

target_include_directories(${PROJECT_NAME}
  PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/Inc
  PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/Src
)

target_link_libraries(${PROJECT_NAME} Engine Core)
if(TARGET_IS_WINDOWS AND BUILD_WINDRV)
  target_link_libraries(${PROJECT_NAME} Window)
endif()

target_compile_definitions(${PROJECT_NAME} PRIVATE EDITOR_EXPORTS UPACKAGE_NAME=${PROJECT_NAME})

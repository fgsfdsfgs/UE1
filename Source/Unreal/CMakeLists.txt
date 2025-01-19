project(Unreal)

if(USE_SDL OR TARGET_IS_DREAMCAST)
  set(SRC_FILES "Src/SDLLaunch.cpp")
elseif(TARGET_IS_WINDOWS)
  set(SRC_FILES "Src/WinLaunch.cpp")
endif()

if(TARGET_IS_DREAMCAST AND DREAMCAST_NO_WHOLE_ARCHIVE)
  list(APPEND SRC_FILES "Src/ForceLink.cpp")
endif()

if(TARGET_IS_WINDOWS)
  list(APPEND SRC_FILES "Src/Res/LaunchRes.rc")
  add_executable(${PROJECT_NAME} WIN32 ${SRC_FILES})
else()
  add_executable(${PROJECT_NAME} ${SRC_FILES})
endif()

target_include_directories(${PROJECT_NAME}
  PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/Inc
  PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/Src
)

target_compile_definitions(${PROJECT_NAME} PRIVATE UNREAL_EXPORTS UPACKAGE_NAME=${PROJECT_NAME})

if(USE_SDL)
  target_link_libraries(${PROJECT_NAME} ${SDL2_LIBRARY} NSDLDrv)
  target_include_directories(${PROJECT_NAME} PRIVATE ${SDL2_INCLUDE_DIR})
elseif(TARGET_IS_WINDOWS)
  target_link_libraries(${PROJECT_NAME} Window)
elseif(TARGET_IS_DREAMCAST)
  # ???
else()
  message(FATAL_ERROR "Platforms other than Windows and Dreamcast can only use NSDLDrv.")
endif()

if(TARGET_IS_ARM AND BUILD_FOR_PSVITA)
  set_property(TARGET ${PROJECT_NAME} PROPERTY POSITION_INDEPENDENT_CODE ON)
  # hammer in the PIE
  target_compile_options(${PROJECT_NAME} PRIVATE -fPIC -fPIE -pie)
  target_link_options(${PROJECT_NAME} PRIVATE -fPIC -fPIE -pie)
endif()

if(BUILD_STATIC)
  set(LINK_PACKAGES ${INSTALL_TARGETS})
  list(REMOVE_ITEM LINK_PACKAGES ${PROJECT_NAME})
  if(TARGET_IS_DREAMCAST AND DREAMCAST_NO_WHOLE_ARCHIVE)
    target_link_libraries(${PROJECT_NAME} ${LINK_PACKAGES})
  else()
    target_link_libraries(${PROJECT_NAME} -Wl,--whole-archive ${LINK_PACKAGES} -Wl,--no-whole-archive)
  endif()
else()
  target_link_libraries(${PROJECT_NAME} Engine Core)
endif()

target_compile_definitions(${PROJECT_NAME} PRIVATE UNREAL_EXPORTS UPACKAGE_NAME=${PROJECT_NAME})

if(TARGET_IS_DREAMCAST AND DREAMCAST_USE_FATFS)
  target_link_libraries(${PROJECT_NAME} fatfs)
endif()

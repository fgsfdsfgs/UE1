project(NOpenALDrv C CXX)

set(SRC_FILES "NOpenALDrv.cpp")

add_library(${PROJECT_NAME} ${LIB_TYPE} ${SRC_FILES})

if(MSVC)
  if(NOT DEFINED OPENAL_INCLUDE_DIR)
    set(OPENAL_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/../Thirdparty/openal-soft/include")
  endif()
  if(NOT DEFINED OPENAL_LIBRARY)
    set(OPENAL_LIBRARY "${CMAKE_SOURCE_DIR}/../Thirdparty/openal-soft/lib/${MSVC_LIBDIR}/OpenAL32.lib")
  endif()
  if(NOT DEFINED LIBXMP_INCLUDE_DIR)
    set(LIBXMP_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/../Thirdparty/libxmp-lite/include/libxmp-lite")
  endif()
  if(NOT DEFINED LIBXMP_LIBRARY)
    set(LIBXMP_LIBRARY "${CMAKE_SOURCE_DIR}/../Thirdparty/libxmp-lite/lib/${MSVC_LIBDIR}/libxmp-lite.lib")
  endif()
else()
  # Won't bother with find_package as we're probably cross compiling.
  if(NOT DEFINED OPENAL_INCLUDE_DIR)
    set(OPENAL_INCLUDE_DIR )
  endif()
  if(NOT DEFINED OPENAL_LIBRARY)
    set(OPENAL_LIBRARY "-lopenal")
  endif()
  if(NOT DEFINED LIBXMP_INCLUDE_DIR)
    set(LIBXMP_INCLUDE_DIR )
  endif()
  if(NOT DEFINED LIBXMP_LIBRARY)
    set(LIBXMP_LIBRARY "-lxmp")
  endif()
endif()

target_include_directories(${PROJECT_NAME}
  PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/Inc
  PRIVATE
  ${OPENAL_INCLUDE_DIR}
  ${LIBXMP_INCLUDE_DIR}
)

if(MSVC)
  # openal-soft release libs are built without safeseh for some reason?
  target_link_options(${PROJECT_NAME} PRIVATE "/SAFESEH:NO")
endif()

target_link_libraries(${PROJECT_NAME} ${OPENAL_LIBRARY} ${LIBXMP_LIBRARY})

target_link_libraries(${PROJECT_NAME} Engine Core)

target_compile_definitions(${PROJECT_NAME} PRIVATE NOPENALDRV_EXPORTS UPACKAGE_NAME=${PROJECT_NAME})

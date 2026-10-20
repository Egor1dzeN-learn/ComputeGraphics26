cmake_minimum_required(VERSION 3.21)
project(itk_vtk_lab CXX)

# Папка, где собраны ITK и VTK (см. README в корне репозитория)
set(LIBS_DIR "D:/CompGraph/libs" CACHE PATH "Folder with itk-build and vtk-build")

find_package(ITK REQUIRED HINTS ${LIBS_DIR}/itk-build)
include(${ITK_USE_FILE})
find_package(VTK REQUIRED HINTS ${LIBS_DIR}/vtk-build COMPONENTS
  FiltersCore RenderingOpenGL2 RenderingVolumeOpenGL2 RenderingFreeType InteractionStyle)

add_executable(itk_vtk main.cpp utf8.manifest)
target_link_libraries(itk_vtk ${ITK_LIBRARIES} ${VTK_LIBRARIES})
vtk_module_autoinit(TARGETS itk_vtk MODULES ${VTK_LIBRARIES})

# Копируем DLL VTK рядом с exe, чтобы запускать без настройки PATH
add_custom_command(TARGET itk_vtk POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_RUNTIME_DLLS:itk_vtk> $<TARGET_FILE_DIR:itk_vtk>
  COMMAND_EXPAND_LISTS)

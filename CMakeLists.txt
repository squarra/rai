cmake_minimum_required(VERSION 3.5)

project(rai_devel)

## variable default values
set(CMAKE_CXX_STANDARD 17 CACHE STRING "C++ version selection")
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
  set(CMAKE_INSTALL_PREFIX "$ENV{HOME}/.local" CACHE PATH "..." FORCE)
endif()
set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
set(CMAKE_INSTALL_RPATH "\$ORIGIN;${CMAKE_INSTALL_PREFIX}/lib")

## options
option(USE_PHYSX "link to physx" ON)
option(USE_H5 "link to h5 library" ON)
option(USE_FREETYPE "link to freetype2 library" ON)
option(BUILD_TESTS "build tests" OFF)

## compile options
add_compile_options(
  -O3 -g -Wall -Wpedantic
  $<$<COMPILE_LANGUAGE:CXX>:-std=c++17>
  $<$<COMPILE_LANGUAGE:CXX>:-Wno-terminate>
  $<$<COMPILE_LANGUAGE:CXX>:-Wno-array-bounds>
  )

################################################################################

add_definitions(
  -DRAI_ROOT_PATH=\"${CMAKE_SOURCE_DIR}\"
  -DRAI_X11 -DRAI_LAPACK -DRAI_EIGEN -DRAI_PTHREAD
  -DRAI_GL
  -DRAI_GLFW -DRAI_ANN -DRAI_QHULL8 -DRAI_ASSIMP -DNO_CYCLE_DETECTION -DRAI_PNG -DRAI_PLY
  -DRAI_GJK -DRAI_CCD -DRAI_FCL -DRAI_Lewiner -DRAI_VHACD
  )

################################################################################

include_directories(
  src
  $ENV{HOME}/.local/include
  /usr/include/eigen3
  )

file(GLOB SRC_Core src/Core/*.cpp)
file(GLOB SRC_Optim src/Optim/*.cpp)
file(GLOB SRC_Geo src/Geo/*.cpp)
file(GLOB SRC_Algo src/Algo/*.cpp)
file(GLOB SRC_Search src/Search/*.cpp)
file(GLOB SRC_PathAlgos src/PathAlgos/*.cpp)
file(GLOB SRC_Gui src/Gui/*.cpp)
file(GLOB SRC_Kin src/Kin/*.cpp)
file(GLOB SRC_KOMO src/KOMO/*.cpp)
file(GLOB SRC_Logic src/Logic/*.cpp)
file(GLOB SRC_LGP src/LGP/*.cpp)
file(GLOB SRC_Control src/Control/*.cpp)

file(GLOB SRC_GeoGJK src/Geo/GJK/*.c)
file(GLOB SRC_GeoPly src/Geo/ply/*.c)
file(GLOB SRC_GeoCcd src/Geo/ccd_rai/*.c)
file(GLOB SRC_GeoLewiner src/Geo/Lewiner/*.cpp)

add_library(raiobjs OBJECT
  ${SRC_Core}
  ${SRC_Optim}
  ${SRC_Geo}
  ${SRC_GeoGJK}
  ${SRC_GeoPly}
  ${SRC_GeoCcd}
  ${SRC_GeoLewiner}
  ${SRC_Algo}
  ${SRC_Search}
  ${SRC_PathAlgos}
  ${SRC_Gui}
  ${SRC_Kin}
  ${SRC_KOMO}
  ${SRC_Logic}
  ${SRC_LGP}
  ${SRC_Control}
  )

################################################################################

link_directories(
  $ENV{HOME}/.local/lib
  )

if(USE_H5)
  link_directories(/usr/lib/x86_64-linux-gnu/hdf5/serial)
endif()

if(USE_PHYSX)
  link_directories($ENV{HOME}/.local/lib/PhysX)
endif()

set_property(TARGET raiobjs PROPERTY POSITION_INDEPENDENT_CODE 1)
add_library(rai SHARED $<TARGET_OBJECTS:raiobjs>)
add_library(rai_static STATIC $<TARGET_OBJECTS:raiobjs>)

################################################################################

#find_package (Eigen3 3.3 REQUIRED NO_MODULE)
#target_link_libraries (rai Eigen3::Eigen)
find_package(PkgConfig REQUIRED)

if(USE_FREETYPE)
  find_package(Freetype REQUIRED)
  add_definitions(-DRAI_FREETYPE)
  include_directories(rai ${FREETYPE_INCLUDE_DIRS})
  target_link_libraries(rai PRIVATE ${FREETYPE_LIBRARIES})
  message(STATUS "[rai] using freetype2 libs: " ${FREETYPE_LIBRARIES})
endif()

if(USE_H5)
  find_package(HDF5 REQUIRED)
  add_definitions(-DRAI_H5)
  include_directories(${HDF5_INCLUDE_DIRS})
  target_link_libraries(rai PRIVATE hdf5_cpp hdf5 z)
  message(STATUS "[rai] using H5 libs: hdf5 hdf5_cpp in " ${HDF5_INCLUDE_DIRS})
endif()

if(USE_PHYSX)
  add_definitions(-DRAI_PHYSX -DNDEBUG -DPX_DISABLE_FLUIDS -DCORELIB -DPX32 -DLINUX)
  target_link_libraries(rai PRIVATE PhysXExtensions_static_64 PhysX_static_64 PhysXCooking_static_64 PhysXCommon_static_64 PhysXFoundation_static_64)
  message(STATUS "[rai] using physx libs from ~/.local")
endif()

if(USE_OPENCV)
  add_definitions(-DRAI_OPENCV)
  include_directories($ENV{HOME}/.local/include/opencv4)
  target_link_libraries(rai opencv_core opencv_highgui opencv_imgproc opencv_videoio)
  message(STATUS "[rai] using OpenCV libs from ~/.local")
endif()

pkg_check_modules(DEPS REQUIRED glfw3 glew blas lapack jsoncpp libpng assimp qhull)
find_package(GLUT REQUIRED)

message(STATUS "[rai] pkg-config libraries: " "${DEPS_LIBRARIES}")

# default/required dependencies
target_link_libraries(rai PRIVATE
  ${DEPS_LIBRARIES}
  ${GLUT_LIBRARIES}
  fcl ccd ANN
  )

################################################################################

get_property(_defs DIRECTORY PROPERTY COMPILE_DEFINITIONS)
message(STATUS "[rai] compiler flags: " "${_defs}")
message(STATUS "[rai] source dir: " "${CMAKE_SOURCE_DIR}")

################################################################################

add_executable(kinEdit bin/src_kinEdit/main.cpp)
target_link_libraries(kinEdit rai)

add_executable(meshTool bin/src_meshTool/main.cpp)
target_link_libraries(meshTool rai)

if(BUILD_TESTS)
  enable_testing()

  function(add_rai_test test_name source_file)
    add_executable(${test_name} ${source_file})
    target_link_libraries(${test_name} ${ARGN})
    get_filename_component(test_dir ${source_file} DIRECTORY)
    add_test(NAME ${test_name}
      COMMAND ${test_name} -noInteractivity
      WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/${test_dir}
    )
  endfunction()

  add_rai_test(test_timingOpt test/Control/timingOpt/main.cpp rai)
  add_rai_test(test_mobileBase test/Control/mobileBase/main.cpp rai)
  add_rai_test(test_switches test/KOMO/switches/main.cpp rai)
  add_rai_test(test_manip test/KOMO/manip/main.cpp rai)
  add_rai_test(test_timeopt test/KOMO/timeopt/main.cpp rai)
  add_rai_test(test_tutorial test/KOMO/tutorial/main.cpp rai)
  add_rai_test(test_komo test/KOMO/komo/main.cpp rai)
  add_rai_test(test_splined test/KOMO/splined/main.cpp rai)
  add_rai_test(test_factored test/KOMO/factored/main.cpp rai)
  add_rai_test(test_skeleton test/KOMO/skeleton/main.cpp rai)
  add_rai_test(test_geo test/Geo/geo/main.cpp rai)
  add_rai_test(test_convex test/Geo/convex/main.cpp rai qhull)
  add_rai_test(test_PairCollisions test/Geo/PairCollisions/main.cpp rai)
  add_rai_test(test_mesh test/Geo/mesh/main.cpp rai)
  add_rai_test(test_signedDistanceFunctions test/Geo/signedDistanceFunctions/main.cpp rai)
  add_rai_test(test_collisionEngines test/Kin/collisionEngines/main.cpp rai)
  add_rai_test(test_CameraView test/Kin/CameraView/main.cpp rai)
  add_rai_test(test_pairCollision_jacobians test/Kin/pairCollision_jacobians/main.cpp rai)
  add_rai_test(test_features test/Kin/features/main.cpp rai)
  add_rai_test(test_bullet test/Kin/bullet/main.cpp rai)
  add_rai_test(test_simulation test/Kin/simulation/main.cpp rai)
  add_rai_test(test_SSB_distance test/Kin/SSB_distance/main.cpp rai)
  add_rai_test(test_dynamics test/Kin/dynamics/main.cpp rai)
  add_rai_test(test_kin test/Kin/kin/main.cpp rai)
  add_rai_test(test_RenderData test/Gui/RenderData/main.cpp rai)
  add_rai_test(test_pcl test/Perception/pcl/main.cpp rai)
  add_rai_test(test_util test/Core/util/main.cpp rai)
  add_rai_test(test_array test/Core/array/main.cpp rai)
  add_rai_test(test_thread test/Core/thread/main.cpp rai)
  add_rai_test(test_graph test/Core/graph/main.cpp rai)
  add_rai_test(test_hdf5 test/Core/h5/main.cpp rai)
  add_rai_test(test_yaml test/Core/yaml/main.cpp rai yaml-cpp)
  add_rai_test(test_ML-regression test/Algo/ML-regression/main.cpp rai)
  add_rai_test(test_ann test/Algo/ann/main.cpp rai)
  add_rai_test(test_rrt test/Algo/rrt/main.cpp rai)
  add_rai_test(test_spanningTree test/Algo/spanningTree/main.cpp rai)
  add_rai_test(test_splines test/Algo/splines/main.cpp rai)
  add_rai_test(test_pickAndPlace test/LGP/pickAndPlace/main.cpp rai)
  add_rai_test(test_newLGP test/LGP/newLGP/main.cpp rai)
  add_rai_test(test_pddl test/Logic/pddl/main.cpp rai)
  add_rai_test(test_fol test/Logic/fol/main.cpp rai)
  add_rai_test(test_optim test/Optim/optim/main.cpp rai)
  add_rai_test(test_nlopt test/Optim/nlopt/main.cpp rai)
  add_rai_test(test_constrained test/Optim/constrained/main.cpp rai)
  add_rai_test(test_benchmarks test/Optim/benchmarks/main.cpp rai)
  add_rai_test(test_ipopt test/Optim/ipopt/main.cpp rai)
endif()

################################################################################

message(STATUS "[rai] installing headers and lib in " ${CMAKE_INSTALL_PREFIX}/include/rai|lib)

install(
  DIRECTORY src/
  DESTINATION ${CMAKE_INSTALL_PREFIX}/include/rai
  FILES_MATCHING
  PATTERN "*.h" PATTERN "*.ipp"
  PATTERN "*retired*" EXCLUDE
  )

install(
  TARGETS rai rai_static
  DESTINATION ${CMAKE_INSTALL_PREFIX}/lib)

install(
  TARGETS kinEdit meshTool
  DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)


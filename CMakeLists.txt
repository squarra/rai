cmake_minimum_required(VERSION 3.15)

project(rai_devel C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- Options ---
option(USE_FREETYPE "link to freetype2 library" ON)
option(USE_H5 "link to h5 library" ON)
option(USE_PHYSX "link to physx" ON)
option(USE_OPENCV "link to opencv" OFF)
option(USE_PYBIND "build python bindings" OFF)
option(BUILD_TESTS "build tests" OFF)

# --- Find Dependencies ---
find_package(PkgConfig REQUIRED)
find_package(GLUT REQUIRED)
find_package(Qhull REQUIRED)

pkg_check_modules(
  DEPS
  REQUIRED
  glfw3
  glew
  blas
  lapack
  jsoncpp
  libpng
  assimp)

if(USE_FREETYPE)
  find_package(Freetype REQUIRED)
endif()

if(USE_H5)
  find_package(HDF5 REQUIRED)
endif()

# --- Source files ---
file(
  GLOB
  RAI_SOURCES
  CONFIGURE_DEPENDS
  "src/Core/*.cpp"
  "src/Optim/*.cpp"
  "src/Geo/*.cpp"
  "src/Algo/*.cpp"
  "src/Search/*.cpp"
  "src/PathAlgos/*.cpp"
  "src/Gui/*.cpp"
  "src/Kin/*.cpp"
  "src/KOMO/*.cpp"
  "src/Logic/*.cpp"
  "src/LGP/*.cpp"
  "src/Control/*.cpp"
  "src/Geo/GJK/*.c"
  "src/Geo/ply/*.c"
  "src/Geo/ccd_rai/*.c"
  "src/Geo/Lewiner/*.cpp"
  "src/DataGen/*.cpp"
  "src/ry/*.cpp")

# --- Library Definition ---
add_library(rai SHARED ${RAI_SOURCES})
set_property(TARGET rai PROPERTY POSITION_INDEPENDENT_CODE ON)

target_include_directories(
  rai
  PUBLIC src
  PRIVATE $ENV{HOME}/.local/include)

target_link_directories(rai PRIVATE $ENV{HOME}/.local/lib)

target_compile_options(
  rai
  PRIVATE -O3
          -g
          -Wall
          -Wpedantic
          $<$<COMPILE_LANGUAGE:CXX>:-std=c++17>
          $<$<COMPILE_LANGUAGE:CXX>:-Wno-terminate>
          $<$<COMPILE_LANGUAGE:CXX>:-Wno-array-bounds>)

target_compile_definitions(
  rai
  PUBLIC RAI_ROOT_PATH=\"${CMAKE_SOURCE_DIR}\"
         RAI_X11
         RAI_LAPACK
         RAI_EIGEN
         RAI_PTHREAD
         RAI_GL
         RAI_GLFW
         RAI_ANN
         RAI_QHULL8
         RAI_ASSIMP
         NO_CYCLE_DETECTION
         RAI_PNG
         RAI_PLY
         RAI_GJK
         RAI_CCD
         RAI_FCL
         RAI_Lewiner
         RAI_VHACD)

# --- Link Libraries ---
target_link_libraries(rai PRIVATE ${DEPS_LIBRARIES} ${GLUT_LIBRARIES} fcl ccd ANN qhull)

if(USE_FREETYPE)
  target_compile_definitions(rai PRIVATE -DRAI_FREETYPE)
  target_include_directories(rai PUBLIC ${FREETYPE_INCLUDE_DIRS})
  target_link_libraries(rai PUBLIC ${FREETYPE_LIBRARIES})
  message(STATUS "[rai] using freetype2 libs: " ${FREETYPE_LIBRARIES})
endif()

if(USE_H5)
  target_compile_definitions(rai PRIVATE -DRAI_H5)
  target_include_directories(rai PUBLIC ${HDF5_INCLUDE_DIRS})
  target_link_libraries(rai PUBLIC hdf5_cpp hdf5 z)
  message(STATUS "[rai] using H5 libs: hdf5 hdf5_cpp in " ${HDF5_INCLUDE_DIRS})
endif()

if(USE_PHYSX)
  target_link_directories(rai PRIVATE $ENV{HOME}/.local/lib/PhysX)
  target_compile_definitions(rai PRIVATE -DRAI_PHYSX -DNDEBUG -DPX_DISABLE_FLUIDS -DCORELIB -DPX32 -DLINUX)
  target_link_libraries(rai PRIVATE PhysXExtensions_static_64 PhysX_static_64 PhysXCooking_static_64 PhysXCommon_static_64 PhysXFoundation_static_64)
  message(STATUS "[rai] using physx libs from ~/.local")
endif()

if(USE_OPENCV)
  target_compile_definitions(rai PRIVATE -DRAI_OPENCV)
  target_include_directories(rai PUBLIC $ENV{HOME}/.local/include/opencv4)
  target_link_libraries(rai PUBLIC opencv_core opencv_highgui opencv_imgproc opencv_videoio)
  message(STATUS "[rai] using OpenCV libs from ~/.local")
endif()

if(USE_PYBIND)
  set(PYBIND11_FINDPYTHON ON)
  find_package(pybind11 CONFIG REQUIRED)
  target_compile_definitions(rai PRIVATE RAI_PYBIND)
  target_include_directories(rai PUBLIC ${pybind11_INCLUDE_DIRS})
  target_link_libraries(rai PUBLIC ${Python_LIBRARIES})
  pybind11_add_module(_robotic src/ry/module.cpp)
  target_link_libraries(_robotic PRIVATE rai)
  install(TARGETS _robotic DESTINATION lib/python)
endif(USE_PYBIND)

# --- Executable Targets ---
add_executable(kinEdit bin/src_kinEdit/main.cpp)
target_link_libraries(kinEdit rai)

add_executable(meshTool bin/src_meshTool/main.cpp)
target_link_libraries(meshTool rai)

# --- Testing ---
if(BUILD_TESTS)
  enable_testing()

  function(add_rai_test test_name source_file)
    add_executable(${test_name} ${source_file})
    target_link_libraries(${test_name} PRIVATE ${ARGN})
    get_filename_component(test_dir ${source_file} DIRECTORY)
    add_test(
      NAME ${test_name}
      COMMAND ${test_name} -noInteractivity
      WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/${test_dir})
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

# --- Installation ---
install(TARGETS kinEdit meshTool DESTINATION bin)

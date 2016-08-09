project(eigenmvn)
cmake_minimum_required(VERSION 2.8)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

find_package(Eigen3 REQUIRED)

# include_directories(${EIGEN3_INCLUDE_DIR})
# add_executable(main test_eigenmvn.cc)

install(
  PROGRAMS ${eigenmvn_SOURCE_DIR}/eigenmvn.h
  DESTINATION ${EIGEN3_INCLUDE_DIR}/Eigen
  RENAME MVN
  )
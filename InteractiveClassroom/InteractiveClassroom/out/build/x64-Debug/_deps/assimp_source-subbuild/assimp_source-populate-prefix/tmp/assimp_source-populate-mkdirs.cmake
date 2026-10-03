# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION ${CMAKE_VERSION}) # this file comes with cmake

# If CMAKE_DISABLE_SOURCE_CHANGES is set to true and the source directory is an
# existing directory in our source tree, calling file(MAKE_DIRECTORY) on it
# would cause a fatal error, even though it would be a no-op.
if(NOT EXISTS "F:/Classroom/InteractiveClassroom/out/build/x64-Debug/_deps/assimp_source-src")
  file(MAKE_DIRECTORY "F:/Classroom/InteractiveClassroom/out/build/x64-Debug/_deps/assimp_source-src")
endif()
file(MAKE_DIRECTORY
  "F:/Classroom/InteractiveClassroom/out/build/x64-Debug/_deps/assimp_source-build"
  "F:/Classroom/InteractiveClassroom/out/build/x64-Debug/_deps/assimp_source-subbuild/assimp_source-populate-prefix"
  "F:/Classroom/InteractiveClassroom/out/build/x64-Debug/_deps/assimp_source-subbuild/assimp_source-populate-prefix/tmp"
  "F:/Classroom/InteractiveClassroom/out/build/x64-Debug/_deps/assimp_source-subbuild/assimp_source-populate-prefix/src/assimp_source-populate-stamp"
  "F:/Classroom/InteractiveClassroom/out/build/x64-Debug/_deps/assimp_source-subbuild/assimp_source-populate-prefix/src"
  "F:/Classroom/InteractiveClassroom/out/build/x64-Debug/_deps/assimp_source-subbuild/assimp_source-populate-prefix/src/assimp_source-populate-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "F:/Classroom/InteractiveClassroom/out/build/x64-Debug/_deps/assimp_source-subbuild/assimp_source-populate-prefix/src/assimp_source-populate-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "F:/Classroom/InteractiveClassroom/out/build/x64-Debug/_deps/assimp_source-subbuild/assimp_source-populate-prefix/src/assimp_source-populate-stamp${cfgdir}") # cfgdir has leading slash
endif()

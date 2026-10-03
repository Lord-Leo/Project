# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION ${CMAKE_VERSION}) # this file comes with cmake

# If CMAKE_DISABLE_SOURCE_CHANGES is set to true and the source directory is an
# existing directory in our source tree, calling file(MAKE_DIRECTORY) on it
# would cause a fatal error, even though it would be a no-op.
if(NOT EXISTS "C:/Users/User/Downloads/InteractiveClassroom/InteractiveClassroom/build/_deps/assimp_source-src")
  file(MAKE_DIRECTORY "C:/Users/User/Downloads/InteractiveClassroom/InteractiveClassroom/build/_deps/assimp_source-src")
endif()
file(MAKE_DIRECTORY
  "C:/Users/User/Downloads/InteractiveClassroom/InteractiveClassroom/build/_deps/assimp_source-build"
  "C:/Users/User/Downloads/InteractiveClassroom/InteractiveClassroom/build/_deps/assimp_source-subbuild/assimp_source-populate-prefix"
  "C:/Users/User/Downloads/InteractiveClassroom/InteractiveClassroom/build/_deps/assimp_source-subbuild/assimp_source-populate-prefix/tmp"
  "C:/Users/User/Downloads/InteractiveClassroom/InteractiveClassroom/build/_deps/assimp_source-subbuild/assimp_source-populate-prefix/src/assimp_source-populate-stamp"
  "C:/Users/User/Downloads/InteractiveClassroom/InteractiveClassroom/build/_deps/assimp_source-subbuild/assimp_source-populate-prefix/src"
  "C:/Users/User/Downloads/InteractiveClassroom/InteractiveClassroom/build/_deps/assimp_source-subbuild/assimp_source-populate-prefix/src/assimp_source-populate-stamp"
)

set(configSubDirs Debug)
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "C:/Users/User/Downloads/InteractiveClassroom/InteractiveClassroom/build/_deps/assimp_source-subbuild/assimp_source-populate-prefix/src/assimp_source-populate-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "C:/Users/User/Downloads/InteractiveClassroom/InteractiveClassroom/build/_deps/assimp_source-subbuild/assimp_source-populate-prefix/src/assimp_source-populate-stamp${cfgdir}") # cfgdir has leading slash
endif()

# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file LICENSE.rst or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION ${CMAKE_VERSION}) # this file comes with cmake

# If CMAKE_DISABLE_SOURCE_CHANGES is set to true and the source directory is an
# existing directory in our source tree, calling file(MAKE_DIRECTORY) on it
# would cause a fatal error, even though it would be a no-op.
if(NOT EXISTS "/home/jeremy/Development/openpix/_deps/rapidocr-src")
  file(MAKE_DIRECTORY "/home/jeremy/Development/openpix/_deps/rapidocr-src")
endif()
file(MAKE_DIRECTORY
  "/home/jeremy/Development/openpix/_deps/rapidocr-build"
  "/home/jeremy/Development/openpix/_deps/rapidocr-subbuild/rapidocr-populate-prefix"
  "/home/jeremy/Development/openpix/_deps/rapidocr-subbuild/rapidocr-populate-prefix/tmp"
  "/home/jeremy/Development/openpix/_deps/rapidocr-subbuild/rapidocr-populate-prefix/src/rapidocr-populate-stamp"
  "/home/jeremy/Development/openpix/_deps/rapidocr-subbuild/rapidocr-populate-prefix/src"
  "/home/jeremy/Development/openpix/_deps/rapidocr-subbuild/rapidocr-populate-prefix/src/rapidocr-populate-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/home/jeremy/Development/openpix/_deps/rapidocr-subbuild/rapidocr-populate-prefix/src/rapidocr-populate-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/home/jeremy/Development/openpix/_deps/rapidocr-subbuild/rapidocr-populate-prefix/src/rapidocr-populate-stamp${cfgdir}") # cfgdir has leading slash
endif()

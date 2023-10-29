# CMakeList.txt: sparrow 的 CMake 项目，在此处包括源代码并定义
# 项目特定的逻辑。
#
aux_source_directory(compiler compiler)

aux_source_directory( gc gc)

aux_source_directory(include include)

aux_source_directory( object object)

aux_source_directory( parser parser)

aux_source_directory( vm vm)

message("var: ${vm}")
include_directories(compiler gc include object parser vm )
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
add_library(CmakeStatic STATIC "${compiler}" "${gc}" "${include}" "${object}" "${parser}" "${vm}" "${CMAKE_CURRENT_SOURCE_DIR}/vm/opcode.inc" "${CMAKE_CURRENT_SOURCE_DIR}/vm/core.script.inc" )

link_directories("${CMAKE_BINARY_DIR}/lib")
include_directories(compiler gc include object parser vm )

# 将源代码添加到此项目的可执行文件。
add_executable (sparrow "sparrow.c" "sparrow.h")
target_link_libraries(sparrow CmakeStatic)
if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET sparrow PROPERTY CXX_STANDARD 20)
endif()

# TODO: 如有需要，请添加测试并安装目标。

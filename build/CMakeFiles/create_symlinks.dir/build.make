# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 3.31

# Delete rule output on recipe failure.
.DELETE_ON_ERROR:

#=============================================================================
# Special targets provided by cmake.

# Disable implicit rules so canonical targets will work.
.SUFFIXES:

# Disable VCS-based implicit rules.
% : %,v

# Disable VCS-based implicit rules.
% : RCS/%

# Disable VCS-based implicit rules.
% : RCS/%,v

# Disable VCS-based implicit rules.
% : SCCS/s.%

# Disable VCS-based implicit rules.
% : s.%

.SUFFIXES: .hpux_make_needs_suffix_list

# Command-line flag to silence nested $(MAKE).
$(VERBOSE)MAKESILENT = -s

#Suppress display of executed commands.
$(VERBOSE).SILENT:

# A target that is always out of date.
cmake_force:
.PHONY : cmake_force

#=============================================================================
# Set environment variables for the build.

# The shell in which to execute make rules.
SHELL = /bin/sh

# The CMake executable.
CMAKE_COMMAND = /home/fangfang/.local/lib/python3.10/site-packages/cmake/data/bin/cmake

# The command to remove a file.
RM = /home/fangfang/.local/lib/python3.10/site-packages/cmake/data/bin/cmake -E rm -f

# Escaping for special characters.
EQUALS = =

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = /home/fangfang/cpp/WikiLex_Searcher/WikiLex-Searcher

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /home/fangfang/cpp/WikiLex_Searcher/WikiLex-Searcher/build

# Utility rule file for create_symlinks.

# Include any custom commands dependencies for this target.
include CMakeFiles/create_symlinks.dir/compiler_depend.make

# Include the progress variables for this target.
include CMakeFiles/create_symlinks.dir/progress.make

CMakeFiles/create_symlinks:
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --blue --bold --progress-dir=/home/fangfang/cpp/WikiLex_Searcher/WikiLex-Searcher/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Creating symbolic links for 'src' and 'data' and 'model' directories in the build directory"
	/home/fangfang/.local/lib/python3.10/site-packages/cmake/data/bin/cmake -E create_symlink /home/fangfang/cpp/WikiLex_Searcher/WikiLex-Searcher/src /home/fangfang/cpp/WikiLex_Searcher/WikiLex-Searcher/build/src
	/home/fangfang/.local/lib/python3.10/site-packages/cmake/data/bin/cmake -E create_symlink /home/fangfang/cpp/WikiLex_Searcher/WikiLex-Searcher/data /home/fangfang/cpp/WikiLex_Searcher/WikiLex-Searcher/build/data
	/home/fangfang/.local/lib/python3.10/site-packages/cmake/data/bin/cmake -E create_symlink /home/fangfang/cpp/WikiLex_Searcher/WikiLex-Searcher/model /home/fangfang/cpp/WikiLex_Searcher/WikiLex-Searcher/build/model
	/home/fangfang/.local/lib/python3.10/site-packages/cmake/data/bin/cmake -E create_symlink /home/fangfang/cpp/WikiLex_Searcher/WikiLex-Searcher/lemwwwroot /home/fangfang/cpp/WikiLex_Searcher/WikiLex-Searcher/build/lemwwwroot

CMakeFiles/create_symlinks.dir/codegen:
.PHONY : CMakeFiles/create_symlinks.dir/codegen

create_symlinks: CMakeFiles/create_symlinks
create_symlinks: CMakeFiles/create_symlinks.dir/build.make
.PHONY : create_symlinks

# Rule to build all files generated by this target.
CMakeFiles/create_symlinks.dir/build: create_symlinks
.PHONY : CMakeFiles/create_symlinks.dir/build

CMakeFiles/create_symlinks.dir/clean:
	$(CMAKE_COMMAND) -P CMakeFiles/create_symlinks.dir/cmake_clean.cmake
.PHONY : CMakeFiles/create_symlinks.dir/clean

CMakeFiles/create_symlinks.dir/depend:
	cd /home/fangfang/cpp/WikiLex_Searcher/WikiLex-Searcher/build && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/fangfang/cpp/WikiLex_Searcher/WikiLex-Searcher /home/fangfang/cpp/WikiLex_Searcher/WikiLex-Searcher /home/fangfang/cpp/WikiLex_Searcher/WikiLex-Searcher/build /home/fangfang/cpp/WikiLex_Searcher/WikiLex-Searcher/build /home/fangfang/cpp/WikiLex_Searcher/WikiLex-Searcher/build/CMakeFiles/create_symlinks.dir/DependInfo.cmake "--color=$(COLOR)"
.PHONY : CMakeFiles/create_symlinks.dir/depend


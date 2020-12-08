###########################################################################
# Copyright 2015, 2016, 2017 IoT.bzh
#
# author: Romain Forlot <romain.forlot@iot.bzh>
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#	http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
###########################################################################
CMAKE_MINIMUM_REQUIRED(VERSION 3.3)

# Project Info
# ------------------
set(PROJECT_NAME signal-composer-plugins-template)
set(PROJECT_LANGUAGES "C;CXX")

project(${PROJECT_NAME} DESCRIPTION "Build and install custom signal-composer-binding plugins")

include(FindPkgConfig)
include(GNUInstallDirs)
include(CTest)
enable_testing()

set(SIGNAL_COMPOSER_BINDING_NAME "signal-composer-binding" CACHE STRING "Name of the signal-composer binding. Needed to install the config and plugin at the correct place")

add_compile_definitions("AFB_BINDING_VERSION=3")

# Compilation Mode (DEBUG, RELEASE)
# ----------------------------------
set(CMAKE_BUILD_TYPE "RELEASE" CACHE STRING "Default Build variant chosen. (Overwritten by cli if given)")

# Prefix path where will be installed the files
# Default: /usr/local (need root permission to write in)
# ------------------------------------------------------
set(INSTALL_PREFIX $ENV{HOME}/opt)

# Customize link option
# -----------------------------
list (APPEND link_libraries -pthread)

pkg_check_modules(signal_composer REQUIRED signal-composer-binding)
pkg_check_modules(helpers REQUIRED afb-libhelpers)
pkg_check_modules(controller REQUIRED afb-libcontroller)

include_directories("${signal_composer_INCLUDEDIR}")
include_directories("${helpers_INCLUDEDIR}")
include_directories("${controller_INCLUDEDIR}")

link_libraries(${signal_composer_LIBRARIES} -L${signal_composer_LIBDIR})
link_libraries(${helpers_LIBRARIES} -L${helpers_LIBDIR})
link_libraries(${controller_LIBRARIES} -L${controller_LIBDIR})

set(TARGETS_LIST "" CACHE INTERNAL "Enumarate targets")

set(TARGET_NAME generated_plugin)

	# Define targets
	ADD_LIBRARY(${TARGET_NAME} MODULE ${TARGET_NAME}.cpp)
	get_target_property(dir ${TARGET_NAME} SOURCE_DIR)
	target_sources(${TARGET_NAME} PRIVATE ${TARGET_NAME}.cpp)

	# Config json file
	CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/control-signal-composer-config.json.in 
		${CMAKE_CURRENT_SOURCE_DIR}/control-signal-composer-config.json)

	# Alsa Plugin properties
	SET_TARGET_PROPERTIES(${TARGET_NAME} PROPERTIES
		LABELS "PLUGIN"
		PREFIX ""
		SUFFIX ".ctlso"
		OUTPUT_NAME ${TARGET_NAME}
	)

	get_target_property(pre ${TARGET_NAME} PREFIX)
	get_target_property(out ${TARGET_NAME} OUTPUT_NAME)
	get_target_property(suf ${TARGET_NAME} SUFFIX)
	get_target_property(bdir ${TARGET_NAME} BINARY_DIR)

	add_custom_target(install_${TARGET_NAME}
	COMMAND install -m644 ${bdir}/${pre}${out}${suf} ${CMAKE_INSTALL_PREFIX}/${SIGNAL_COMPOSER_BINDING_NAME}/lib/plugins
	COMMAND
	)
	add_dependencies(install_${TARGET_NAME} ${TARGET_NAME})


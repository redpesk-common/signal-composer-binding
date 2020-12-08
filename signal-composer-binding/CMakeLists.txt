###########################################################################
# Copyright 2015, 2016, 2017 IoT.bzh
#
# author: Fulup Ar Foll <fulup@iot.bzh>
# contrib: Romain Forlot <romain.forlot@iot.bzh>
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
###########################################################################

INSTALL(FILES 	signal-composer.hpp
				source.hpp
				signal.hpp
				observer-pattern.hpp
				clientApp.hpp
				DESTINATION ${CMAKE_INSTALL_FULL_INCLUDEDIR})

# Add target to project dependency list
PROJECT_TARGET_ADD(signal-composer)

	# Define project Targets
	add_library(${TARGET_NAME} SHARED ${TARGET_NAME}.cpp source.cpp signal.cpp clientApp.cpp)

	# Binder exposes a unique public entry point
	SET_TARGET_PROPERTIES(${TARGET_NAME} PROPERTIES
	LABELS "LIBRARY"
	IMPORTED_LOCATION ${CMAKE_CURRENT_BINARY_DIR}/lib${TARGET_NAME}.so
	)

	TARGET_INCLUDE_DIRECTORIES(${TARGET_NAME}
		PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
	)

# Add target to project dependency list
PROJECT_TARGET_ADD(signal-composer-binding)
	CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/${TARGET_NAME}-apidef.json.in ${CMAKE_CURRENT_SOURCE_DIR}/${TARGET_NAME}-apidef.json)
	# Define project Targets
	add_library(${TARGET_NAME} MODULE ${TARGET_NAME}.cpp)

	TARGET_INCLUDE_DIRECTORIES(${TARGET_NAME}
		PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
	)

	TARGET_LINK_LIBRARIES(${TARGET_NAME} signal-composer)

	# Binder exposes a unique public entry point
	SET_TARGET_PROPERTIES(${TARGET_NAME} PROPERTIES
		PREFIX "afb-"
		LABELS "BINDINGV3"
		LINK_FLAGS ${BINDINGS_LINK_FLAG}
		INSTALL_RPATH "$ORIGIN"
		OUTPUT_NAME ${TARGET_NAME}
	)

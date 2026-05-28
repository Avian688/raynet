cmake_policy(SET CMP0048 NEW)
cmake_minimum_required(VERSION 3.5...3.18)
project(rlomnetbind VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_DEBUG_POSTFIX _dbg)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5 CACHE STRING "Minimum policy version for vendored CMake projects")

# For each simlib in the simlib directory, add it to link libraries
file(GLOB SIMLIB_DIRS RELATIVE ${PROJECT_SOURCE_DIR}/simlibs ${PROJECT_SOURCE_DIR}/simlibs/*)
foreach(lib ${SIMLIB_DIRS})
    if (IS_DIRECTORY ${PROJECT_SOURCE_DIR}/simlibs/${lib}/src)
        # Append library name to LIB_NAMES (used for target_link_libraries)
        list(APPEND LIB_NAMES ${lib})

        # Append library path to LIB_PATHS (used for link_directories)
        set(LIB_PATH ${PROJECT_SOURCE_DIR}/simlibs/${lib}/src)
        list(APPEND LIB_PATHS ${LIB_PATH})

        # Set the path for this specific library (may not be needed)
        string(TOUPPER ${lib} LIB_NAME)
        set(${LIB_NAME}_PATH ${LIB_PATH})
        message(STATUS "${LIB_NAME}_PATH = ${${LIB_NAME}_PATH}")
    endif()
endforeach()

# Link OMNeT++ first so dependent sample roots can be derived from the selected install.
if(DEFINED ENV{OMNETPP_ROOT} AND NOT OMNETPP_ROOT)
    set(OMNETPP_ROOT "$ENV{OMNETPP_ROOT}" CACHE PATH "Path to OMNeT++ root directory")
endif()
find_package(OmnetPP 6.0.1 REQUIRED)
include(OmnetppHelpers)

set(OMNETPP_SAMPLES_ROOT "$ENV{OMNETPP_SAMPLES_ROOT}" CACHE PATH "OMNeT++ samples root")
if(NOT OMNETPP_SAMPLES_ROOT)
    set(OMNETPP_SAMPLES_ROOT "${OMNETPP_ROOT}/samples" CACHE PATH "OMNeT++ samples root" FORCE)
endif()

set(INET_ROOT "$ENV{INET_ROOT}" CACHE PATH "INET root directory")
if(NOT INET_ROOT)
    file(GLOB _inet_candidates LIST_DIRECTORIES true "${OMNETPP_SAMPLES_ROOT}/inet*")
    list(SORT _inet_candidates)
    list(REVERSE _inet_candidates)
    foreach(_inet_candidate ${_inet_candidates})
        if(IS_DIRECTORY "${_inet_candidate}/src")
            set(INET_ROOT "${_inet_candidate}" CACHE PATH "INET root directory" FORCE)
            break()
        endif()
    endforeach()
endif()
if(NOT IS_DIRECTORY "${INET_ROOT}/src")
    message(FATAL_ERROR "INET_ROOT must point at an INET checkout with a src directory. Set INET_ROOT or pass -DINET_ROOT=/path/to/inet.")
endif()
set(INET_MODEL_PATH "${INET_ROOT}/src")

set(TCPGOODPUT_ROOT "$ENV{TCPGOODPUT_ROOT}" CACHE PATH "tcpGoodputApplications root directory")
if(NOT TCPGOODPUT_ROOT)
    set(TCPGOODPUT_ROOT "${OMNETPP_SAMPLES_ROOT}/tcpGoodputApplications" CACHE PATH "tcpGoodputApplications root directory" FORCE)
endif()
set(TCPGOODPUT_PATH "${TCPGOODPUT_ROOT}/src") # tcpGoodputApplications creates goodput vectors for plotting via custom TcpApp.
list(APPEND LIB_PATHS ${TCPGOODPUT_PATH})
list(APPEND LIB_NAMES tcpGoodputApplications)

set(LEOSATELLITES_ROOT "$ENV{LEOSATELLITES_ROOT}" CACHE PATH "leosatellites root directory")
if(NOT LEOSATELLITES_ROOT)
    set(LEOSATELLITES_ROOT "${OMNETPP_SAMPLES_ROOT}/leosatellites" CACHE PATH "leosatellites root directory" FORCE)
endif()
set(LEOSATELLITES_PATH "${LEOSATELLITES_ROOT}/src") # leosatellites contains features that ORBTCPEXPERIMENTS needs.
list(APPEND LIB_PATHS ${LEOSATELLITES_PATH})
list(APPEND LIB_NAMES leosatellites)

set(ORBTCPEXPERIMENTS_ROOT "$ENV{ORBTCPEXPERIMENTS_ROOT}" CACHE PATH "orbtcpExperiments root directory")
if(NOT ORBTCPEXPERIMENTS_ROOT)
    set(ORBTCPEXPERIMENTS_ROOT "${OMNETPP_SAMPLES_ROOT}/orbtcpExperiments" CACHE PATH "orbtcpExperiments root directory" FORCE)
endif()
set(ORBTCPEXPERIMENTS_PATH "${ORBTCPEXPERIMENTS_ROOT}/src") # Aiden's experiment suite with custom components.
list(APPEND LIB_PATHS ${ORBTCPEXPERIMENTS_PATH})
list(APPEND LIB_NAMES orbtcpExperiments)

foreach(_required_sample_path ${TCPGOODPUT_PATH} ${LEOSATELLITES_PATH} ${ORBTCPEXPERIMENTS_PATH})
    if(NOT IS_DIRECTORY "${_required_sample_path}")
        message(FATAL_ERROR "Required sample library path does not exist: ${_required_sample_path}")
    endif()
endforeach()

if(NOT APPLE)
    set(NO_AS_NEEDED_FLAG "-Wl,--no-as-needed")
endif()


# Declare Release libraries
set(MY_LIB_LINK_LIBRARIES_RELEASE optimized oppsim optimized oppcommon optimized oppcmdenv optimized Cmdrlenv optimized oppnedxml optimized oppenvir optimized ${NO_AS_NEEDED_FLAG} INET) # Simlibs are automated now! But for reference: optimized TcpPaced optimized RLComponents optimized cartpole optimized JamesLib
message(MY_LIB_LINK_LIBRARIES_RELEASE IS ${MY_LIB_LINK_LIBRARIES_RELEASE})
foreach(lib_name ${LIB_NAMES})
    list(APPEND MY_LIB_LINK_LIBRARIES_RELEASE "optimized")
    if(NO_AS_NEEDED_FLAG)
        list(APPEND MY_LIB_LINK_LIBRARIES_RELEASE "${NO_AS_NEEDED_FLAG}")
    endif()
    list(APPEND MY_LIB_LINK_LIBRARIES_RELEASE "${lib_name}")
endforeach()
message(MY_LIB_LINK_LIBRARIES_RELEASE IS ${MY_LIB_LINK_LIBRARIES_RELEASE})

# Declare debug libraries
set(MY_LIB_LINK_LIBRARIES_DEBUG debug oppsim_dbg debug oppcommon_dbg debug oppcmdenv_dbg debug Cmdrlenv debug oppnedxml_dbg debug oppenvir_dbg debug ${NO_AS_NEEDED_FLAG} INET_dbg) # Simlibs are automated now! But for reference: optimized TcpPaced optimized RLComponents optimized cartpole optimized JamesLib
message(MY_LIB_LINK_LIBRARIES_DEBUG IS ${MY_LIB_LINK_LIBRARIES_DEBUG})
foreach(lib_name ${LIB_NAMES})
    list(APPEND MY_LIB_LINK_LIBRARIES_DEBUG "debug")
    if(NO_AS_NEEDED_FLAG)
        list(APPEND MY_LIB_LINK_LIBRARIES_DEBUG "${NO_AS_NEEDED_FLAG}")
    endif()
    list(APPEND MY_LIB_LINK_LIBRARIES_DEBUG "${lib_name}_dbg")
endforeach()
message(MY_LIB_LINK_LIBRARIES_DEBUG IS ${MY_LIB_LINK_LIBRARIES_DEBUG})

add_compile_options(-fpermissive -fno-omit-frame-pointer -fPIC)
add_compile_definitions(HAVE_SWAPCONTEXT)
find_package(Python3 3.12 EXACT REQUIRED COMPONENTS Interpreter Development)
include_directories(${Python3_INCLUDE_DIRS} ${PROJECT_SOURCE_DIR}/src ${PROJECT_SOURCE_DIR}/extern/pybind11/include ${OMNETPP_INCLUDE_DIR} ${OMNETPP_ROOT}/include ${OMNETPP_ROOT}/src ${LIB_PATHS})
link_directories(${OMNETPP_ROOT}/lib ${INET_MODEL_PATH} ${LIB_PATHS})

# Build Raynet
add_library(rlomnet SHARED ${PROJECT_SOURCE_DIR}/src/GymApi.cc)
add_library(Cmdrlenv SHARED ${PROJECT_SOURCE_DIR}/src/Cmdrlenv.cc)
add_executable(testcpp ${PROJECT_SOURCE_DIR}/src/gymApiTest.cc)

add_subdirectory(extern/pybind11)
pybind11_add_module(omnetbind MODULE ${PROJECT_SOURCE_DIR}/src/GymApiBind.cc)
set_target_properties(omnetbind PROPERTIES DEBUG_POSTFIX "") # Force omnetbind.so to not have the _dbg postfix. This allows the python import to continue working without changes.
message(PROJECT_SOURCE_DIR="${CMAKE_BUILD_TYPE}")

if(CMAKE_BUILD_TYPE STREQUAL release)
    target_link_libraries(Cmdrlenv oppcmdenv oppsim oppcommon oppnedxml oppenvir RLComponents)
    target_link_libraries(omnetbind PRIVATE ${MY_LIB_LINK_LIBRARIES_RELEASE})
    target_link_libraries(rlomnet PRIVATE ${MY_LIB_LINK_LIBRARIES_RELEASE})
    target_link_libraries(omnetbind PRIVATE rlomnet)
    target_link_libraries(testcpp rlomnet oppsim)
elseif(CMAKE_BUILD_TYPE STREQUAL debug)
    target_link_libraries(Cmdrlenv oppcmdenv_dbg oppsim_dbg oppcommon_dbg oppnedxml_dbg oppenvir_dbg RLComponents_dbg)
    set_target_properties(Cmdrlenv PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})
    target_link_libraries(omnetbind PRIVATE rlomnet)
    target_link_libraries(rlomnet PRIVATE ${MY_LIB_LINK_LIBRARIES_DEBUG})
    set_target_properties(testcpp PROPERTIES DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX})
    target_link_libraries(testcpp rlomnet oppsim_dbg)
endif()

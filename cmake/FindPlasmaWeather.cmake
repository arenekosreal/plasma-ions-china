set(_PLASMA_WEATHER_DATAENGINE_LAST_VERSION 6.4.5) # 6.4.5 is the last KDE6 version using DataEngine.
set(_PLASMA_WEATHER_QT_MIN_VERSION 6.5.0) # 6.5.0 is the first used Qt6 version of KDE6.

if(NOT QT_MAJOR_VERSION)
    string(REGEX REPLACE "^([0-9]+)\\.([0-9]+)\\.([0-9]+)" "\\1" QT_MAJOR_VERSION "${_PLASMA_WEATHER_QT_MIN_VERSION}")
endif()
if(QT_MAJOR_VERSION LESS 6)
    message(FATAL_ERROR "Qt < 6 is not supported.")
endif()

include(FindPackageHandleStandardArgs)

find_package_check_version("${_PLASMA_WEATHER_DATAENGINE_LAST_VERSION}" _PLASMA_WEATHER_LEGACY_ALLOWED
                           HANDLE_VERSION_RANGE)

if(Ion IN_LIST PlasmaWeather_FIND_COMPONENTS AND NOT Data IN_LIST PlasmaWeather_FIND_COMPONENTS)
    list(APPEND PlasmaWeather_FIND_COMPONENTS Data)
endif()

# plasma_weather_target(COMPONENT <component-name> HEADER <header> LIBRARY <library> [REQUIRED])
# Create a target based on component name.
function(plasma_weather_target)
    set(_PLASMA_WEATHER_TARGET_ARGUMENT_NAMES_OPTIONS "REQUIRED")
    set(_PLASMA_WEATHER_TARGET_ARGUMENT_NAMES_ONE_VALUES "COMPONENT" "HEADER" "LIBRARY")
    set(_PLASMA_WEATHER_TARGET_ARGUMENT_NAMES_MULTIPLE_VALUES "")
    cmake_parse_arguments(PARSE_ARGV 0 _PLASMA_WEATHER_TARGET
                          "${_PLASMA_WEATHER_TARGET_ARGUMENT_NAMES_OPTIONS}"            # Options
                          "${_PLASMA_WEATHER_TARGET_ARGUMENT_NAMES_ONE_VALUES}"         # Arguments with single value
                          "${_PLASMA_WEATHER_TARGET_ARGUMENT_NAMES_MULTIPLE_VALUES}")   # Arguments with multiple values
    set(${CMAKE_FIND_PACKAGE_NAME}_${_PLASMA_WEATHER_TARGET_COMPONENT}_FOUND FALSE PARENT_SCOPE)
    find_path(_PLASMA_WEATHER_TARGET_${_PLASMA_WEATHER_TARGET_COMPONENT}_INCLUDE_ROOT
              NAMES "${_PLASMA_WEATHER_TARGET_HEADER}"
              DOC "Include root path of ${_PLASMA_WEATHER_TARGET_COMPONENT}")
    find_library(_PLASMA_WEATHER_TARGET_${_PLASMA_WEATHER_TARGET_COMPONENT}_LIBRARY
                 NAMES "${_PLASMA_WEATHER_TARGET_LIBRARY}"
                 DOC "Library file path of ${_PLASMA_WEATHER_TARGET_COMPONENT}")
    mark_as_advanced(_PLASMA_WEATHER_TARGET_${_PLASMA_WEATHER_TARGET_COMPONENT}_INCLUDE_ROOT _PLASMA_WEATHER_TARGET_${_PLASMA_WEATHER_TARGET_COMPONENT}_LIBRARY)
    if(_PLASMA_WEATHER_TARGET_${_PLASMA_WEATHER_TARGET_COMPONENT}_INCLUDE_ROOT AND _PLASMA_WEATHER_TARGET_${_PLASMA_WEATHER_TARGET_COMPONENT}_LIBRARY)
        cmake_path(REMOVE_FILENAME _PLASMA_WEATHER_TARGET_HEADER OUTPUT_VARIABLE _PLASMA_WEATHER_TARGET_INCLUDE_RELATIVE_DIR)
        cmake_path(ABSOLUTE_PATH _PLASMA_WEATHER_TARGET_${_PLASMA_WEATHER_TARGET_COMPONENT}_INCLUDE_ROOT OUTPUT_VARIABLE _PLASMA_WEATHER_TARGET_INCLUDE_ROOT)
        cmake_path(ABSOLUTE_PATH _PLASMA_WEATHER_TARGET_${_PLASMA_WEATHER_TARGET_COMPONENT}_LIBRARY OUTPUT_VARIABLE _PLASMA_WEATHER_TARGET_LIBRARY)
        file(REAL_PATH "${_PLASMA_WEATHER_TARGET_LIBRARY}" _PLASMA_WEATHER_TARGET_LIBRARY_REAL)
        string(REGEX MATCH "[0-9]+\\.[0-9]+\\.[0-9]+$" _PLASMA_WEATHER_TARGET_LIBRARY_VERSION "${_PLASMA_WEATHER_TARGET_LIBRARY_REAL}")
        set(_PLASMA_WEATHER_TARGET_NAME Plasma::Weather::${_PLASMA_WEATHER_TARGET_COMPONENT})
        add_library(${_PLASMA_WEATHER_TARGET_NAME} SHARED IMPORTED GLOBAL)
        set(_PLASMA_WEATHER_TARGET_INCLUDE_DIR "${_PLASMA_WEATHER_TARGET_INCLUDE_ROOT}/${_PLASMA_WEATHER_TARGET_INCLUDE_RELATIVE_DIR}")
        set_target_properties(${_PLASMA_WEATHER_TARGET_NAME}
                              PROPERTIES IMPORTED_LOCATION "${_PLASMA_WEATHER_TARGET_LIBRARY}"
                                         INTERFACE_INCLUDE_DIRECTORIES "${_PLASMA_WEATHER_TARGET_INCLUDE_DIR}")
        find_package_check_version("${_PLASMA_WEATHER_TARGET_LIBRARY_VERSION}" _PLASMA_WEATHER_TARGET_LIBRARY_VERSION_CHECK_RESULT
                                   HANDLE_VERSION_RANGE
                                   RESULT_MESSAGE_VARIABLE _PLASMA_WEATHER_TARGET_LIBRARY_VERSION_CHECK_RESULT_MESSAGE)
        if(_PLASMA_WEATHER_TARGET_LIBRARY_VERSION_CHECK_RESULT)
            if(NOT ${CMAKE_FIND_PACKAGE_NAME}_FIND_QUIETLY)
                set(_PLASMA_WEATHER_TARGET_FOUND_MSG_HEAD "Found ${CMAKE_FIND_PACKAGE_NAME} ${_PLASMA_WEATHER_TARGET_COMPONENT}: ${_PLASMA_WEATHER_TARGET_LIBRARY}")
                message(STATUS "${_PLASMA_WEATHER_TARGET_FOUND_MSG_HEAD} ${_PLASMA_WEATHER_TARGET_LIBRARY_VERSION_CHECK_RESULT_MESSAGE}")
            endif()
            set(${CMAKE_FIND_PACKAGE_NAME}_${_PLASMA_WEATHER_TARGET_COMPONENT}_LIBRARY "${_PLASMA_WEATHER_TARGET_LIBRARY}" PARENT_SCOPE)
            set(${CMAKE_FIND_PACKAGE_NAME}_${_PLASMA_WEATHER_TARGET_COMPONENT}_INCLUDE_DIR "${_PLASMA_WEATHER_TARGET_INCLUDE_DIR}" PARENT_SCOPE)
            set(${CMAKE_FIND_PACKAGE_NAME}_${_PLASMA_WEATHER_TARGET_COMPONENT}_VERSION "${_PLASMA_WEATHER_TARGET_LIBRARY_VERSION}" PARENT_SCOPE)
            set(${CMAKE_FIND_PACKAGE_NAME}_INCLUDE_DIRS ${${CMAKE_FIND_PACKAGE_NAME}_INCLUDE_DIRS}
                                                        ${${CMAKE_FIND_PACKAGE_NAME}_${_PLASMA_WEATHER_TARGET_COMPONENT}_INCLUDE_DIR} PARENT_SCOPE)
            set(${CMAKE_FIND_PACKAGE_NAME}_LIBRARIES ${${CMAKE_FIND_PACKAGE_NAME}_LIBRARIES}
                                                     ${${CMAKE_FIND_PACKAGE_NAME}_${_PLASMA_WEATHER_TARGET_COMPONENT}_LIBRARY} PARENT_SCOPE)
            set(${CMAKE_FIND_PACKAGE_NAME}_${_PLASMA_WEATHER_TARGET_COMPONENT}_FOUND TRUE PARENT_SCOPE)
        else()
            message($<IF:$<_PLASMA_WEATHER_TARGET_REQUIRED>,FATAL_ERROR,WARNING>
                    "${CMAKE_FIND_PACKAGE_NAME} ${_PLASMA_WEATHER_TARGET_COMPONENT} was not found: ${_PLASMA_WEATHER_TARGET_LIBRARY_VERSION_CHECK_RESULT_MESSAGE}")
        endif()
    endif()
endfunction()

if(Data IN_LIST PlasmaWeather_FIND_COMPONENTS)
    plasma_weather_target(COMPONENT Data HEADER "plasma/weather/plasmaweatherdata_export.h" LIBRARY "plasmaweatherdata" $<PlasmaWeather_FIND_REQUIRED_Data:REQUIRED>)
    if(TARGET Plasma::Weather::Data)
        find_package(Qt${QT_MAJOR_VERSION} ${_PLASMA_WEATHER_QT_MIN_VERSION} COMPONENTS Qml REQUIRED)
        target_link_libraries(Plasma::Weather::Data INTERFACE Qt::Qml)
    endif()
endif()

if(Ion IN_LIST PlasmaWeather_FIND_COMPONENTS)
    plasma_weather_target(COMPONENT Ion HEADER "plasma/weather/ion.h" LIBRARY "plasmaweatherion" $<PlasmaWeather_FIND_REQUIRED_Ion:REQUIRED>)
    if(TARGET Plasma::Weather::Ion)
        set(PlasmaWeather_VERSION "${PlasmaWeather_Ion_VERSION}")
        target_link_libraries(Plasma::Weather::Ion INTERFACE Plasma::Weather::Data)
    endif()
endif()

if(IonLegacy IN_LIST PlasmaWeather_FIND_COMPONENTS)
    if(_PLASMA_WEATHER_LEGACY_ALLOWED)
        plasma_weather_target(COMPONENT IonLegacy HEADER "plasma5support/weather/ion.h" LIBRARY "weather_ion" $<PlasmaWeather_FIND_REQUIRED_IonLegacy:REQUIRED>)
        if(TARGET Plasma::Weather::IonLegacy)
            find_package(Plasma5Support REQUIRED)
            target_link_libraries(Plasma::Weather::IonLegacy INTERFACE Plasma::Plasma5Support)
            if(NOT PlasmaWeather_VERSION)
                # PlasmaWeather_IonLegacy_VERSION is not equals to actual version.
                set(PlasmaWeather_VERSION ${Plasma5Support_VERSION})
            endif()
        endif()
    endif()
endif()

find_package_handle_standard_args(PlasmaWeather
                                  VERSION_VAR PlasmaWeather_VERSION
                                  HANDLE_VERSION_RANGE
                                  HANDLE_COMPONENTS)

mark_as_advanced(_PLASMA_WEATHER_DATAENGINE_LAST_VERSION _PLASMA_WEATHER_LEGACY_ALLOWED _PLASMA_WEATHER_QT_MIN_VERSION)

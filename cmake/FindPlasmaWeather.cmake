if(NOT QT_MAJOR_VERSION)
    set(QT_MAJOR_VERSION 6)
endif()
if(QT_MAJOR_VERSION LESS 6)
    message(FATAL_ERROR "Qt < 6 is not supported.")
endif()

set(_PLASMA_WEATHER_DATAENGINE_LAST_VERSION 6.4.5) # 6.4.5 is the last version using DataEngine.

include(FindPackageHandleStandardArgs)
find_package_check_version("${_PLASMA_WEATHER_DATAENGINE_LAST_VERSION}" _PLASMA_WEATHER_LEGACY_ALLOWED
                           HANDLE_VERSION_RANGE)
set(PlasmaWeather_Ion_FOUND FALSE)

find_path(_PLASMA_WEATHER_ION_INCLUDE_ROOT "plasma/weatherion/ion.h")
find_library(_PLASMA_WEATHER_ION_LIBRARY "plasmaweatherion")
if(_PLASMA_WEATHER_ION_INCLUDE_ROOT AND _PLASMA_WEATHER_ION_LIBRARY)
    set(PlasmaWeather_Data_FOUND FALSE)
    find_path(_PLASMA_WEATHER_DATA_INCLUDE_ROOT "plasma/weatherdata/plasmaweatherdata_export.h" REQUIRED)
    find_library(_PLASMA_WEATHER_DATA_LIBRARY "plasmaweatherdata" REQUIRED)
    cmake_path(ABSOLUTE_PATH _PLASMA_WEATHER_DATA_INCLUDE_ROOT)
    cmake_path(ABSOLUTE_PATH _PLASMA_WEATHER_DATA_LIBRARY)
    find_package(Qt${QT_MAJOR_VERSION} COMPONENTS Qml REQUIRED)
    add_library(PlasmaWeather::Data SHARED IMPORTED GLOBAL)
    set_target_properties(PlasmaWeather::Data
                          PROPERTIES IMPORTED_LOCATION "${_PLASMA_WEATHER_DATA_LIBRARY}"
                                     INTERFACE_INCLUDE_DIRECTORIES "${_PLASMA_WEATHER_DATA_INCLUDE_ROOT}/plasma/weatherdata")
    target_link_libraries(PlasmaWeather::Data INTERFACE Qt::Qml)
    file(REAL_PATH "${_PLASMA_WEATHER_DATA_LIBRARY}" _PLASMA_WEATHER_DATA_LIBRARY_REAL_PATH)
    string(REGEX MATCH "[0-9]+\\.[0-9]+\\.[0-9]+$" _PLASMA_WEATHER_DATA_VERSION "${_PLASMA_WEATHER_DATA_LIBRARY_REAL_PATH}")

    set(PlasmaWeather_Data_INCLUDE_DIR "${_PLASMA_WEATHER_DATA_INCLUDE_ROOT}/plasma/weatherdata")
    set(PlasmaWeather_Data_LIBRARY "${_PLASMA_WEATHER_DATA_LIBRARY}")
    set(PlasmaWeather_Data_VERSION "${_PLASMA_WEATHER_DATA_VERSION}")
    find_package_check_version("${PlasmaWeather_Data_VERSION}" _PLASMA_WEATHER_DATA_VERSION_CHECK_RESULT
                               HANDLE_VERSION_RANGE
                               RESULT_MESSAGE_VARIABLE _PLASMA_WEATHER_DATA_VERSION_CHECK_RESULT_MESSAGE)
    if(_PLASMA_WEATHER_DATA_VERSION_CHECK_RESULT)
        if (NOT PlasmaWeather_FIND_QUIETLY)
            message(STATUS "Found ${CMAKE_FIND_PACKAGE_NAME} Data: ${PlasmaWeather_Data_LIBRARY} ${_PLASMA_WEATHER_DATA_VERSION_CHECK_RESULT_MESSAGE}")
        endif()
        list(APPEND PlasmaWeather_LIBRARIES "${PlasmaWeather_Data_LIBRARY}")
        list(APPEND PlasmaWeather_INCLUDE_DIRS "${PlasmaWeather_Data_INCLUDE_DIR}")
        set(PlasmaWeather_Data_FOUND TRUE)
    elseif(PlasmaWeather_FIND_REQUIRED_Data)
        message(FATAL_ERROR "${CMAKE_FIND_PACKAGE_NAME} Data was not found: ${_PLASMA_WEATHER_DATA_VERSION_CHECK_RESULT_MESSAGE}")
    else()
        message(WARNING "${CMAKE_FIND_PACKAGE_NAME} Data was not found: ${_PLASMA_WEATHER_DATA_VERSION_CHECK_RESULT_MESSAGE}")
    endif()
    cmake_path(ABSOLUTE_PATH _PLASMA_WEATHER_ION_INCLUDE_ROOT)
    cmake_path(ABSOLUTE_PATH _PLASMA_WEATHER_ION_LIBRARY)
    add_library(PlasmaWeather::Ion SHARED IMPORTED GLOBAL)
    file(REAL_PATH "${_PLASMA_WEATHER_ION_LIBRARY}" _PLASMA_WEATHER_ION_LIBRARY_REAL_PATH)
    string(REGEX MATCH "[0-9]+\\.[0-9]+\\.[0-9]+$" _PLASMA_WEATHER_ION_VERSION "${_PLASMA_WEATHER_ION_LIBRARY_REAL_PATH}")
    set_target_properties(PlasmaWeather::Ion
                          PROPERTIES IMPORTED_LOCATION "${_PLASMA_WEATHER_ION_LIBRARY}"
                                     INTERFACE_INCLUDE_DIRECTORIES "${_PLASMA_WEATHER_ION_INCLUDE_ROOT}/plasma/weatherion")
    target_link_libraries(PlasmaWeather::Ion INTERFACE PlasmaWeather::Data)

    set(PlasmaWeather_Ion_INCLUDE_DIR "${_PLASMA_WEATHER_ION_INCLUDE_ROOT}/plasma/weatherion")
    set(PlasmaWeather_Ion_LIBRARY "${_PLASMA_WEATHER_ION_LIBRARY}")
    set(PlasmaWeather_Ion_VERSION "${_PLASMA_WEATHER_ION_VERSION}")
    find_package_check_version("${PlasmaWeather_Ion_VERSION}" _PLASMA_WEATHER_ION_VERSION_CHECK_RESULT
                               HANDLE_VERSION_RANGE
                               RESULT_MESSAGE_VARIABLE _PLASMA_WEATHER_ION_VERSION_CHECK_RESULT_MESSAGE)
    if(_PLASMA_WEATHER_ION_VERSION_CHECK_RESULT)
        if(NOT PlasmaWeather_FIND_QUIETLY)
            message(STATUS "Found ${CMAKE_FIND_PACKAGE_NAME} Ion: ${PlasmaWeather_Ion_LIBRARY} ${_PLASMA_WEATHER_ION_VERSION_CHECK_RESULT_MESSAGE}")
        endif()
        list(APPEND PlasmaWeather_INCLUDE_DIRS "${PlasmaWeather_Ion_INCLUDE_DIR}")
        list(APPEND PlasmaWeather_LIBRARIES "${PlasmaWeather_Ion_LIBRARY}")
        set(PlasmaWeather_Ion_FOUND TRUE)
        set(PlasmaWeather_Ion_LEGACY FALSE)
    elseif(PlasmaWeather_FIND_REQUIRED_Ion)
        message(FATAL_ERROR "${CMAKE_FIND_PACKAGE_NAME} Ion was not found: ${_PLASMA_WEATHER_ION_VERSION_CHECK_RESULT_MESSAGE}")
    else()
        message(WARNING "${CMAKE_FIND_PACKAGE_NAME} Ion was not found: ${_PLASMA_WEATHER_ION_VERSION_CHECK_RESULT_MESSAGE}")
    endif()
endif()

if(NOT TARGET PlasmaWeather::Ion AND _PLASMA_WEATHER_LEGACY_ALLOWED)
    find_path(_PLASMA_WEATHER_ION_LEGACY_INCLUDE_ROOT "plasma5support/weather/ion.h")
    find_library(_PLASMA_WEATHER_ION_LEGACY_LIBRARY "weather_ion")
    if(_PLASMA_WEATHER_ION_LEGACY_INCLUDE_ROOT AND _PLASMA_WEATHER_ION_LEGACY_LIBRARY)
        cmake_path(ABSOLUTE_PATH _PLASMA_WEATHER_ION_LEGACY_INCLUDE_ROOT)
        cmake_path(ABSOLUTE_PATH _PLASMA_WEATHER_ION_LEGACY_LIBRARY)
        find_package(Plasma5Support REQUIRED)
        add_library(PlasmaWeather::Ion SHARED IMPORTED GLOBAL)
        set_target_properties(PlasmaWeather::Ion
                              PROPERTIES IMPORTED_LOCATION "${_PLASMA_WEATHER_ION_LEGACY_LIBRARY}"
                                         INTERFACE_INCLUDE_DIRECTORIES "${_PLASMA_WEATHER_ION_LEGACY_INCLUDE_ROOT}/plasma5support/weather")
        target_link_libraries(PlasmaWeather::Ion INTERFACE Plasma::Plasma5Support)
        file(REAL_PATH "${_PLASMA_WEATHER_ION_LEGACY_LIBRARY}" _PLASMA_WEATHER_ION_LEGACY_LIBRARY_REAL_PATH)
        string(REGEX MATCH "[0-9]+\\.[0-9]+\\.[0-9]+$" _PLASMA_WEATHER_ION_LEGACY_VERSION "${_PLASMA_WEATHER_ION_LEGACY_LIBRARY_REAL_PATH}")

        set(PlasmaWeather_Ion_INCLUDE_DIR "${_PLASMA_WEATHER_ION_LEGACY_INCLUDE_ROOT}/plasma5support/weather")
        set(PlasmaWeather_Ion_LIBRARY "${_PLASMA_WEATHER_ION_LEGACY_LIBRARY}")
        set(PlasmaWeather_Ion_VERSION "${_PLASMA_WEATHER_ION_LEGACY_VERSION}")
        set(PlasmaWeather_Ion_LEGACY TRUE)
        find_package_check_version("${PlasmaWeather_Ion_VERSION}" _PLASMA_WEATHER_ION_LEGACY_VERSION_CHECK_RESULT
                                   HANDLE_VERSION_RANGE
                                   RESULT_MESSAGE_VARIABLE _PLASMA_WEATHER_ION_LEGACY_VERSION_CHECK_RESULT_MESSAGE)
        if(_PLASMA_WEATHER_ION_LEGACY_VERSION_CHECK_RESULT)
            if(NOT PlasmaWeather_FIND_QUIETLY)
                message(STATUS "Found ${CMAKE_FIND_PACKAGE_NAME} Ion (DataEngine version): ${PlasmaWeather_Ion_LIBRARY} ${_PLASMA_WEATHER_ION_LEGACY_VERSION_CHECK_RESULT_MESSAGE}")
            endif()
            list(APPEND PlasmaWeather_INCLUDE_DIRS "${PlasmaWeather_Ion_INCLUDE_DIR}")
            list(APPEND PlasmaWeather_LIBRARIES "${PlasmaWeather_Ion_LIBRARY}")
            set(PlasmaWeather_Ion_FOUND TRUE)
            set(PlasmaWeather_Ion_LEGACY TRUE)
        elseif(PlasmaWeather_FIND_REQUIRED_Ion)
            message(FATAL_ERROR "${CMAKE_FIND_PACKAGE_NAME} Ion (DataEngine version) was not found: ${_PLASMA_WEATHER_ION_LEGACY_VERSION_CHECK_RESULT_MESSAGE}")
        else()
            message(WARNING "${CMAKE_FIND_PACKAGE_NAME} Ion (DataEngine version) was not found: ${_PLASMA_WEATHER_ION_LEGACY_VERSION_CHECK_RESULT_MESSAGE}")
        endif()
    endif()
endif()

set(PlasmaWeather_VERSION ${PlasmaWeather_Ion_VERSION})
find_package_handle_standard_args(PlasmaWeather
                                  VERSION_VAR PlasmaWeather_VERSION
                                  HANDLE_VERSION_RANGE
                                  HANDLE_COMPONENTS)

mark_as_advanced(_PLASMA_WEATHER_DATAENGINE_LAST_VERSION _PLASMA_WEATHER_LEGACY_ALLOWED
                 _PLASMA_WEATHER_ION_INCLUDE_ROOT _PLASMA_WEATHER_ION_LIBRARY
                 _PLASMA_WEATHER_DATA_INCLUDE_ROOT _PLASMA_WEATHER_DATA_LIBRARY
                 _PLASMA_WEATHER_DATA_LIBRARY_REAL_PATH _PLASMA_WEATHER_DATA_VERSION
                 _PLASMA_WEATHER_DATA_VERSION_CHECK_RESULT _PLASMA_WEATHER_DATA_VERSION_CHECK_RESULT_MESSAGE
                 _PLASMA_WEATHER_ION_LIBRARY_REAL_PATH _PLASMA_WEATHER_ION_VERSION
                 _PLASMA_WEATHER_ION_VERSION_CHECK_RESULT _PLASMA_WEATHER_ION_VERSION_CHECK_RESULT_MESSAGE
                 _PLASMA_WEATHER_ION_LEGACY_INCLUDE_ROOT _PLASMA_WEATHER_ION_LEGACY_LIBRARY
                 _PLASMA_WEATHER_ION_LEGACY_LIBRARY_REAL_PATH _PLASMA_WEATHER_ION_LEGACY_VERSION
                 _PLASMA_WEATHER_ION_LEGACY_VERSION_CHECK_RESULT _PLASMA_WEATHER_ION_LEGACY_VERSION_CHECK_RESULT_MESSAGE)

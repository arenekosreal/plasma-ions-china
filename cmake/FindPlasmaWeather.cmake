set(_PLASMA_WEATHER_QT_MIN_VERSION 6.5.0) # 6.5.0 is the first used Qt6 version of KDE6.
mark_as_advanced(_PLASMA_WEATHER_QT_MIN_VERSION)

if(NOT QT_MAJOR_VERSION)
    string(REGEX REPLACE "^([0-9]+)\\.([0-9]+)\\.([0-9]+)" "\\1" QT_MAJOR_VERSION "${_PLASMA_WEATHER_QT_MIN_VERSION}")
endif()
if(QT_MAJOR_VERSION LESS 6)
    message(FATAL_ERROR "Qt < 6 is not supported.")
endif()

if(Ion IN_LIST PlasmaWeather_FIND_COMPONENTS AND NOT Data IN_LIST PlasmaWeather_FIND_COMPONENTS)
    list(APPEND PlasmaWeather_FIND_COMPONENTS Data)
    set(PlasmaWeather_FIND_REQUIRED_Data TRUE)
endif()

include(FindPackageHandleStandardArgs)

if(Data IN_LIST PlasmaWeather_FIND_COMPONENTS)
    find_path(PlasmaWeather_Data_INCLUDE_DIR
              "plasma/weather/plasmaweatherdata_export.h"
              DOC "Include directory path for Plasma::Weather::Data")
    find_library(PlasmaWeather_Data_LIBRARY
                 "plasmaweatherdata"
                 DOC "Library file path for Plasma::Weather::Data")
    if(PlasmaWeather_Data_INCLUDE_DIR AND PlasmaWeather_Data_LIBRARY)
        set(PlasmaWeather_Data_INCLUDE_DIR "${PlasmaWeather_Data_INCLUDE_DIR}/plasma/weather")
        file(REAL_PATH "${PlasmaWeather_Data_LIBRARY}" _PLASMA_WEATHER_Data_LIBRARY)
        string(REGEX MATCH "[0-9]+\\.[0-9]+\\.[0-9]+$" PlasmaWeather_Data_VERSION "${_PLASMA_WEATHER_Data_LIBRARY}")
        find_package_check_version("${PlasmaWeather_Data_VERSION}" _PLASMA_WEATHER_Data_VERSION_CHECK_RESULT
                                   HANDLE_VERSION_RANGE
                                   RESULT_MESSAGE_VARIABLE _PLASMA_WEATHER_Data_VERSION_CHECK_RESULT_MESSAGE)
        if(_PLASMA_WEATHER_Data_VERSION_CHECK_RESULT)
            add_library(Plasma::Weather::Data SHARED IMPORTED GLOBAL)
            set_target_properties(Plasma::Weather::Data
                                  PROPERTIES IMPORTED_LOCATION "${PlasmaWeather_Data_LIBRARY}"
                                             INTERFACE_INCLUDE_DIRECTORIES "${PlasmaWeather_Data_INCLUDE_DIR}")
        endif()
    endif()
endif()
if(TARGET Plasma::Weather::Data)
    find_package(Qt${QT_MAJOR_VERSION} REQUIRED COMPONENTS Qml)
    target_link_libraries(Plasma::Weather::Data INTERFACE Qt::Qml)
    set(PlasmaWeather_Data_FOUND TRUE)
    if(NOT PlasmaWeather_FIND_QUIETLY)
        message(STATUS "Found PlasmaWeather Data: ${PlasmaWeather_Data_LIBRARY} ${_PLASMA_WEATHER_Data_VERSION_CHECK_RESULT_MESSAGE}")
    endif()
elseif(PlasmaWeather_FIND_REQUIRED_Data)
    message(FATAL_ERROR "PlasmaWeather Data was not found: ${_PLASMA_WEATHER_Data_VERSION_CHECK_RESULT_MESSAGE}")
elseif(Data IN_LIST PlasmaWeather_FIND_COMPONENTS)
    message(WARNING "PlasmaWeather Data was not found: ${_PLASMA_WEATHER_Data_VERSION_CHECK_RESULT_MESSAGE}")
endif()
mark_as_advanced(_PLASMA_WEATHER_Data_LIBRARY
                 _PLASMA_WEATHER_Data_VERSION_CHECK_RESULT
                 _PLASMA_WEATHER_Data_VERSION_CHECK_RESULT_MESSAGE)

if(Ion IN_LIST PlasmaWeather_FIND_COMPONENTS)
    find_path(PlasmaWeather_Ion_INCLUDE_DIR
              "plasma/weather/ion.h"
              DOC "Include directory path for Plasma::Weather::Ion")
    find_library(PlasmaWeather_Ion_LIBRARY
                "plasmaweatherion"
                DOC "Library file path for Plasma::Weather::Ion")
    if(PlasmaWeather_Ion_INCLUDE_DIR AND PlasmaWeather_Ion_LIBRARY)
        set(PlasmaWeather_Ion_INCLUDE_DIR "${PlasmaWeather_Ion_INCLUDE_DIR}/plasma/weather")
        file(REAL_PATH "${PlasmaWeather_Ion_LIBRARY}" _PLASMA_WEATHER_Ion_LIBRARY)
        string(REGEX MATCH "[0-9]+\\.[0-9]+\\.[0-9]+$" PlasmaWeather_Ion_VERSION "${_PLASMA_WEATHER_Ion_LIBRARY}")
        find_package_check_version("${PlasmaWeather_Ion_VERSION}" _PLASMA_WEATHER_Ion_VERSION_CHECK_RESULT
                                   HANDLE_VERSION_RANGE
                                   RESULT_MESSAGE_VARIABLE _PLASMA_WEATHER_Ion_VERSION_CHECK_RESULT_MESSAGE)
        if(_PLASMA_WEATHER_Ion_VERSION_CHECK_RESULT)
            add_library(Plasma::Weather::Ion SHARED IMPORTED GLOBAL)
            set_target_properties(Plasma::Weather::Ion
                                  PROPERTIES IMPORTED_LOCATION "${PlasmaWeather_Ion_LIBRARY}"
                                             INTERFACE_INCLUDE_DIRECTORIES "${PlasmaWeather_Ion_INCLUDE_DIR}")
        endif()
    endif()
endif()
if(TARGET Plasma::Weather::Ion)
    set(PlasmaWeather_VERSION "${PlasmaWeather_Ion_VERSION}")
    target_link_libraries(Plasma::Weather::Ion INTERFACE Plasma::Weather::Data)            
    set(PlasmaWeather_Ion_FOUND TRUE)
    if(NOT PlasmaWeather_FIND_QUIETLY)
        message(STATUS "Found PlasmaWeather Ion: ${PlasmaWeather_Ion_LIBRARY} ${_PLASMA_WEATHER_Ion_VERSION_CHECK_RESULT_MESSAGE}")
    endif()
elseif(PlasmaWeather_FIND_REQUIRED_Ion)
    message(FATAL_ERROR "PlasmaWeather Ion was not found: ${_PLASMA_WEATHER_Ion_VERSION_CHECK_RESULT_MESSAGE}")
elseif(Ion IN_LIST PlasmaWeather_FIND_COMPONENTS)
    message(WARNING "PlasmaWeather Ion was not found: ${_PLASMA_WEATHER_Ion_VERSION_CHECK_RESULT_MESSAGE}")
endif()
mark_as_advanced(_PLASMA_WEATHER_Ion_LIBRARY
                 _PLASMA_WEATHER_Ion_VERSION_CHECK_RESULT
                 _PLASMA_WEATHER_Ion_VERSION_CHECK_RESULT_MESSAGE)

if(IonLegacy IN_LIST PlasmaWeather_FIND_COMPONENTS)
    find_path(PlasmaWeather_IonLegacy_INCLUDE_DIR
              "plasma5support/weather/ion.h"
              DOC "Include directory path for Plasma::Weather::IonLegacy")
    find_library(PlasmaWeather_IonLegacy_LIBRARY
                 "weather_ion"
                 DOC "Library file path for Plasma::Weather::IonLegacy")
    if(PlasmaWeather_IonLegacy_INCLUDE_DIR AND PlasmaWeather_IonLegacy_LIBRARY)
        set(PlasmaWeather_IonLegacy_INCLUDE_DIR "${PlasmaWeather_IonLegacy_INCLUDE_DIR}/plasma5support/weather")
        file(REAL_PATH "${PlasmaWeather_IonLegacy_LIBRARY}" _PLASMA_WEATHER_IonLegacy_LIBRARY)
        string(REGEX MATCH "[0-9]+\\.[0-9]+\\.[0-9]+$" PlasmaWeather_IonLegacy_VERSION "${_PLASMA_WEATHER_IonLegacy_LIBRARY}")
        find_package(Plasma5Support REQUIRED) # IonLegacy's soname is not equals to its version
        find_package_check_version("${Plasma5Support_VERSION}" _PLASMA_WEATHER_IonLegacy_VERSION_CHECK_RESULT
                                   HANDLE_VERSION_RANGE
                                   RESULT_MESSAGE_VARIABLE _PLASMA_WEATHER_IonLegacy_VERSION_CHECK_RESULT_MESSAGE)
        if(_PLASMA_WEATHER_IonLegacy_VERSION_CHECK_RESULT)
            add_library(Plasma::Weather::IonLegacy SHARED IMPORTED GLOBAL)
            set_target_properties(Plasma::Weather::IonLegacy
                                  PROPERTIES IMPORTED_LOCATION "${PlasmaWeather_IonLegacy_LIBRARY}"
                                             INTERFACE_INCLUDE_DIRECTORIES "${PlasmaWeather_IonLegacy_INCLUDE_DIR}")
        endif()
    endif()
endif()
if(TARGET Plasma::Weather::IonLegacy)
    if(NOT PlasmaWeather_VERSION)
        set(PlasmaWeather_VERSION "${Plasma5Support_VERSION}")
    endif()
    target_link_libraries(Plasma::Weather::IonLegacy INTERFACE Plasma::Plasma5Support)
    set(PlasmaWeather_IonLegacy_FOUND TRUE)
    if(NOT PlasmaWeather_FIND_QUIETLY)
        message(STATUS "Found PlasmaWeather Legacy Ion: ${PlasmaWeather_IonLegacy_LIBRARY} ${_PLASMA_WEATHER_IonLegacy_VERSION_CHECK_RESULT_MESSAGE}")
    endif()
elseif(PlasmaWeather_FIND_REQUIRED_IonLegacy)
    message(FATAL_ERROR "PlasmaWeather IonLegacy was not found: ${_PLASMA_WEATHER_IonLegacy_VERSION_CHECK_RESULT_MESSAGE}")
elseif(IonLegacy IN_LIST PlasmaWeather_FIND_COMPONENTS)
    message(WARNING "PlasmaWeather IonLegacy was not found: ${_PLASMA_WEATHER_IonLegacy_VERSION_CHECK_RESULT_MESSAGE}")
endif()
mark_as_advanced(_PLASMA_WEATHER_IonLegacy_LIBRARY
                 _PLASMA_WEATHER_IonLegacy_VERSION_CHECK_RESULT
                 _PLASMA_WEATHER_IonLegacy_VERSION_CHECK_RESULT_MESSAGE)

find_package_handle_standard_args(PlasmaWeather
                                  VERSION_VAR PlasmaWeather_VERSION
                                  HANDLE_VERSION_RANGE
                                  HANDLE_COMPONENTS)

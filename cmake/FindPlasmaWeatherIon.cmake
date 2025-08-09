# Copied from https://zren.github.io/2019/02/12/building-envcan-weather-ion-by-itself and ported to KDE 6 only.

# - Try to find the Plasma Weather Ion library
# Once done this will define
#
#  PlasmaWeatherIon_FOUND           - system has Plasma Weather Ion
#  PlasmaWeatherIon_INCLUDE_DIRS    - the Plasma Weather Ion include directory
#  PlasmaWeatherIon_LIBRARIES       - Plasma Weather Ion library
#  PlasmaWeatherIon_LEGACY          - If Plasma Weather Ion is DataEngine version
#  Plasma::WeatherIon               - A target which can be linked to.

if (PlasmaWeatherIon_INCLUDE_DIR AND PlasmaWeatherIon_LIBRARY)
    # Already in cache, be silent
    set(PlasmaWeatherIon_FIND_QUIETLY TRUE)
endif (PlasmaWeatherIon_INCLUDE_DIR AND PlasmaWeatherIon_LIBRARY)


set(_ION_H_NEW weatherion_export.h)
find_path(_INCLUDE_DIR_NEW "plasma/weatherion/${_ION_H_NEW}")
find_library(_LIBRARY_NEW weatherion)

set(_ION_H_OLD ion.h)
find_path(_INCLUDE_DIR_OLD "plasma5support/weather/${_ION_H_OLD}")
find_library(_LIBRARY_OLD weather_ion)

if (_INCLUDE_DIR_NEW AND _LIBRARY_NEW)
    message(DEBUG "Found library in kdeplasma-addons.")
    set(PlasmaWeatherIon_FOUND TRUE)
    set(PlasmaWeatherIon_INCLUDE_DIRS "${_INCLUDE_DIR_NEW}/plasma/weatherion")
    set(PlasmaWeatherIon_LIBRARIES "${_LIBRARY_NEW}")
    set(PlasmaWeatherIon_LEGACY FALSE)

    add_library(Plasma::WeatherIon SHARED IMPORTED GLOBAL)
    set_target_properties(Plasma::WeatherIon
                          PROPERTIES IMPORTED_LOCATION "${_LIBRARY_NEW}"
                                     INTERFACE_INCLUDE_DIRECTORIES ${PlasmaWeatherIon_INCLUDE_DIRS})
elseif (_INCLUDE_DIR_OLD AND _LIBRARY_OLD)
    message(DEBUG "Found library in plasma-workspace.")
    set(PlasmaWeatherIon_FOUND TRUE)
    set(PlasmaWeatherIon_INCLUDE_DIRS "${_INCLUDE_DIR_OLD}/plasma5support/weather")
    set(PlasmaWeatherIon_LIBRARIES "${_LIBRARY_OLD}")
    set(PlasmaWeatherIon_LEGACY TRUE)

    add_library(Plasma::WeatherIon SHARED IMPORTED GLOBAL)
    set_target_properties(Plasma::WeatherIon
                          PROPERTIES IMPORTED_LOCATION "${_LIBRARY_OLD}"
                                     INTERFACE_INCLUDE_DIRECTORIES ${PlasmaWeatherIon_INCLUDE_DIRS})
    find_package(Plasma5Support REQUIRED)
    target_link_libraries(Plasma::WeatherIon INTERFACE Plasma::Plasma5Support)
endif (_INCLUDE_DIR_NEW AND _LIBRARY_NEW)


if (PlasmaWeatherIon_FOUND)
    if (NOT PlasmaWeatherIon_FIND_QUIETLY)
        message(STATUS "Found Plasma Weather Ion library: ${PlasmaWeatherIon_LIBRARIES}")
    endif (NOT PlasmaWeatherIon_FIND_QUIETLY)
else (PlasmaWeatherIon_FOUND)
    if (PlasmaWeatherIon_FIND_REQUIRED)
        message(FATAL_ERROR "Plasma Weather Ion library was not found")
    endif(PlasmaWeatherIon_FIND_REQUIRED)
endif (PlasmaWeatherIon_FOUND)

mark_as_advanced(_ION_H_OLD _ION_H_NEW _INCLUDE_DIR_OLD _LIBRARY_OLD _INCLUDE_DIR_NEW _LIBRARY_NEW)

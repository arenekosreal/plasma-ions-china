if (PlasmaWeatherData_INCLUDE_DIR AND PlasmaWeatherData_LIBRARY)
    set(PlasmaWeatherData_QUIETLY TRUE)
endif(PlasmaWeatherData_INCLUDE_DIR AND PlasmaWeatherData_LIBRARY)

find_path(_INCLUDE_DIR "plasma/weatherdata/plasmaweatherdata_export.h"
          PATHS "${CMAKE_CURRENT_SOURCE_DIR}/include")
find_library(_LIBRARY plasmaweatherdata)

if (_INCLUDE_DIR AND _LIBRARY)
    set(PlasmaWeatherData_FOUND TRUE)
    set(PlasmaWeatherData_INCLUDE_DIRS "${_INCLUDE_DIR}/plasma/weatherdata")
    set(PlasmaWeatherData_LIBRARIES "${_LIBRARY}")

    find_package(Qt${QT_MAJOR_VERSION} COMPONENTS Qml REQUIRED)
    add_library(Plasma::WeatherData SHARED IMPORTED GLOBAL)
    set_target_properties(Plasma::WeatherData
                          PROPERTIES IMPORTED_LOCATION "${_LIBRARY}"
                                     INTERFACE_INCLUDE_DIRECTORIES "${PlasmaWeatherData_INCLUDE_DIRS}")
    target_link_libraries(Plasma::WeatherData INTERFACE Qt::Qml)
endif(_INCLUDE_DIR AND _LIBRARY)

if (PlasmaWeatherData_FOUND)
    if (NOT PlasmaWeatherData_FIND_QUIETLY)
        message(STATUS "Found Plasma Weather Data: ${PlasmaWeatherData_LIBRARIES}")
    endif(NOT PlasmaWeatherData_FIND_QUIETLY)
else (PlasmaWeatherData_FOUND)
    if (PlasmaWeatherData_FIND_REQUIRED)
        message(FATAL_ERROR "Plasma Weather Data was not found.")
    endif(PlasmaWeatherData_FIND_REQUIRED)
endif(PlasmaWeatherData_FOUND)

mark_as_advanced(_INCLUDE_DIR _LIBRARY)

function(plasma_ions_china_setup_ion_headers)
    set(options)
    set(oneValueArgs VERSION HASH TARGET_NAME)
    set(multiValueArgs VARIABLES)
    cmake_parse_arguments(PARSE_ARGV 0 PLASMA_IONS_CHINA
                          "${options}"
                          "${oneValueArgs}"
                          "${multiValueArgs}")
    # IMPORTED target needs its include directory exists when configuring
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/include/plasma/weather")
    
    include(ExternalProject)
    ExternalProject_Add("${PLASMA_IONS_CHINA_TARGET_NAME}"
                        URL https://download.kde.org/stable/plasma/${PLASMA_IONS_CHINA_VERSION}/kdeplasma-addons-${PLASMA_IONS_CHINA_VERSION}.tar.xz
                        URL_HASH "${PLASMA_IONS_CHINA_HASH}"
                        BUILD_COMMAND ""
                        INSTALL_COMMAND ""
                        TEST_COMMAND ""
                        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/include/plasma/weather"
                        COMMAND ${CMAKE_COMMAND} -E echo "Copying Ion headers..."
                        COMMAND ${CMAKE_COMMAND} -E copy "<BINARY_DIR>/applets/weather/ions/plasmaweatherion_export.h"
                                                         "<BINARY_DIR>/applets/weather/weatherdata/plasmaweatherdata_export.h"
                                                         "<SOURCE_DIR>/applets/weather/ions/ion.h"
                                                         "<SOURCE_DIR>/applets/weather/weatherdata/currentday.h"
                                                         "<SOURCE_DIR>/applets/weather/weatherdata/forecast.h"
                                                         "<SOURCE_DIR>/applets/weather/weatherdata/futuredays.h"
                                                         "<SOURCE_DIR>/applets/weather/weatherdata/lastday.h"
                                                         "<SOURCE_DIR>/applets/weather/weatherdata/lastobservation.h"
                                                         "<SOURCE_DIR>/applets/weather/weatherdata/locations.h"
                                                         "<SOURCE_DIR>/applets/weather/weatherdata/metadata.h"
                                                         "<SOURCE_DIR>/applets/weather/weatherdata/station.h"
                                                         "<SOURCE_DIR>/applets/weather/weatherdata/warnings.h"
                                                         "${CMAKE_CURRENT_BINARY_DIR}/include/plasma/weather"
                        COMMAND ${CMAKE_COMMAND} -E echo "Ion headers are ready at ${CMAKE_CURRENT_BINARY_DIR}/include/plasma/weather.")
    foreach(PLASMA_IONS_CHINA_VARIABLE IN LISTS PLASMA_IONS_CHINA_VARIABLES)
        message(DEBUG "Setting ${PLASMA_IONS_CHINA_VARIABLE} to ${CMAKE_CURRENT_BINARY_DIR}/include...")
        set("${PLASMA_IONS_CHINA_VARIABLE}" "${CMAKE_CURRENT_BINARY_DIR}/include" PARENT_SCOPE)
    endforeach()
endfunction()

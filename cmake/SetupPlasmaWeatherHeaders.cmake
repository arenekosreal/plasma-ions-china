include(ExternalProject)
ExternalProject_Add(ion_headers
                   URL https://download.kde.org/stable/plasma/${HEADER_VERSION}/kdeplasma-addons-${HEADER_VERSION}.tar.xz
                   URL_HASH SHA256=${HEADER_SOURCE_TARBALL_SHA256}
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

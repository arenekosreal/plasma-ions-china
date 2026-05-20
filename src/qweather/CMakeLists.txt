set(CACHE{PLASMA_IONS_CHINA_QWEATHER_PRIVATE_KEY} TYPE FILEPATH
    HELP "The path to the private key to sign qweather jwt token."
    VALUE "")
set(CACHE{PLASMA_IONS_CHINA_QWEATHER_KID} TYPE STRING
    HELP "The kid of qweather jwt token header."
    VALUE "")
set(CACHE{PLASMA_IONS_CHINA_QWEATHER_SUB} TYPE STRING
    HELP "The sub of qweather jwt token payload."
    VALUE "")
set(CACHE{PLASMA_IONS_CHINA_QWEATHER_API_HOST} TYPE STRING
    HELP "The api host of qweather api request."
    VALUE "")
if (NOT IS_READABLE "${PLASMA_IONS_CHINA_QWEATHER_PRIVATE_KEY}")
    message(WARNING
          "qweather: Private key is not readable, skip adding QWeather ion.")
    return()
endif()
if (NOT PLASMA_IONS_CHINA_QWEATHER_KID)
    message(WARNING "qweather: KID is empty, skip adding QWeather ion.")
    return()
endif()
if (NOT PLASMA_IONS_CHINA_QWEATHER_SUB)
    message(WARNING "qweather: SUB is empty, skip adding QWeather ion.")
    return()
endif()
if (NOT PLASMA_IONS_CHINA_QWEATHER_API_HOST)
    message(WARNING "qweather: API_HOST is empty, skip adding QWeather ion.")
    return()
endif()

kcoreaddons_add_plugin(qweather INSTALL_NAMESPACE "${ION_INSTALL_NAMESPACE}")
target_sources(qweather PRIVATE ed25519.hpp ed25519.cpp
                                qweather.hpp qweather.cpp)
ecm_qt_declare_logging_category(qweather
                                HEADER qweather_debug.hpp
                                IDENTIFIER WEATHER::ION::QWEATHER
                                CATEGORY_NAME org.kde.weather.ion.qweather
                                DEFAULT_SEVERITY Info
                                DESCRIPTION "www.qweather.com ion plugin"
                                EXPORT PLASMAWORKSPACE)
ecm_qt_declare_logging_category(qweather
                                HEADER ed25519_debug.hpp
                                IDENTIFIER WEATHER::HELPER::ED25519
                                CATEGORY_NAME de.weather.ion.qweather.ed25519
                                DEFAULT_SEVERITY Info
                                DESCRIPTION "ed25519 module for QWeather"
                                EXPORT PLASMAWORKSPACE)
find_package(Qt${QT_MAJOR_VERSION} ${QT_MIN_VERSION} REQUIRED
             COMPONENTS Core Network)
cmake_pkg_config(IMPORT libsodium >=1.0.0 REQUIRED)
target_link_libraries(qweather PRIVATE
                      Plasma::Weather::Ion
                      PkgConfig::libsodium
                      Qt::Core
                      Qt::Network
                      KF${KF_MAJOR_VERSION}::UnitConversion
                      KF${KF_MAJOR_VERSION}::I18n
                      KF${KF_MAJOR_VERSION}::CoreAddons)
set(_PRIVATE_KEY_ALIAS "qweather/privatekey.pem")
set_source_files_properties("${PLASMA_IONS_CHINA_QWEATHER_PRIVATE_KEY}"
                            PROPERTIES
                              QT_RESOURCE_ALIAS "${_PRIVATE_KEY_ALIAS}")
qt_add_resources(qweather "api-keys"
                 FILES "${PLASMA_IONS_CHINA_QWEATHER_PRIVATE_KEY}")
target_compile_definitions(qweather
                           PRIVATE
                            KID=\"${PLASMA_IONS_CHINA_QWEATHER_KID}\"
                            SUB=\"${PLASMA_IONS_CHINA_QWEATHER_SUB}\"
                            KEY_FILE=\":/${_PRIVATE_KEY_ALIAS}\"
                            API_HOST=\"${PLASMA_IONS_CHINA_QWEATHER_API_HOST}\")

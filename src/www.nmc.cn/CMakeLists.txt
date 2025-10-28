set(NMCCN_ION_NAME nmccn)
if(PlasmaWeatherIon_LEGACY)
    message(WARNING "No maintainer is using KDE < 6.5 so the legacy version is unmaintained now.")
    set(NMCCN_TARGET_NAME plasma_engine_${NMCCN_ION_NAME})
    set(LOGGING_CATEGORY_NAME kde.dataengine.ion.nmccn)
    set(LOGGING_CATEGORY_DESCRIPTION "DataEngine ion nmccn")
    add_library(${NMCCN_TARGET_NAME} MODULE nmccn.hpp nmccn.cpp)
    install(TARGETS ${NMCCN_TARGET_NAME} DESTINATION "${PLASMA5SUPPORT_DATAENGINES_PLUGINDIR}")
    add_compile_definitions(-DION_NAME=\"${NMCCN_ION_NAME}\")
else()
    set(NMCCN_TARGET_NAME ${NMCCN_ION_NAME})
    set(LOGGING_CATEGORY_NAME org.kde.weather.ion.nmccn)
    set(LOGGING_CATEGORY_DESCRIPTION "www.nmc.com ion plugin")
    kcoreaddons_add_plugin(${NMCCN_TARGET_NAME} INSTALL_NAMESPACE "${ION_INSTALL_NAMESPACE}")
    target_sources(${NMCCN_TARGET_NAME} PRIVATE nmccn.hpp nmccn.cpp)
endif()

ecm_qt_declare_logging_category(${NMCCN_TARGET_NAME}
                                HEADER nmccn_debug.hpp
                                IDENTIFIER IONENGINE_NMCCN
                                CATEGORY_NAME ${LOGGING_CATEGORY_NAME}
                                DEFAULT_SEVERITY Info
                                DESCRIPTION "${LOGGING_CATEGORY_DESCRIPTION}"
                                EXPORT PLASMAWORKSPACE)

find_package(Qt${QT_MAJOR_VERSION} ${QT_MIN_VERSION} REQUIRED COMPONENTS Core Network)
target_link_libraries(${NMCCN_TARGET_NAME}
                      PRIVATE Plasma::WeatherIon
                              Qt::Core
                              Qt::Network
                              KF${QT_MAJOR_VERSION}::UnitConversion
                              KF${QT_MAJOR_VERSION}::I18n
                              KF${QT_MAJOR_VERSION}::CoreAddons)

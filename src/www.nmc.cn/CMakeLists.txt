set(ION_NAME nmccn)
if(PlasmaWeatherIon_LEGACY)
    set(TARGET_NAME plasma_engine_${ION_NAME})
    add_library(${TARGET_NAME} MODULE nmccn.hpp nmccn.cpp)
    install(TARGETS ${TARGET_NAME} DESTINATION "${PLASMA5SUPPORT_DATAENGINES_PLUGINDIR}")
    add_compile_definitions(-DION_NAME=\"${ION_NAME}\")
else()
    set(TARGET_NAME ${ION_NAME})
    kcoreaddons_add_plugin(${TARGET_NAME} INSTALL_NAMESPACE "${ION_INSTALL_NAMESPACE}")
    target_sources(${TARGET_NAME} PRIVATE nmccn.hpp nmccn.cpp)
endif()

ecm_qt_declare_logging_category(${TARGET_NAME}
                                HEADER nmccn_debug.hpp
                                IDENTIFIER IONENGINE_NMCCN
                                CATEGORY_NAME kde.dataengine.ion.nmccn
                                DEFAULT_SEVERITY Info
                                DESCRIPTION "DataEngine ion nmccn"
                                EXPORT PLASMAWORKSPACE)

find_package(Qt${QT_MAJOR_VERSION} REQUIRED COMPONENTS Core Network)
target_link_libraries(${TARGET_NAME}
                      PRIVATE Plasma::WeatherIon
                              Qt::Core
                              Qt::Network
                              KF${QT_MAJOR_VERSION}::UnitConversion
                              KF${QT_MAJOR_VERSION}::I18n)                          

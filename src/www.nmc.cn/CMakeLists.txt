set(ION_NAME www.nmc.cn)
if(PlasmaWeatherIon_LEGACY)
    set(TARGET_NAME plasma_engine_wwwnmccn)
    add_library(${TARGET_NAME} MODULE wwwnmccn.hpp wwwnmccn.cpp)
    install(TARGETS ${TARGET_NAME} DESTINATION "${PLASMA5SUPPORT_DATAENGINES_PLUGINDIR}")
    add_compile_definitions(-DION_NAME=\"${ION_NAME}\")
else()
    set(TARGET_NAME wwwnmccn)
    kcoreaddons_add_plugin(${TARGET_NAME} INSTALL_NAMESPACE "${ION_INSTALL_NAMESPACE}")
    target_sources(${TARGET_NAME} PRIVATE wwwnmccn.hpp wwwnmccn.cpp)
endif()

ecm_qt_declare_logging_category(${TARGET_NAME}
                                HEADER wwwnmccn_debug.hpp
                                IDENTIFIER IONENGINE_WWWNMCCN
                                CATEGORY_NAME kde.dataengine.ion.wwwnmccn
                                DEFAULT_SEVERITY Info
                                DESCRIPTION "DataEngine ion wwwnmccn"
                                EXPORT PLASMAWORKSPACE)

find_package(LibXml2 REQUIRED)
find_package(Qt${QT_MAJOR_VERSION} REQUIRED COMPONENTS Core Network)
target_link_libraries(${TARGET_NAME}
                      PRIVATE Plasma::WeatherIon
                              LibXml2::LibXml2
                              Qt::Core
                              Qt::Network
                              KF${QT_MAJOR_VERSION}::UnitConversion
                              KF${QT_MAJOR_VERSION}::I18n)                          

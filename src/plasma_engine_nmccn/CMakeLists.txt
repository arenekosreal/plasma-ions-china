add_library(plasma_engine_nmccn MODULE nmccn.hpp nmccn.cpp)
install(TARGETS plasma_engine_nmccn DESTINATION "${PLASMA5SUPPORT_DATAENGINES_PLUGINDIR}")
ecm_qt_declare_logging_category(plasma_engine_nmccn
                                HEADER nmccn_debug.hpp
                                IDENTIFIER IONENGINE_NMCCN
                                CATEGORY_NAME kde.dataengine.ion.nmccn
                                DEFAULT_SEVERITY Info
                                DESCRIPTION "DataEngine ion nmccn"
                                EXPORT PLASMAWORKSPACE)
find_package(Qt${QT_MAJOR_VERSION} ${QT_MIN_VERSION} REQUIRED COMPONENTS Core Network)
target_link_libraries(plasma_engine_nmccn
                      PRIVATE Plasma::Weather::IonLegacy
                              Qt::Core
                              Qt::Network
                              KF${KF_MAJOR_VERSION}::UnitConversion
                              KF${KF_MAJOR_VERSION}::I18n
                              KF${KF_MAJOR_VERSION}::CoreAddons)

kcoreaddons_add_plugin(nmccn INSTALL_NAMESPACE "${ION_INSTALL_NAMESPACE}")
target_sources(nmccn PRIVATE nmccn.hpp nmccn.cpp)
ecm_qt_declare_logging_category(nmccn
                                HEADER nmccn_debug.hpp
                                IDENTIFIER WEATHER::ION::NMCCN
                                CATEGORY_NAME org.kde.weather.ion.nmccn
                                DEFAULT_SEVERITY Info
                                DESCRIPTION "www.nmc.cn ion plugin"
                                EXPORT PLASMAWORKSPACE)
find_package(Qt${QT_MAJOR_VERSION} ${QT_MIN_VERSION} REQUIRED COMPONENTS Core Network)
target_link_libraries(nmccn PRIVATE
                      Plasma::Weather::Ion
                      Qt::Core
                      Qt::Network
                      KF${KF_MAJOR_VERSION}::UnitConversion
                      KF${KF_MAJOR_VERSION}::I18n
                      KF${KF_MAJOR_VERSION}::CoreAddons)

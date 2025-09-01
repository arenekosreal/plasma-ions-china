qt_add_executable(test_nmccn test_nmccn.hpp test_nmccn.cpp)
target_include_directories(test_nmccn PRIVATE "../../src/www.nmc.cn")
target_link_libraries(test_nmccn PRIVATE Plasma::WeatherIon
                                         Qt::Core
                                         Qt::Network
                                         Qt::Test
                                         KF${QT_MAJOR_VERSION}::UnitConversion
                                         KF${QT_MAJOR_VERSION}::I18n)
add_test(NAME test_nmccn COMMAND test_nmccn)

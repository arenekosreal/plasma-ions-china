cmake_minimum_required(VERSION 4.0)

project(plasma-ions-china
        DESCRIPTION "A collection of ions for Chinese users."
        HOMEPAGE_URL "https://github.com/arenekosreal/plasma-ions-china"
        LANGUAGES CXX)

if(NOT PROJECT_VERSION)
    find_package(Git)
    if (Git_FOUND)
        execute_process(COMMAND "${GIT_EXECUTABLE}" describe --tags --always
                        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
                        OUTPUT_VARIABLE GIT_TAG
                        OUTPUT_STRIP_TRAILING_WHITESPACE
                        RESULT_VARIABLE GIT_RESULT
                        ERROR_QUIET)
        if (GIT_RESULT EQUAL 0 AND GIT_TAG MATCHES "^v[0-9]+\\.[0-9]+\\.[0-9]+")
            string(REGEX REPLACE "^v([0-9]+\\.[0-9]+\\.[0-9]+)" "\\1" PROJECT_VERSION "${GIT_TAG}")
            message("Extracted project version ${PROJECT_VERSION} from git tag.")
        endif()
    endif()
    if(NOT PROJECT_VERSION)
        set(PROJECT_VERSION 0.1.0)
        message(WARNING "Failed to find git to get tag of repository. Using fallback value ${PROJECT_VERSION} instead.")
    endif()
endif()

set(QT_MAJOR_VERSION 6)
set(KF_MAJOR_VERSION ${QT_MAJOR_VERSION})
set(QT_MIN_VERSION ${QT_MAJOR_VERSION}.6.0)
set(KF_MIN_VERSION ${KF_MAJOR_VERSION}.0.0)
find_package(ECM ${KF_MIN_VERSION} REQUIRED NO_MODULE)
list(APPEND CMAKE_MODULE_PATH "${ECM_MODULE_PATH}" "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(ECMQtDeclareLoggingCategory)
include(KDEInstallDirs)
include(KDECompilerSettings)
include(KDECMakeSettings)
include(KDEClangFormat)
include(KDEGitCommitHooks)

set(CMAKE_FIND_PACKAGE_PREFER_CONFIG TRUE) # Config, then Module
find_package(PlasmaWeather REQUIRED)
find_package(KF${QT_MAJOR_VERSION} ${KF_MIN_VERSION} REQUIRED COMPONENTS UnitConversion I18n)
set(TRANSLATION_DOMAIN "plasma_ions_china")
add_compile_definitions(-DTRANSLATION_DOMAIN=\"${TRANSLATION_DOMAIN}\")
if(PlasmaWeather_Ion_LEGACY)
    add_compile_definitions(-DION_LEGACY)
    add_compile_definitions(-DKDE_WEATHER_TRANSLATION_DOMAIN=\"plasma_engine_weather\")
else()
    find_package(KF${KF_MAJOR_VERSION} ${KF_MIN_VERSION} REQUIRED COMPONENTS CoreAddons)
    set(ION_INSTALL_NAMESPACE "plasma/weather_ions")
    add_compile_definitions(-DKDE_WEATHER_TRANSLATION_DOMAIN=\"plasma_applet_org.kde.plasma.weather\")
endif()
ki18n_install(po)

add_subdirectory("src")

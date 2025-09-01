cmake_minimum_required(VERSION 4.0)

project(plasma-ions-china
        DESCRIPTION "A collection of ions for Chinese users."
        HOMEPAGE_URL "https://github.com/arenekosreal/plasma-ions-china"
        LANGUAGES CXX)

if(NOT "${PROJECT_VERSION}")
    set(PROJECT_VERSION_FALLBACK 0.1.0)
    find_package(Git)
    if (${Git_FOUND})
        execute_process(COMMAND "${GIT_EXECUTABLE}" describe --tags --always
                        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
                        OUTPUT_VARIABLE GIT_TAG
                        OUTPUT_STRIP_TRAILING_WHITESPACE
                        RESULT_VARIABLE GIT_RESULT
                        ERROR_QUIET)
        if (${GIT_RESULT} EQUAL 0 AND ${GIT_TAG} MATCHES "^v[0-9]+\.[0-9]+\.[0-9]+")
            string(REGEX REPLACE "^v([0-9]+\.[0-9]+\.[0-9]+)" "\1" PROJECT_VERSION "${GIT_TAG}")
            message("Extracted project version ${PROJECT_VERSION} from git tag.")
        else()
            set(PROJECT_VERSION "${PROJECT_VERSION_FALLBACK}")
            message(WARNING "Failed to execute git to get tag of repository. Using fallback value ${PROJECT_VERSION} instead.")
        endif()
    else()
        set(PROJECT_VERSION "${PROJECT_VERSION_FALLBACK}")
        message(WARNING "Failed to find git to get tag of repository. Using fallback value ${PROJECT_VERSION} instead.")
    endif()
endif()

set(QT_MAJOR_VERSION 6)
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})
find_package(ECM REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH} ${CMAKE_MODULE_PATH})
if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    include(CTest)
endif()
include(ECMQtDeclareLoggingCategory)
include(KDEInstallDirs)
include(KDECompilerSettings)
include(KDECMakeSettings)
include(KDEClangFormat)
include(KDEGitCommitHooks)

find_package(Gettext REQUIRED)
find_package(PlasmaWeatherIon REQUIRED)
find_package(KF${QT_MAJOR_VERSION} REQUIRED COMPONENTS UnitConversion I18n)
set(TRANSLATION_DOMAIN "plasma_ions_china")
add_compile_definitions(-DTRANSLATION_DOMAIN=\"${TRANSLATION_DOMAIN}\")
if(PlasmaWeatherIon_LEGACY)
    add_compile_definitions(-DION_LEGACY)
    add_compile_definitions(-DKDE_WEATHER_TRANSLATION_DOMAIN=\"plasma_engine_weather\")
else()
    find_package(KF${QT_MAJOR_VERSION} REQUIRED COMPONENTS CoreAddons)
    set(ION_INSTALL_NAMESPACE "plasma/weather_ions")
    add_compile_definitions(-DKDE_WEATHER_TRANSLATION_DOMAIN=\"plasma_applet_org.kde.plasma.weather\")
endif()
set(LANGUAGES zh_CN)
foreach (_LANGUAGE IN LISTS LANGUAGES)
    file(COPY_FILE "${CMAKE_CURRENT_SOURCE_DIR}/po/${_LANGUAGE}/${TRANSLATION_DOMAIN}.po"
                   "${CMAKE_CURRENT_BINARY_DIR}/${_LANGUAGE}.po")
endforeach()
gettext_process_pot_file("${CMAKE_CURRENT_SOURCE_DIR}/po/${TRANSLATION_DOMAIN}.pot"
                         ALL
                         INSTALL_DESTINATION "${KDE_INSTALL_LOCALEDIR}"
                         LANGUAGES ${LANGUAGES})

add_subdirectory("src")
if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME AND BUILD_TESTING)
    add_subdirectory("tests")
endif()

function(plasma_ions_china_setup_po_targets)
    set(options)
    set(oneValueArgs TRANSLATION_DOMAIN)
    set(multiValueArgs)
    cmake_parse_arguments(PARSE_ARGV 0 PLASMA_IONS_CHINA
                          "${options}" "${oneValueArgs}" "${multiValueArgs}")
    find_package(Gettext)
    if(NOT GETTEXT_XGETTEXT_EXECUTABLE)
        find_program(GETTEXT_XGETTEXT_EXECUTABLE xgettext)
    endif()
    if(GETTEXT_XGETTEXT_EXECUTABLE)
        file(GLOB_RECURSE CPP_FILES RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/src/**/*.cpp")
        add_custom_target(update-pot
                          COMMENT "Updated ${CMAKE_CURRENT_SOURCE_DIR}/po/${PLASMA_IONS_CHINA_TRANSLATION_DOMAIN}.pot"
                          DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/po/${PLASMA_IONS_CHINA_TRANSLATION_DOMAIN}.pot")
        add_custom_command(TARGET update-pot
                           PRE_BUILD
                           COMMAND "${GETTEXT_XGETTEXT_EXECUTABLE}" --c++ --from-code=utf-8 --qt
                                   --output="${CMAKE_CURRENT_SOURCE_DIR}/po/${PLASMA_IONS_CHINA_TRANSLATION_DOMAIN}.pot"
                                   --default-domain="${PLASMA_IONS_CHINA_TRANSLATION_DOMAIN}"
                                   --keyword=i18n
                                   --package-name="${PROJECT_NAME}"
                                   --package-version="${PROJECT_VERSION}"
                                   ${CPP_FILES}
                               WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
                               COMMENT "Updating ${CMAKE_CURRENT_SOURCE_DIR}/po/${PLASMA_IONS_CHINA_TRANSLATION_DOMAIN}.pot...")
    else()
        message(WARNING "xgettext is not found, skip adding targets to update pot file.")
    endif()
    if(GETTEXT_MSGMERGE_EXECUTABLE)
        file(GLOB_RECURSE PO_FILES RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/po/**/*.po")
        add_custom_target(update-po
                          COMMENT "Updated existing po files."
                          DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/po/${PLASMA_IONS_CHINA_TRANSLATION_DOMAIN}.pot")
        foreach(PO_FILE IN LISTS PO_FILES)
            add_custom_command(TARGET update-po
                               PRE_BUILD
                               COMMAND "${GETTEXT_MSGMERGE_EXECUTABLE}"
                                       "${CMAKE_CURRENT_SOURCE_DIR}/${PO_FILE}"
                                       "${CMAKE_CURRENT_SOURCE_DIR}/po/${PLASMA_IONS_CHINA_TRANSLATION_DOMAIN}.pot"
                                       --update
                               WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
                               COMMENT "Updating ${CMAKE_CURRENT_SOURCE_DIR}/${PO_FILE}...")
        endforeach()
    else()
        message(WARNING "msgmerge is not found, skip adding target to update po files.")
    endif()
endfunction()

function(beygram_add_test target)
    qt_add_executable(${target} ${target}.cpp)
    target_link_libraries(${target}
        PRIVATE
            beygram_core
            beygram_telegram
            beygram_platform_windows
            beygram_ui
            Qt6::Core
            Qt6::Gui
            Qt6::Test
            beygram_warnings
    )
    add_test(NAME ${target} COMMAND ${target})
    set_tests_properties(${target} PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 60
    )
    beygram_copy_tdlib_runtime(${target})
endfunction()

beygram_add_test(tst_redaction)
beygram_add_test(tst_colorcontrast)
beygram_add_test(tst_app_paths)
beygram_add_test(tst_telegram_config)
beygram_add_test(tst_auth_requests)
beygram_add_test(tst_telegram_types)
beygram_add_test(tst_chat_models)
beygram_add_test(tst_chat_folders)
beygram_add_test(tst_draft_store)
beygram_add_test(tst_message_search)
beygram_add_test(tst_search_model)
beygram_add_test(tst_user_profile)
beygram_add_test(tst_appearance_store)
beygram_add_test(tst_notification_formatter)
beygram_add_test(tst_update_scheduler)
beygram_add_test(tst_text_formatting)
beygram_add_test(tst_reply_info)
beygram_add_test(tst_media_viewer)
beygram_add_test(tst_media_image)
beygram_add_test(tst_telegram_requests)
beygram_add_test(tst_logging)
beygram_add_test(tst_secret_store)
beygram_add_test(tst_theme)
beygram_add_test(tst_qr_image)
beygram_add_test(tst_tdjson_transport)
beygram_add_test(tst_tdlib_locator)
beygram_add_test(tst_account_registry)
beygram_add_test(tst_session_lock)
beygram_add_test(tst_database_key)
beygram_add_test(tst_credential_store)
beygram_add_test(tst_telegram_session)
beygram_add_test(tst_tdlib_integration)
set_tests_properties(tst_tdlib_integration PROPERTIES TIMEOUT 180)
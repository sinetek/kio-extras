add_definitions(-DTRANSLATION_DOMAIN=\"kio5_afc\")

set(kio_afc_SRCS
    kio_afc.cpp
    afcdevice.cpp
    afcresult.cpp
    afcurl.cpp
    afcdiskusage.cpp
    afcfilereader.cpp
)

ecm_qt_declare_logging_category(kio_afc_SRCS
                                HEADER afc-debug.h
                                IDENTIFIER KIO_AFC_LOG
                                CATEGORY_NAME kio_afc)

add_library(kio_afc MODULE ${kio_afc_SRCS})

target_link_libraries(kio_afc
    PUBLIC
    KF5::KIOCore
    IMobileDevice::IMobileDevice
    PList::PList
    PRIVATE
    KF5::I18n
)

set_target_properties(kio_afc PROPERTIES OUTPUT_NAME "afc")
install(TARGETS kio_afc DESTINATION ${KDE_INSTALL_PLUGINDIR}/kf5/kio)

install(FILES solid_afc.desktop DESTINATION ${KDE_INSTALL_DATADIR}/solid/actions)

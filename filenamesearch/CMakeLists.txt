add_library(kio_filenamesearch MODULE kio_filenamesearch.cpp)
target_link_libraries(kio_filenamesearch KF5::KIOCore KF5::DBusAddons Qt5::Network KF5::KIOCore KF5::I18n)
set_target_properties(kio_filenamesearch PROPERTIES OUTPUT_NAME "filenamesearch")

install(TARGETS kio_filenamesearch DESTINATION ${KDE_INSTALL_PLUGINDIR}/kf5/kio)
install(FILES filenamesearch.protocol DESTINATION  ${KDE_INSTALL_KSERVICES5DIR})

add_subdirectory(kded)

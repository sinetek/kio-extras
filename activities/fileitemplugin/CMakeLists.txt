# vim:set softtabstop=3 shiftwidth=3 tabstop=3 expandtab:

set (kactivitymanagerd_fileitem_linking_plugin_SRCS
   FileItemLinkingPlugin.cpp
   FileItemLinkingPluginActionLoader.cpp
   ../utils/qsqlquery_iterator.cpp
   )

add_library (
   kactivitymanagerd_fileitem_linking_plugin MODULE
   ${kactivitymanagerd_fileitem_linking_plugin_SRCS}
   )

target_link_libraries (
   kactivitymanagerd_fileitem_linking_plugin
   Qt5::Core
   Qt5::Sql
   KF5::KIOFileWidgets
   KF5::I18n
   KF5::Activities
   )

install (
   TARGETS kactivitymanagerd_fileitem_linking_plugin
   DESTINATION ${KDE_INSTALL_PLUGINDIR}/kf5/kfileitemaction
   )

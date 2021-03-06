## Check for XDR functions
include(CheckCXXSymbolExists)

set(CMAKE_REQUIRED_LIBRARIES ${CMAKE_REQUIRED_LIBRARIES} ${TIRPC_LIBRARIES})
set(CMAKE_REQUIRED_INCLUDES ${CMAKE_REQUIRED_INCLUDES} ${TIRPC_INCLUDE_DIRS})
check_cxx_symbol_exists(xdr_u_int64_t "rpc/xdr.h" HAVE_XDR_U_INT64_T)
check_cxx_symbol_exists(xdr_uint64_t "rpc/xdr.h" HAVE_XDR_UINT64_T)
check_cxx_symbol_exists(xdr_u_hyper "rpc/xdr.h" HAVE_XDR_U_HYPER)
check_cxx_symbol_exists(xdr_u_longlong_t "rpc/xdr.h" HAVE_XDR_U_LONGLONG_T)

if (NOT HAVE_XDR_U_INT64_T AND NOT HAVE_XDR_UINT64_T AND NOT HAVE_XDR_U_HYPER AND NOT HAVE_XDR_U_LONGLONG_T)
    message(FATAL_ERROR "Could not find 64-bit XDR datatype functions!")
endif (NOT HAVE_XDR_U_INT64_T AND NOT HAVE_XDR_UINT64_T AND NOT HAVE_XDR_U_HYPER AND NOT HAVE_XDR_U_LONGLONG_T)

if (HAVE_XDR_U_INT64_T)
    add_definitions(-DHAVE_XDR_U_INT64_T)
endif (HAVE_XDR_U_INT64_T)
if (HAVE_XDR_UINT64_T)
    add_definitions(-DHAVE_XDR_UINT64_T)
endif (HAVE_XDR_UINT64_T)
if (HAVE_XDR_U_HYPER)
    add_definitions(-DHAVE_XDR_U_HYPER)
endif (HAVE_XDR_U_HYPER)
if (HAVE_XDR_U_LONGLONG_T)
    add_definitions(-DHAVE_XDR_U_LONGLONG_T)
endif (HAVE_XDR_U_LONGLONG_T)

add_definitions(-DTRANSLATION_DOMAIN=\"kio5_nfs\")
include_directories(${TIRPC_INCLUDE_DIRS})

add_library(kio_nfs MODULE kio_nfs.cpp nfsv2.cpp nfsv3.cpp rpc_nfs3_prot_xdr.c rpc_nfs2_prot_xdr.c)
target_link_libraries(kio_nfs KF5::KIOCore KF5::I18n Qt5::Network ${TIRPC_LIBRARIES})
set_target_properties(kio_nfs PROPERTIES OUTPUT_NAME "nfs")

install(TARGETS kio_nfs DESTINATION ${KDE_INSTALL_PLUGINDIR}/kf5/kio)

########### install files ###############

install( FILES nfs.protocol  DESTINATION  ${KDE_INSTALL_KSERVICES5DIR} )

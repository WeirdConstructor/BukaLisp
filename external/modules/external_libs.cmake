# Boost and Poco paths:
if (MSVC)
    include(external/modules/local_paths.cmake)
endif()

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)

#
# Boost Setup
#
set(Boost_USE_STATIC_LIBS ON)
set(Boost_USE_MULTITHREADED ON)
if (WIN32)
    add_definitions(-DBOOST_USE_WINAPI_VERSION=0x0501)
    if (MSVC)
        # disable autolinking in boost and some other warnings
        add_definitions(-DBOOST_ALL_NO_LIB -D_CRT_SECURE_NO_WARNINGS -D_SCL_SECURE_NO_WARNINGS -D_WIN32_WINNT=0x0501)
    endif()
    set(Boost_THREADAPI win32)
else()
    add_definitions(-DBOOST_ALL_NO_LIB)
endif()
if (WIN32)
    find_package(Boost 1.60 COMPONENTS
        system
        filesystem
        locale
        thread
        date_time
        regex
    REQUIRED)
else()
    find_package(Boost 1.50 COMPONENTS
        system
        filesystem
        locale
        thread
        date_time
        regex
    REQUIRED)
endif()

#
# POCO Setup
#
if (WIN32)
    if (!MSVC)
        set(POCO_LIB     /mingw32/lib/)
        set(POCO_INCLUDE /mingw32/include/Poco/)
    endif()

    set(POCO_INCLUDEDIRS
        ${POCO_INCLUDE}/JSON/include
        ${POCO_INCLUDE}/Foundation/include
        ${POCO_INCLUDE}/Util/include
        ${POCO_INCLUDE}/Net/include
        ${POCO_INCLUDE}/NetSSL_OpenSSL/include
        ${POCO_INCLUDE}/Crypto/include
        ${POCO_INCLUDE}/Zip/include
        ${POCO_INCLUDE}/Data/include
        ${POCO_INCLUDE}/Data/ODBC/include
        ${POCO_INCLUDE}/../OpenSSL-Win32/include
    )
else()
#    set(POCO_INCLUDEDIRS
#        ${POCO_INCLUDE}/JSON/include
#        ${POCO_INCLUDE}/Foundation/include
#        ${POCO_INCLUDE}/Util/include
#        ${POCO_INCLUDE}/Net/include
#        ${POCO_INCLUDE}/NetSSL_OpenSSL/include
#        ${POCO_INCLUDE}/Crypto/include
#        ${POCO_INCLUDE}/Zip/include
#        ${POCO_INCLUDE}/Data/include
#        ${POCO_INCLUDE}/Data/ODBC/include
#    )
endif()

set(POCO_LIBRARYDIRS ${POCO_LIB})
if (WIN32)
    if (MSVC)
        find_library(POCO_LIBRARY           NAMES PocoFoundationmd  PATHS ${POCO_LIBRARYDIRS})
        find_library(POCO_NET_LIBRARY       NAMES PocoNetmd         HINTS ${POCO_LIBRARYDIRS})
        find_library(POCO_UTIL_LIBRARY      NAMES PocoUtilmd        HINTS ${POCO_LIBRARYDIRS})
        find_library(POCO_JSON_LIBRARY      NAMES PocoJSONmd        HINTS ${POCO_LIBRARYDIRS})
        find_library(POCO_NETSSL_LIBRARY    NAMES PocoNetSSLmd      HINTS ${POCO_LIBRARYDIRS})
        find_library(POCO_XML_LIBRARY       NAMES PocoXMLmd         HINTS ${POCO_LIBRARYDIRS})
        find_library(POCO_CRYPTO_LIBRARY    NAMES PocoCryptomd      HINTS ${POCO_LIBRARYDIRS})
        find_library(POCO_ZIP_LIBRARY       NAMES PocoZipmd         HINTS ${POCO_LIBRARYDIRS})
        find_library(POCO_DATA_LIBRARY      NAMES PocoDatamd        HINTS ${POCO_LIBRARYDIRS})
        find_library(POCO_ODBC_LIBRARY      NAMES PocoDataODBCmd    HINTS ${POCO_LIBRARYDIRS})
    else()
        find_library(POCO_LIBRARY           NAMES PocoFoundation    PATHS ${POCO_LIBRARYDIRS})
        find_library(POCO_NET_LIBRARY       NAMES PocoNet           HINTS ${POCO_LIBRARYDIRS})
        find_library(POCO_UTIL_LIBRARY      NAMES PocoUtil          HINTS ${POCO_LIBRARYDIRS})
        find_library(POCO_JSON_LIBRARY      NAMES PocoJSON          HINTS ${POCO_LIBRARYDIRS})
        find_library(POCO_NETSSL_LIBRARY    NAMES PocoNetSSL        HINTS ${POCO_LIBRARYDIRS})
        find_library(POCO_XML_LIBRARY       NAMES PocoXML           HINTS ${POCO_LIBRARYDIRS})
        find_library(POCO_CRYPTO_LIBRARY    NAMES PocoCrypto        HINTS ${POCO_LIBRARYDIRS})
        find_library(POCO_ZIP_LIBRARY       NAMES PocoZip           HINTS ${POCO_LIBRARYDIRS})
        find_library(POCO_DATA_LIBRARY      NAMES PocoData          HINTS ${POCO_LIBRARYDIRS})
        find_library(POCO_ODBC_LIBRARY      NAMES PocoDataODBC      HINTS ${POCO_LIBRARYDIRS})
    endif()
else()
    find_library(POCO_LIBRARY           NAMES PocoFoundation PATHS ${POCO_LIBRARYDIRS})
    find_library(POCO_NET_LIBRARY       NAMES PocoNet        HINTS ${POCO_LIBRARYDIRS})
    find_library(POCO_UTIL_LIBRARY      NAMES PocoUtil       HINTS ${POCO_LIBRARYDIRS})
    find_library(POCO_NETSSL_LIBRARY    NAMES PocoNetSSL     HINTS ${POCO_LIBRARYDIRS})
    find_library(POCO_XML_LIBRARY       NAMES PocoXML        HINTS ${POCO_LIBRARYDIRS})
    find_library(POCO_CRYPTO_LIBRARY    NAMES PocoCrypto     HINTS ${POCO_LIBRARYDIRS})
    find_library(POCO_ZIP_LIBRARY       NAMES PocoZip        HINTS ${POCO_LIBRARYDIRS})
    find_library(POCO_DATA_LIBRARY      NAMES PocoData          HINTS ${POCO_LIBRARYDIRS})
    find_library(POCO_ODBC_LIBRARY      NAMES PocoDataODBC      HINTS ${POCO_LIBRARYDIRS})
    set(POCO_JSON_LIBRARY)
endif()

set(POCO_LIBRARIES
    ${POCO_LIBRARY}
    ${POCO_NET_LIBRARY}
    ${POCO_NETSSL_LIBRARY}
    ${POCO_JSON_LIBRARY}
    ${POCO_XML_LIBRARY}
    ${POCO_CRYPTO_LIBRARY}
    ${POCO_UTIL_LIBRARY}
    ${POCO_ZIP_LIBRARY})

if (WIN32)
    add_definitions(-DPOCO_WIN32_UTF8 -DPOCO_STATIC)
else()
    add_definitions(-DPOCO_STATIC)
endif()

if (WIN32)
    if (MSYS)
        find_library(ICONV_LIBRARY  NAMES libiconv.a ) # iconv)
        find_library(ICUIN_LIBRARY  NAMES libsicuin.a) # icuin)
        find_library(ICUUC_LIBRARY  NAMES libsicuuc.a) # icuuc)
        find_library(ICUDT_LIBRARY  NAMES libsicudt.a) # icuuc)
        set(BOOST_SUPPORT_LIBS Boost::disable_autolinking
            ${ICONV_LIBRARY} ${ICUIN_LIBRARY} ${ICUUC_LIBRARY} ${ICUDT_LIBRARY})
    else()
        set(BOOST_SUPPORT_LIBS Boost::disable_autolinking)
    endif()
else()
    find_library(ICUIN_LIBRARY  NAMES libicui18n.a) # icuin)
    find_library(ICUUC_LIBRARY  NAMES libicuuc.a)   # icuuc)
    find_library(ICUDT_LIBRARY  NAMES libicudata.a) # icuuc)
    set(BOOST_SUPPORT_LIBS ${ICUIN_LIBRARY} ${ICUUC_LIBRARY} ${ICUDT_LIBRARY})
endif()

#
# Qt Setup
#
find_package(Qt5Core)
find_package(Qt5Widgets)
find_package(Qt5PrintSupport)
find_package(Qt5Gui)

#if (Qt5Widgets_FOUND)
#    add_definitions(-DHAS_QT5=1)
#    if (NOT WIN32)
#        add_definitions(-fPIC)
#    endif()
#    add_library(bkl_mod_qt STATIC
#        modules/qt/init.cpp
#        modules/qt/line_editor.cpp
#    )
#    if (MSVC)
#        set(QT_LIBRARIES Qt5::Core Qt5::Widgets)
#    else()
#        set(QT_LIBRARIES Qt5::Core Qt5::Widgets) # msys2: -ljasper -ldbus-1 -lwebp)
#    endif()
#    set(QT_INCLUDEDIRS ${Qt5Widgets_INCLUDE_DIRS}
#                       ${Qt5Core_INCLUDE_DIRS})
#    TARGET_LINK_LIBRARIES(lalrt_support lalrt_qt ${QT_LIBRARIES})
#else()
    add_definitions(-DHAS_QT5=0)
    set(QT_LIBRARIES)
    set(QT_INCLUDEDIRS)
#endif()

include_directories(${Boost_INCLUDE_DIRS} ${POCO_INCLUDEDIRS} ${QT_INCLUDEDIRS})

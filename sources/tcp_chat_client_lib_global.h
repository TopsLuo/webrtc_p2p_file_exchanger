#ifndef TCP_CHAT_CLIENT_LIB_GLOBAL_H
#define TCP_CHAT_CLIENT_LIB_GLOBAL_H

#include <QtCore/qglobal.h>

#if defined(TCP_CHAT_CLIENT_LIB_LIBRARY)
#  define TCP_CHAT_CLIENT_LIBSHARED_EXPORT Q_DECL_EXPORT
#else
#  define TCP_CHAT_CLIENT_LIBSHARED_EXPORT Q_DECL_IMPORT
#endif

#endif // TCP_CHAT_CLIENT_LIB_GLOBAL_H

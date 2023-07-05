
#ifndef TINYTIFF_EXPORT_H
#define TINYTIFF_EXPORT_H

#ifdef TINYTIFF_STATIC_DEFINE
#  define TINYTIFF_EXPORT
#  define TINYTIFF_NO_EXPORT
#else
#  ifndef TINYTIFF_EXPORT
#    ifdef TinyTIFF_EXPORTS
        /* We are building this library */
#      define TINYTIFF_EXPORT 
#    else
        /* We are using this library */
#      define TINYTIFF_EXPORT 
#    endif
#  endif

#  ifndef TINYTIFF_NO_EXPORT
#    define TINYTIFF_NO_EXPORT 
#  endif
#endif

#ifndef TINYTIFF_DEPRECATED
#  define TINYTIFF_DEPRECATED __declspec(deprecated)
#endif

#ifndef TINYTIFF_DEPRECATED_EXPORT
#  define TINYTIFF_DEPRECATED_EXPORT TINYTIFF_EXPORT TINYTIFF_DEPRECATED
#endif

#ifndef TINYTIFF_DEPRECATED_NO_EXPORT
#  define TINYTIFF_DEPRECATED_NO_EXPORT TINYTIFF_NO_EXPORT TINYTIFF_DEPRECATED
#endif

#if 0 /* DEFINE_NO_DEPRECATED */
#  ifndef TINYTIFF_NO_DEPRECATED
#    define TINYTIFF_NO_DEPRECATED
#  endif
#endif

#endif /* TINYTIFF_EXPORT_H */

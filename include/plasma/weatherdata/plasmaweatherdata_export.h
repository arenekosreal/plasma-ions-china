
#ifndef PLASMAWEATHERDATA_EXPORT_H
#define PLASMAWEATHERDATA_EXPORT_H

#ifdef PLASMAWEATHERDATA_STATIC_DEFINE
#  define PLASMAWEATHERDATA_EXPORT
#  define PLASMAWEATHERDATA_NO_EXPORT
#else
#  ifndef PLASMAWEATHERDATA_EXPORT
#    ifdef plasmaweatherdata_EXPORTS
        /* We are building this library */
#      define PLASMAWEATHERDATA_EXPORT __attribute__((visibility("default")))
#    else
        /* We are using this library */
#      define PLASMAWEATHERDATA_EXPORT __attribute__((visibility("default")))
#    endif
#  endif

#  ifndef PLASMAWEATHERDATA_NO_EXPORT
#    define PLASMAWEATHERDATA_NO_EXPORT __attribute__((visibility("hidden")))
#  endif
#endif

#ifndef PLASMAWEATHERDATA_DEPRECATED
#  define PLASMAWEATHERDATA_DEPRECATED __attribute__ ((__deprecated__))
#endif

#ifndef PLASMAWEATHERDATA_DEPRECATED_EXPORT
#  define PLASMAWEATHERDATA_DEPRECATED_EXPORT PLASMAWEATHERDATA_EXPORT PLASMAWEATHERDATA_DEPRECATED
#endif

#ifndef PLASMAWEATHERDATA_DEPRECATED_NO_EXPORT
#  define PLASMAWEATHERDATA_DEPRECATED_NO_EXPORT PLASMAWEATHERDATA_NO_EXPORT PLASMAWEATHERDATA_DEPRECATED
#endif

/* NOLINTNEXTLINE(readability-avoid-unconditional-preprocessor-if) */
#if 0 /* DEFINE_NO_DEPRECATED */
#  ifndef PLASMAWEATHERDATA_NO_DEPRECATED
#    define PLASMAWEATHERDATA_NO_DEPRECATED
#  endif
#endif

#endif /* PLASMAWEATHERDATA_EXPORT_H */

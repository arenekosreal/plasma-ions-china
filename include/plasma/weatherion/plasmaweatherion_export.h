
#ifndef PLASMAWEATHERION_EXPORT_H
#define PLASMAWEATHERION_EXPORT_H

#ifdef PLASMAWEATHERION_STATIC_DEFINE
#  define PLASMAWEATHERION_EXPORT
#  define PLASMAWEATHERION_NO_EXPORT
#else
#  ifndef PLASMAWEATHERION_EXPORT
#    ifdef plasmaweatherion_EXPORTS
        /* We are building this library */
#      define PLASMAWEATHERION_EXPORT __attribute__((visibility("default")))
#    else
        /* We are using this library */
#      define PLASMAWEATHERION_EXPORT __attribute__((visibility("default")))
#    endif
#  endif

#  ifndef PLASMAWEATHERION_NO_EXPORT
#    define PLASMAWEATHERION_NO_EXPORT __attribute__((visibility("hidden")))
#  endif
#endif

#ifndef PLASMAWEATHERION_DEPRECATED
#  define PLASMAWEATHERION_DEPRECATED __attribute__ ((__deprecated__))
#endif

#ifndef PLASMAWEATHERION_DEPRECATED_EXPORT
#  define PLASMAWEATHERION_DEPRECATED_EXPORT PLASMAWEATHERION_EXPORT PLASMAWEATHERION_DEPRECATED
#endif

#ifndef PLASMAWEATHERION_DEPRECATED_NO_EXPORT
#  define PLASMAWEATHERION_DEPRECATED_NO_EXPORT PLASMAWEATHERION_NO_EXPORT PLASMAWEATHERION_DEPRECATED
#endif

/* NOLINTNEXTLINE(readability-avoid-unconditional-preprocessor-if) */
#if 0 /* DEFINE_NO_DEPRECATED */
#  ifndef PLASMAWEATHERION_NO_DEPRECATED
#    define PLASMAWEATHERION_NO_DEPRECATED
#  endif
#endif

#endif /* PLASMAWEATHERION_EXPORT_H */

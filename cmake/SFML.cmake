# SFML 3: se usa una instalación del sistema si existe; si no (Fedora solo
# empaqueta SFML 2.6), se descarga y compila como librería estática.
find_package(SFML 3 COMPONENTS Graphics QUIET)

if(NOT SFML_FOUND)
    message(STATUS "SFML 3 no encontrado en el sistema: se descargará SFML 3.1.0")
    set(SFML_BUILD_AUDIO OFF CACHE BOOL "" FORCE)
    set(SFML_BUILD_NETWORK OFF CACHE BOOL "" FORCE)
    set(BUILD_SHARED_LIBS OFF)
    FetchContent_Declare(SFML
        GIT_REPOSITORY https://github.com/SFML/SFML.git
        GIT_TAG 3.1.0
        GIT_SHALLOW ON
        EXCLUDE_FROM_ALL
        SYSTEM)
    FetchContent_MakeAvailable(SFML)
endif()

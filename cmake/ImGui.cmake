# Dear ImGui + ImGui-SFML para la interfaz gráfica (wrl_gui). Se compilan como
# una librería estática propia en vez de usar el CMake de ImGui-SFML, que exige
# un ImGui instalado con find_package.
find_package(OpenGL REQUIRED)

FetchContent_Declare(imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.91.9
    GIT_SHALLOW ON
    SOURCE_SUBDIR sin_cmake)
FetchContent_Declare(imgui_sfml
    GIT_REPOSITORY https://github.com/SFML/imgui-sfml.git
    GIT_TAG v3.0
    GIT_SHALLOW ON
    SOURCE_SUBDIR sin_cmake)
FetchContent_MakeAvailable(imgui imgui_sfml)

add_library(imgui_sfml STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_sfml_SOURCE_DIR}/imgui-SFML.cpp)
target_include_directories(imgui_sfml SYSTEM PUBLIC ${imgui_SOURCE_DIR} ${imgui_sfml_SOURCE_DIR})
target_compile_definitions(imgui_sfml PUBLIC IMGUI_USER_CONFIG="imconfig-SFML.h")
# ImGui-SFML 3.0 usa algunas funciones de SFML 3.1 marcadas como obsoletas.
target_compile_options(imgui_sfml PRIVATE -Wno-deprecated-declarations)
target_link_libraries(imgui_sfml PUBLIC SFML::Graphics OpenGL::GL)

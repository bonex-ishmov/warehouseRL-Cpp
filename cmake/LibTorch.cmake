# LibTorch (API C++ de PyTorch), descargado una sola vez a .deps/.
#
# No se usa find_package(Torch): su configuración hace enable_language(CUDA) y
# exige un nvcc compatible con el GCC del sistema (GCC 16 no lo es con ningún
# CUDA actual). El proyecto no compila kernels propios, así que basta con
# enlazar las librerías ya compiladas, que traen su propio runtime de CUDA.
#
# Para reutilizar una libtorch ya extraída: -DLIBTORCH_DIR=/ruta/a/libtorch
# Para una versión solo CPU (por ejemplo en CI): -DWRL_TORCH_VARIANT=cpu

set(WRL_LIBTORCH_VERSION "2.14.1")
set(WRL_TORCH_VARIANT "cu130" CACHE STRING "Variante de LibTorch: cu130 o cpu")
set(LIBTORCH_DIR "" CACHE PATH "Ruta a una libtorch extraída (vacío = descargar a .deps/)")

if(NOT LIBTORCH_DIR)
    set(_deps "${PROJECT_SOURCE_DIR}/.deps")
    set(_name "libtorch-${WRL_LIBTORCH_VERSION}-${WRL_TORCH_VARIANT}")
    set(_dir "${_deps}/${_name}")

    if(NOT EXISTS "${_dir}/lib/libtorch.so")
        set(_zip "${_deps}/${_name}.zip")
        if(NOT EXISTS "${_zip}")
            set(_url "https://download.pytorch.org/libtorch/${WRL_TORCH_VARIANT}/libtorch-shared-with-deps-${WRL_LIBTORCH_VERSION}%2B${WRL_TORCH_VARIANT}.zip")
            message(STATUS "Descargando LibTorch ${WRL_LIBTORCH_VERSION}+${WRL_TORCH_VARIANT}...")
            file(DOWNLOAD "${_url}" "${_zip}.part" SHOW_PROGRESS STATUS _status)
            list(GET _status 0 _code)
            if(NOT _code EQUAL 0)
                file(REMOVE "${_zip}.part")
                message(FATAL_ERROR "No se pudo descargar LibTorch: ${_status}")
            endif()
            file(RENAME "${_zip}.part" "${_zip}")
        endif()
        message(STATUS "Extrayendo LibTorch...")
        file(REMOVE_RECURSE "${_deps}/extract-tmp")
        file(ARCHIVE_EXTRACT INPUT "${_zip}" DESTINATION "${_deps}/extract-tmp")
        file(RENAME "${_deps}/extract-tmp/libtorch" "${_dir}")
        file(REMOVE_RECURSE "${_deps}/extract-tmp")
        file(REMOVE "${_zip}")
    endif()
    set(LIBTORCH_DIR "${_dir}" CACHE PATH "" FORCE)
endif()

set(_lib "${LIBTORCH_DIR}/lib")
if(NOT EXISTS "${_lib}/libtorch.so")
    message(FATAL_ERROR "LIBTORCH_DIR=${LIBTORCH_DIR} no contiene lib/libtorch.so")
endif()
message(STATUS "LibTorch: ${LIBTORCH_DIR}")

add_library(Torch::Torch INTERFACE IMPORTED)
target_include_directories(Torch::Torch SYSTEM INTERFACE
    "${LIBTORCH_DIR}/include"
    "${LIBTORCH_DIR}/include/torch/csrc/api/include")
set(_torch_libs "${_lib}/libtorch.so" "${_lib}/libtorch_cpu.so" "${_lib}/libc10.so")
if(EXISTS "${_lib}/libtorch_cuda.so")
    # --no-as-needed: nadie llama directamente a símbolos de libtorch_cuda, pero
    # hay que cargarla para que registre el backend CUDA.
    target_link_options(Torch::Torch INTERFACE "LINKER:--no-as-needed")
    list(PREPEND _torch_libs "${_lib}/libtorch_cuda.so" "${_lib}/libc10_cuda.so")
endif()
target_link_libraries(Torch::Torch INTERFACE ${_torch_libs})

# CUDA Graphs: solo hacen falta las cabeceras del runtime de CUDA (no nvcc).
set(_cuda_inc "")
foreach(_c "$ENV{CUDA_HOME}/include" "/usr/local/cuda/include" "/opt/cuda/include")
    if(EXISTS "${_c}/cuda_runtime_api.h")
        set(_cuda_inc "${_c}")
        break()
    endif()
endforeach()
if(EXISTS "${_lib}/libtorch_cuda.so" AND _cuda_inc)
    target_include_directories(Torch::Torch SYSTEM INTERFACE "${_cuda_inc}")
    target_compile_definitions(Torch::Torch INTERFACE WRL_CUDA_GRAPHS=1)
    # Las cabeceras en línea llaman al runtime: se enlaza el que trae LibTorch.
    file(GLOB _cudart "${_lib}/libcudart*.so*")
    if(_cudart)
        list(GET _cudart 0 _cudart)
        target_link_libraries(Torch::Torch INTERFACE "${_cudart}")
    endif()
    message(STATUS "CUDA Graphs habilitados (cabeceras en ${_cuda_inc})")
else()
    target_compile_definitions(Torch::Torch INTERFACE WRL_CUDA_GRAPHS=0)
endif()

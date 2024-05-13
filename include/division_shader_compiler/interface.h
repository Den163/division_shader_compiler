#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "division_shader_compiler_export.h"

#ifdef __cplusplus
extern "C"
{
#endif

    typedef enum DivisionCompilerShaderType
      : int32_t
    {
        DIVISION_COMPILER_SHADER_TYPE_VERTEX = 1,
        DIVISION_COMPILER_SHADER_TYPE_FRAGMENT = 2
    } DivisionCompilerShaderType;

    typedef struct DivisionShaderCompilerContext
    {
        void* spirv_buffer;
        char* output_src_buffer;
        size_t spirv_buffer_size;
        size_t output_src_buffer_size;
    } DivisionShaderCompilerContext;

    DIVISION_EXPORT bool
    division_shader_compiler_alloc(DivisionShaderCompilerContext** ctx);
    DIVISION_EXPORT void
    division_shader_compiler_free(DivisionShaderCompilerContext* ctx);

    DIVISION_EXPORT bool division_shader_compiler_compile_glsl_to_spirv(
        DivisionShaderCompilerContext* ctx,
        const char* source,
        int32_t source_size,
        DivisionCompilerShaderType shader_type,
        const char* spirv_entry_point_name,
        size_t* out_spirv_byte_count
    );

    DIVISION_EXPORT bool division_shader_compiler_compile_spirv_to_metal(
        DivisionShaderCompilerContext* ctx,
        const void* spirv_bytes,
        size_t spirv_byte_count,
        DivisionCompilerShaderType shader_type,
        const char* entry_point,
        size_t* out_metal_size
    );

#ifdef __cplusplus
}
#endif
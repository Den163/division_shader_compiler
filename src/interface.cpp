#include "division_shader_compiler/interface.h"

#include <SPIRV/GlslangToSpv.h>
#include <cstdio>
#include <glslang/Public/ResourceLimits.h>
#include <glslang/Public/ShaderLang.h>
#include <spirv_msl.hpp>

#define DIVISION_BUFFER_INIT_SIZE 1024

static inline EShLanguage
division_shader_type_to_glslang_type(DivisionCompilerShaderType shader_type);

static inline spv::ExecutionModel
division_shader_type_to_spv_cross_type(DivisionCompilerShaderType shader_type);

bool division_shader_compiler_alloc(DivisionShaderCompilerContext** ctx)
{
    auto* context = static_cast<DivisionShaderCompilerContext*>(
        malloc(sizeof(DivisionShaderCompilerContext))
    );
    if (context == nullptr)
    {
        fprintf(stderr, "Failed to alloc shader compiler context");
        return false;
    }

    context->spirv_buffer = malloc(DIVISION_BUFFER_INIT_SIZE);
    context->output_src_buffer = static_cast<char*>(malloc(DIVISION_BUFFER_INIT_SIZE));

    if (context->spirv_buffer == nullptr || context->output_src_buffer == nullptr)
    {
        fprintf(stderr, "Failed to allocate shader compiler buffers");
        free(context);
        return false;
    }

    context->output_src_buffer_size = DIVISION_BUFFER_INIT_SIZE;
    context->spirv_buffer_size = DIVISION_BUFFER_INIT_SIZE;

    if (!glslang::InitializeProcess())
    {
        fprintf(stderr, "Failed to init glslang");
        free(context);
        return false;
    }

    *ctx = context;
    return true;
}

void division_shader_compiler_free(DivisionShaderCompilerContext* ctx)
{
    free(ctx->spirv_buffer);
    free(ctx->output_src_buffer);

    glslang::FinalizeProcess();
}

bool division_shader_compiler_compile_glsl_to_spirv(
    DivisionShaderCompilerContext* ctx,
    const char* source,
    int32_t source_size,
    DivisionCompilerShaderType shader_type,
    const char* spirv_entry_point_name,
    size_t* out_spirv_byte_count
)
{
    EShLanguage glslang_shader_type;
    try
    {
        glslang_shader_type = division_shader_type_to_glslang_type(shader_type);
    }
    catch (std::runtime_error& e)
    {
        fprintf(stderr, "%s\n", e.what());
        return false;
    }

    glslang::TShader shader { glslang_shader_type };
    const TBuiltInResource* default_resource = GetDefaultResources();

    shader.setPreamble("#extension GL_GOOGLE_include_directive: enable\n");
    shader.setEnvInput(
        glslang::EShSource::EShSourceGlsl,
        glslang_shader_type,
        glslang::EShClientOpenGL,
        450
    );
    shader.setEnvClient(
        glslang::EShClient::EShClientOpenGL,
        glslang::EshTargetClientVersion::EShTargetOpenGL_450
    );
    shader.setEnvTarget(
        glslang::EShTargetLanguage::EShTargetSpv,
        glslang::EShTargetLanguageVersion::EShTargetSpv_1_5
    );
    shader.setStringsWithLengths(&source, &source_size, 1);
    if (spirv_entry_point_name)
    {
        shader.setSourceEntryPoint("main");
        shader.setEntryPoint(spirv_entry_point_name);
    }

    if (!shader.parse(default_resource, 450, false, EShMessages::EShMsgDefault))
    {
        const char* info_log = shader.getInfoLog();
        fprintf(stderr, "Failed to parse a shader. Log: %s\n", info_log);
        return false;
    }

    glslang::TProgram program {};
    program.addShader(&shader);
    if (!program.link(EShMessages::EShMsgDefault))
    {
        const char* info_log = program.getInfoLog();
        fprintf(stderr, "Failed to link a program. Log: %s\n", info_log);
        return false;
    }

    const glslang::TIntermediate* intermediate =
        program.getIntermediate(glslang_shader_type);
    std::vector<uint32_t> spv {};
    glslang::SpvOptions options {
        .generateDebugInfo = false,
        .stripDebugInfo = false,
        .disableOptimizer = false,
        .optimizeSize = true,
        .disassemble = false,
        .validate = true,
        .emitNonSemanticShaderDebugInfo = false,
        .emitNonSemanticShaderDebugSource = false
    };

    spv::SpvBuildLogger logger {};
    glslang::GlslangToSpv(*intermediate, spv, &logger, &options);

    auto msg = logger.getAllMessages();

    if (!msg.empty())
    {
        printf("SPIRV log messages: \n%s\n", msg.c_str());
    }

    size_t spv_size = sizeof(uint32_t[spv.size()]);
    if (spv_size > ctx->spirv_buffer_size)
    {
        ctx->spirv_buffer = realloc(ctx->spirv_buffer, spv_size);
        ctx->spirv_buffer_size = spv_size;

        if (ctx->spirv_buffer == nullptr)
        {
            fprintf(stderr, "Failed to realloc shader compiler buffer");
            return false;
        }
    }

    memcpy(ctx->spirv_buffer, spv.data(), spv_size);
    *out_spirv_byte_count = spv_size;

    return true;
}

bool division_shader_compiler_compile_spirv_to_metal(
    DivisionShaderCompilerContext* ctx,
    const void* spirv_bytes,
    size_t spirv_byte_count,
    DivisionCompilerShaderType shader_type,
    const char* entry_point,
    size_t* out_metal_size
)
{
    try
    {
        size_t spv_size = spirv_byte_count / sizeof(uint32_t);
        spirv_cross::CompilerMSL msl { (const uint32_t*)spirv_bytes, spv_size };

        msl.set_entry_point(
            entry_point, division_shader_type_to_spv_cross_type(shader_type)
        );
        spirv_cross::CompilerMSL::Options opt {
            .msl_version = spirv_cross::CompilerMSL::Options::make_msl_version(3, 0),
            .enable_decoration_binding = true,
        };

        msl.set_msl_options(opt);
        std::string out_metal = msl.compile();

        size_t src_size = out_metal.size() + 1;

        if (src_size > ctx->output_src_buffer_size)
        {
            ctx->output_src_buffer =
                static_cast<char*>(realloc(ctx->output_src_buffer, src_size));
            ctx->output_src_buffer_size = src_size;

            if (ctx->output_src_buffer == nullptr)
            {
                fprintf(stderr, "Failed to realloc shader compiler buffer");
                return false;
            }
        }

        memcpy(ctx->output_src_buffer, out_metal.c_str(), src_size);
        *out_metal_size = src_size;
        return true;
    }
    catch (std::runtime_error& e)
    {
        fprintf(stderr, "Error while compiling spirv to msl: %s\n", e.what());
        return false;
    }
}

EShLanguage division_shader_type_to_glslang_type(DivisionCompilerShaderType shader_type)
{
    switch (shader_type)
    {
        case DIVISION_COMPILER_SHADER_TYPE_VERTEX:
            return EShLanguage::EShLangVertex;
        case DIVISION_COMPILER_SHADER_TYPE_FRAGMENT:
            return EShLanguage::EShLangFragment;
        default:
            throw std::runtime_error("Unknown shader typ to EShLanguage mapping");
    }
}

spv::ExecutionModel
division_shader_type_to_spv_cross_type(DivisionCompilerShaderType shader_type)
{
    switch (shader_type)
    {
        case DIVISION_COMPILER_SHADER_TYPE_VERTEX:
            return spv::ExecutionModel::ExecutionModelVertex;
        case DIVISION_COMPILER_SHADER_TYPE_FRAGMENT:
            return spv::ExecutionModel::ExecutionModelFragment;
        default:
            throw std::runtime_error("Unknown shader type to spv::ExecutionModel mapping"
            );
    }
}

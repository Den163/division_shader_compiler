#include "division_shader_compiler/interface.h"

#include <filesystem>
#include <fstream>
#include <ios>
#include <iostream>
#include <string>

struct ContextOwner
{
    ContextOwner()
      : ctx_ptr(nullptr)
    {
        division_shader_compiler_alloc(&ctx_ptr);
    }
    ~ContextOwner() { division_shader_compiler_free(ctx_ptr); }

    DivisionShaderCompilerContext* ctx_ptr;
};

static bool
read_file_to_str(const std::filesystem::path& input_path, std::string& output_string)
{
    std::ifstream input_file { input_path };
    if (!input_file.is_open())
    {
        std::cerr << "Can't open file: '" << input_path << "'" << std::endl;
        return false;
    }

    output_string.clear();
    std::string l;
    while (std::getline(input_file, l))
    {
        output_string += l;
    }

    return true;
}

static bool write_file_from_str(
    const std::filesystem::path& output_path,
    const std::string& string_to_write
)
{
    std::ofstream output_file { output_path };
    if (output_file.is_open())
    {
        std::cout << "Can't create/open file '" << output_path << "'" << std::endl;
        return false;
    }

    output_file << string_to_write;

    return true;
}

int main(int argc, char** argv)
{
    using namespace std::filesystem;

    if (argc != 4)
    {
        std::cerr << "Wrong arguments. "
                  << "Usage: >division_shader_compiler <--vertex|--fragment> "
                  << "<input shader path> "
                  << "<output shader path>" << std::endl;
        return -1;
    }

    std::string shader_type_str = argv[1];
    DivisionCompilerShaderType shader_type;
    std::string msl_entry_point;

    if (shader_type_str == "vertex")
    {
        shader_type = DIVISION_COMPILER_SHADER_TYPE_VERTEX;
        msl_entry_point = "vert";
    }
    else if (shader_type_str == "fragment")
    {
        shader_type = DIVISION_COMPILER_SHADER_TYPE_FRAGMENT;
        msl_entry_point = "frag";
    }
    else
    {
        std::cerr << "The first (shader type) argument must be '--vertex' or '--fragment'"
                  << std::endl;
        return -1;
    }

    path input_path = argv[2];
    path output_path = argv[3];

    if (!exists(input_path))
    {
        std::cerr << "Input file does not exist" << std::endl;
        return -1;
    }

    ContextOwner ctx_owner {};

    std::string input_source {};
    if (read_file_to_str(input_path, input_source))
    {
        return -1;
    }

    size_t spirv_bye_count = 0;
    if (!division_shader_compiler_compile_glsl_to_spirv(
            ctx_owner.ctx_ptr,
            input_source.data(),
            input_source.size(),
            DIVISION_COMPILER_SHADER_TYPE_FRAGMENT,
            msl_entry_point.data(),
            &spirv_bye_count
        ))
    {
        std::cerr << "Failed to compile glsl from file '" << input_path << "' to spirv"
                  << std::endl;
        return -1;
    }

    size_t out_msl_size = 0;
    if (!division_shader_compiler_compile_spirv_to_metal(
            ctx_owner.ctx_ptr,
            ctx_owner.ctx_ptr->spirv_buffer,
            ctx_owner.ctx_ptr->spirv_buffer_size,
            shader_type,
            msl_entry_point.data(),
            &out_msl_size
        ))
    {
        std::cerr << "Failed to compile spirv to metal (source glsl file: '" << input_path
                  << "')" << std::endl;
        return -1;
    }

    write_file_from_str(
        output_path,
        std::string {
            ctx_owner.ctx_ptr->output_src_buffer,
            ctx_owner.ctx_ptr->output_src_buffer_size,
        }
    );

    return 0;
}
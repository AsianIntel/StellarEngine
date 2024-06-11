module;

#include <wrl/client.h>
#include <dxcapi.h>
#include <iostream>
#include <vector>

export module stellar.render.vulkan.shader;

import stellar.core.result;

template<typename T>
using ComPtr = Microsoft::WRL::ComPtr<T>;

constexpr GUID GUID_DxcLibrary = {
    0x6245d6af,
    0x66e0,
    0x48fd,
    {0x80, 0xb4, 0x4d, 0x27, 0x17, 0x96, 0x74, 0x8c} };

constexpr CLSID GUID_DxcCompiler = {
    0x73e22d93,
    0xe6ce,
    0x47f3,
    {0xb5, 0xbf, 0xf0, 0x66, 0x4f, 0x39, 0xc1, 0xb0} };

export struct ShaderCompiler {
    ComPtr<IDxcUtils> utils{};
    ComPtr<IDxcCompiler3> compiler{};

    Result<void, HRESULT> initialize() {
        if (const auto res = DxcCreateInstance(GUID_DxcLibrary, IID_PPV_ARGS(&utils)); FAILED(res)) {
            return Err(res);
        }
        if (const auto res = DxcCreateInstance(GUID_DxcCompiler, IID_PPV_ARGS(&compiler)); FAILED(res)) {
            return Err(res);
        }
        return Ok();
    }

    [[nodiscard]] std::vector<uint8_t> compile(std::string_view source, std::string_view entrypoint, std::string_view target, const std::vector<std::string>& defines) const {
        ComPtr<IDxcBlobEncoding> source_blob;
        utils->CreateBlob(source.data(), source.size(), CP_UTF8, source_blob.GetAddressOf());

        std::vector<LPCWSTR> arguments;
        arguments.push_back(L"-E");
        auto w_entrypoint = std::wstring(entrypoint.begin(), entrypoint.end());
        arguments.push_back(w_entrypoint.c_str());
        arguments.push_back(L"-T");
        auto w_target = std::wstring(target.begin(), target.end());
        arguments.push_back(w_target.c_str());
        arguments.push_back(L"-spirv");
        arguments.push_back(DXC_ARG_WARNINGS_ARE_ERRORS);
        arguments.push_back(DXC_ARG_DEBUG);

        std::vector<std::wstring> w_defines;
        for (const auto& define: defines) {
            w_defines.push_back(std::wstring(define.begin(), define.end()));
        }
        for (const auto& define: w_defines) {
            arguments.push_back(L"-D");
            arguments.push_back(define.c_str());
        }

        DxcBuffer source_buffer {
            .Ptr = source_blob->GetBufferPointer(),
            .Size = source_blob->GetBufferSize(),
            .Encoding = 0
        };

        ComPtr<IDxcResult> compile_result;
        compiler->Compile(&source_buffer, arguments.data(), arguments.size(), nullptr, IID_PPV_ARGS(&compile_result));

        ComPtr<IDxcBlobUtf8> errors;
        compile_result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&errors), nullptr);
        if (errors && errors->GetStringLength() > 0) {
            std::cout << static_cast<char*>(errors->GetBufferPointer());
            __debugbreak();
        }

        ComPtr<IDxcBlob> output;
        compile_result->GetResult(&output);
        
        std::vector res(static_cast<uint8_t*>(output->GetBufferPointer()), static_cast<uint8_t*>(output->GetBufferPointer()) + output->GetBufferSize());
        return res;
    }
};
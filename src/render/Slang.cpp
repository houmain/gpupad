
#include "Slang.h"

#include <slang.h>
#include <slang-com-ptr.h>
#include <slang-com-helper.h>

#include <array>

void staticInitSlang()
{
    Slang::ComPtr<slang::IGlobalSession> globalSession;
    createGlobalSession(globalSession.writeRef());
    slang::SessionDesc sessionDesc = {};

    slang::TargetDesc targetDesc = {};
    targetDesc.format = SLANG_SPIRV;
    targetDesc.profile = globalSession->findProfile("spirv_1_5");

    sessionDesc.targets = &targetDesc;
    sessionDesc.targetCount = 1;

    std::array<slang::PreprocessorMacroDesc, 2> preprocessorMacroDesc = {
        slang::PreprocessorMacroDesc{ "BIAS_VALUE", "1138" },
        slang::PreprocessorMacroDesc{ "OTHER_MACRO", "float" }
    };
    sessionDesc.preprocessorMacros = preprocessorMacroDesc.data();
    sessionDesc.preprocessorMacroCount = preprocessorMacroDesc.size();

    std::array<slang::CompilerOptionEntry, 1> options = {
        { slang::CompilerOptionName::EmitSpirvDirectly,
            { slang::CompilerOptionValueKind::Int, 1, 0, nullptr, nullptr } }
    };
    sessionDesc.compilerOptionEntries = options.data();
    sessionDesc.compilerOptionEntryCount = options.size();

    Slang::ComPtr<slang::ISession> session;
    globalSession->createSession(sessionDesc, session.writeRef());

    Slang::ComPtr<slang::IModule> slangModule;
    {
        Slang::ComPtr<slang::IBlob> diagnosticsBlob;
        const char *source = R"slang(
              
StructuredBuffer<float> buffer0;
StructuredBuffer<float> buffer1;
RWStructuredBuffer<float> result;

[shader("compute")]
[numthreads(1,1,1)]
void computeMain(uint3 threadId : SV_DispatchThreadID)
{
    uint index = threadId.x;
    result[index] = buffer0[index] + buffer1[index];
}  
              )slang";

        const char *moduleName = "shortest";
        const char *modulePath = "shortest.slang";
        slangModule = session->loadModuleFromSourceString(moduleName,
            modulePath, source, diagnosticsBlob.writeRef());
        //diagnoseIfNeeded(diagnosticsBlob);
        //if (!slangModule) {
        //    return -1;
        //}
    }

    Slang::ComPtr<slang::IEntryPoint> entryPoint;
    {
        Slang::ComPtr<slang::IBlob> diagnosticsBlob;
        slangModule->findEntryPointByName("computeMain", entryPoint.writeRef());
        //if (!entryPoint) {
        //    std::cout << "Error getting entry point" << std::endl;
        //    return -1;
        //}
    }

    std::array<slang::IComponentType *, 2> componentTypes = { slangModule,
        entryPoint };

    Slang::ComPtr<slang::IComponentType> composedProgram;
    {
        Slang::ComPtr<slang::IBlob> diagnosticsBlob;
        SlangResult result = session->createCompositeComponentType(
            componentTypes.data(), componentTypes.size(),
            composedProgram.writeRef(), diagnosticsBlob.writeRef());
        //diagnoseIfNeeded(diagnosticsBlob);
        //SLANG_RETURN_ON_FAIL(result);
    }

    Slang::ComPtr<slang::IComponentType> linkedProgram;
    {
        Slang::ComPtr<slang::IBlob> diagnosticsBlob;
        SlangResult result = composedProgram->link(linkedProgram.writeRef(),
            diagnosticsBlob.writeRef());
        //diagnoseIfNeeded(diagnosticsBlob);
        //SLANG_RETURN_ON_FAIL(result);
    }

    if (true) {

        Slang::ComPtr<slang::IBlob> spirvCode;
        {
            Slang::ComPtr<slang::IBlob> diagnosticsBlob;
            SlangResult result =
                linkedProgram->getEntryPointCode(0, // entryPointIndex
                    0, // targetIndex
                    spirvCode.writeRef(), diagnosticsBlob.writeRef());
            //diagnoseIfNeeded(diagnosticsBlob);
            //SLANG_RETURN_ON_FAIL(result);
        }

    } else {
        Slang::ComPtr<slang::IBlob> spirvCode;
        {
            Slang::ComPtr<slang::IBlob> diagnosticsBlob;
            SlangResult result = linkedProgram->getTargetCode(0, // targetIndex
                spirvCode.writeRef(), diagnosticsBlob.writeRef());
            //diagnoseIfNeeded(diagnosticsBlob);
            //SLANG_RETURN_ON_FAIL(result);
        }
    }
}
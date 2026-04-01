// dllmain.cpp : 定义 DLL 应用程序的入口点。
#include "pch.h"

#include <cstring>
#include <cwchar>
#include <string>

#include <ProToolkit.h>
#include <ProUICmd.h>
#include <ProDrawing.h>
#include <ProMdl.h>
#include <ProMenuBar.h>

namespace
{
constexpr char kMenuName[] = "AutoDrawingMenu";
constexpr char kMenuLabelKey[] = "AutoDrawingMenuLabel";
constexpr char kActionName[] = "CreateGbDrawingAction";
constexpr char kButtonName[] = "CreateGbDrawingButton";
constexpr char kButtonLabelKey[] = "CreateGbDrawingLabel";
constexpr char kButtonHelpKey[] = "CreateGbDrawingHelp";
constexpr wchar_t kMessageFileName[] = L"creo_autodraw_messages.txt";
constexpr wchar_t kDefaultTemplateName[] = L"GB_A3";
constexpr wchar_t kDialogTitle[] = L"Creo Auto Drawing";
constexpr double kDefaultPaperWidthInch = 16.5354;
constexpr double kDefaultPaperHeightInch = 11.6929;

uiCmdCmdId g_actionId = 0;

void ShowMessage(const std::wstring& text, UINT flags = MB_OK | MB_ICONINFORMATION)
{
    MessageBoxW(nullptr, text.c_str(), kDialogTitle, flags);
}

std::wstring FormatErrorMessage(const wchar_t* prefix, ProError error)
{
    wchar_t buffer[512]{};
    swprintf_s(buffer, L"%ls\r\n错误码: %d", prefix, static_cast<int>(error));
    return buffer;
}

void CopyWideString(const wchar_t* source, wchar_t* destination, size_t destinationSize)
{
    if (destinationSize == 0)
    {
        return;
    }

    wcsncpy_s(destination, destinationSize, source, _TRUNCATE);
}

std::wstring GetTemplateName()
{
    wchar_t buffer[PRO_MDLNAME_SIZE]{};
    const DWORD valueLength = GetEnvironmentVariableW(L"CREO_GB_DRAWING_TEMPLATE", buffer, static_cast<DWORD>(_countof(buffer)));
    if (valueLength > 0 && valueLength < _countof(buffer))
    {
        return buffer;
    }

    return kDefaultTemplateName;
}

double GetConfiguredBaseScale()
{
    wchar_t buffer[64]{};
    const DWORD valueLength = GetEnvironmentVariableW(L"CREO_AUTODRAW_SCALE", buffer, static_cast<DWORD>(_countof(buffer)));
    if (valueLength == 0 || valueLength >= _countof(buffer))
    {
        return PRO_DRAWING_SCALE_DEFAULT;
    }

    const double scale = _wtof(buffer);
    return scale > 0.0 ? scale : PRO_DRAWING_SCALE_DEFAULT;
}

void InitializeIdentityMatrix(ProMatrix matrix)
{
    memset(matrix, 0, sizeof(ProMatrix));
    for (int index = 0; index < 4; ++index)
    {
        matrix[index][index] = 1.0;
    }
}

bool IsSupportedModelType(ProMdlType modelType)
{
    return modelType == PRO_PART || modelType == PRO_ASSEMBLY ||
           modelType == PRO_MDL_PART || modelType == PRO_MDL_ASSEMBLY;
}

uiCmdAccessState CanCreateDrawing(uiCmdAccessMode)
{
    ProMdl currentModel = nullptr;
    ProMdlType modelType = PRO_MDL_UNUSED;

    if (ProMdlCurrentGet(&currentModel) != PRO_TK_NO_ERROR)
    {
        return ACCESS_UNAVAILABLE;
    }

    if (ProMdlTypeGet(currentModel, &modelType) != PRO_TK_NO_ERROR)
    {
        return ACCESS_UNAVAILABLE;
    }

    return IsSupportedModelType(modelType) ? ACCESS_AVAILABLE : ACCESS_UNAVAILABLE;
}

ProError CreateGbDrawingForCurrentModel()
{
    ProMdl currentModel = nullptr;
    ProMdlType modelType = PRO_MDL_UNUSED;
    ProName modelName{};

    ProError error = ProMdlCurrentGet(&currentModel);
    if (error != PRO_TK_NO_ERROR)
    {
        ShowMessage(FormatErrorMessage(L"未获取到当前模型，请先激活零件或装配。", error), MB_OK | MB_ICONERROR);
        return error;
    }

    error = ProMdlTypeGet(currentModel, &modelType);
    if (error != PRO_TK_NO_ERROR)
    {
        ShowMessage(FormatErrorMessage(L"获取当前模型类型失败。", error), MB_OK | MB_ICONERROR);
        return error;
    }

    if (!IsSupportedModelType(modelType))
    {
        ShowMessage(L"当前激活对象不是零件或装配，无法自动生成工程图。", MB_OK | MB_ICONWARNING);
        return PRO_TK_BAD_CONTEXT;
    }

    error = ProMdlNameGet(currentModel, modelName);
    if (error != PRO_TK_NO_ERROR)
    {
        ShowMessage(FormatErrorMessage(L"读取当前模型名称失败。", error), MB_OK | MB_ICONERROR);
        return error;
    }

    ProMdlnameShortdata drawingModelData{};
    CopyWideString(modelName, drawingModelData.name, _countof(drawingModelData.name));
    error = ProMdlExtensionGet(currentModel, drawingModelData.type);
    if (error != PRO_TK_NO_ERROR)
    {
        ShowMessage(FormatErrorMessage(L"读取当前模型扩展名失败。", error), MB_OK | MB_ICONERROR);
        return error;
    }

    ProMdlName drawingName{};
    CopyWideString(modelName, drawingName, _countof(drawingName));

    ProMdlName templateName{};
    const std::wstring configuredTemplateName = GetTemplateName();
    CopyWideString(configuredTemplateName.c_str(), templateName, _countof(templateName));

    ProDrawing drawing = nullptr;
    ProDwgcreateErrs creationErrors = nullptr;
    const ProDwgcreateOptions options = PRODWGCREATE_DISPLAY_DRAWING |
                                        PRODWGCREATE_SHOW_ERROR_DIALOG |
                                        PRODWGCREATE_WRITE_ERRORS_TO_FILE;

    error = ProDrawingFromTemplateCreate(drawingName, templateName, &drawingModelData, options, &drawing, &creationErrors);
    if (creationErrors != nullptr)
    {
        ProDwgcreateErrsFree(&creationErrors);
    }

    if (error != PRO_TK_NO_ERROR)
    {
        wchar_t message[768]{};
        swprintf_s(message,
            L"创建同名工程图失败。\r\n请确认模板 [%ls] 已存在，且建议使用已配置国标图框的空白模板。\r\n错误码: %d",
            configuredTemplateName.c_str(),
            static_cast<int>(error));
        ShowMessage(message, MB_OK | MB_ICONERROR);
        return error;
    }

    error = ProMdlDisplay(reinterpret_cast<ProMdl>(drawing));
    if (error != PRO_TK_NO_ERROR)
    {
        ShowMessage(FormatErrorMessage(L"工程图已创建，但切换到工程图窗口失败。", error), MB_OK | MB_ICONWARNING);
        return error;
    }

    double paperWidth = kDefaultPaperWidthInch;
    double paperHeight = kDefaultPaperHeightInch;
    ProPlotPaperSize paperSize = A3_SIZE_PLOT;
    const ProError formatError = ProDrawingFormatSizeGet(drawing, -1, &paperSize, &paperWidth, &paperHeight);
    if (formatError != PRO_TK_NO_ERROR)
    {
        paperWidth = kDefaultPaperWidthInch;
        paperHeight = kDefaultPaperHeightInch;
    }

    ProMatrix frontOrientation{};
    InitializeIdentityMatrix(frontOrientation);

    const double baseScale = GetConfiguredBaseScale();

    ProPoint3d frontPosition{ paperWidth * 0.38, paperHeight * 0.58, 0.0 };
    ProPoint3d topPosition{ frontPosition[0], frontPosition[1] - paperHeight * 0.28, 0.0 };
    ProPoint3d leftPosition{ frontPosition[0] + paperWidth * 0.26, frontPosition[1], 0.0 };

    ProView frontView = nullptr;
    ProView topView = nullptr;
    ProView leftView = nullptr;

    error = ProDrawingGeneralviewCreate(drawing,
        reinterpret_cast<ProSolid>(currentModel),
        1,
        PRO_B_FALSE,
        frontPosition,
        baseScale,
        frontOrientation,
        &frontView);
    if (error != PRO_TK_NO_ERROR)
    {
        ShowMessage(FormatErrorMessage(L"工程图已创建，但主视图创建失败。", error), MB_OK | MB_ICONERROR);
        return error;
    }

    error = ProDrawingProjectedviewCreate(drawing, frontView, PRO_B_FALSE, topPosition, &topView);
    if (error != PRO_TK_NO_ERROR)
    {
        ShowMessage(FormatErrorMessage(L"工程图已创建，但俯视图创建失败。", error), MB_OK | MB_ICONERROR);
        return error;
    }

    error = ProDrawingProjectedviewCreate(drawing, frontView, PRO_B_FALSE, leftPosition, &leftView);
    if (error != PRO_TK_NO_ERROR)
    {
        ShowMessage(FormatErrorMessage(L"工程图已创建，但左视图创建失败。", error), MB_OK | MB_ICONERROR);
        return error;
    }

    ProDwgSheetRegenerate(drawing, 1);
    ProMdlSave(reinterpret_cast<ProMdl>(drawing));

    wchar_t successMessage[768]{};
    swprintf_s(successMessage,
        L"已生成同名工程图：%ls.drw\r\n模板：%ls\r\n已按国标第一角法放置主视图、俯视图、左视图。",
        drawingName,
        configuredTemplateName.c_str());
    ShowMessage(successMessage);

    return PRO_TK_NO_ERROR;
}

int OnCreateGbDrawing(uiCmdCmdId, uiCmdValue*, void*)
{
    return static_cast<int>(CreateGbDrawingForCurrentModel());
}

ProError RegisterCommandAndMenu()
{
    ProError error = ProCmdActionAdd(const_cast<char*>(kActionName),
        OnCreateGbDrawing,
        uiCmdNoPriority,
        CanCreateDrawing,
        PRO_B_TRUE,
        PRO_B_TRUE,
        &g_actionId);
    if (error != PRO_TK_NO_ERROR && error != PRO_TK_E_FOUND)
    {
        return error;
    }

    ProFileName messageFile{};
    CopyWideString(kMessageFileName, messageFile, _countof(messageFile));

    error = ProMenubarMenuAdd(const_cast<char*>(kMenuName),
        const_cast<char*>(kMenuLabelKey),
        const_cast<char*>("Help"),
        PRO_B_FALSE,
        messageFile);
    if (error != PRO_TK_NO_ERROR && error != PRO_TK_E_FOUND)
    {
        return error;
    }

    error = ProMenubarmenuPushbuttonAdd(const_cast<char*>(kMenuName),
        const_cast<char*>(kButtonName),
        const_cast<char*>(kButtonLabelKey),
        const_cast<char*>(kButtonHelpKey),
        nullptr,
        PRO_B_FALSE,
        g_actionId,
        messageFile);
    if (error != PRO_TK_NO_ERROR && error != PRO_TK_E_FOUND)
    {
        return error;
    }

    return PRO_TK_NO_ERROR;
}
}

extern "C" __declspec(dllexport) int user_initialize()
{
    const ProError error = RegisterCommandAndMenu();
    if (error != PRO_TK_NO_ERROR)
    {
        ShowMessage(FormatErrorMessage(L"Creo 插件初始化失败。", error), MB_OK | MB_ICONERROR);
    }

    return static_cast<int>(error);
}

extern "C" __declspec(dllexport) void user_terminate()
{
}

BOOL APIENTRY DllMain(HMODULE hModule,
    DWORD ul_reason_for_call,
    LPVOID lpReserved)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}


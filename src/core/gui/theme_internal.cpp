#include "theme_manager.h"

static void theme_internal_Marine_Grey(unsigned int color[])
{
    color[ImGuiCol_Border] = 0x6D6D7F7F;
    color[ImGuiCol_BorderShadow] = 0x00000000;
    color[ImGuiCol_Button] = 0x00000066;
    color[ImGuiCol_ButtonActive] = 0x00000066;
    color[ImGuiCol_ButtonHovered] = 0x4A4A4A72;
    color[ImGuiCol_CheckMark] = 0x3D84E0FF;
    color[ImGuiCol_ChildBg] = 0xFFFFFF00;
    color[ImGuiCol_DragDropTarget] = 0x0000FFE5;
    color[ImGuiCol_FrameBg] = 0x33353889;
    color[ImGuiCol_FrameBgActive] = 0x70707089;
    color[ImGuiCol_FrameBgHovered] = 0x70707089;
    color[ImGuiCol_Header] = 0x5E5E4C4F;
    color[ImGuiCol_HeaderActive] = 0x5E5E4C4F;
    color[ImGuiCol_HeaderHovered] = 0x70707089;
    color[ImGuiCol_MenuBarBg] = 0x232323FF;
    color[ImGuiCol_ModalWindowDimBg] = 0x33333359;
    color[ImGuiCol_NavHighlight] = 0x666666FF;
    color[ImGuiCol_NavWindowingDimBg] = 0x33333333;
    color[ImGuiCol_NavWindowingHighlight] = 0x000000B2;
    color[ImGuiCol_PlotHistogram] = 0xBA9926FF;
    color[ImGuiCol_PlotHistogramHovered] = 0xFF9900FF;
    color[ImGuiCol_PlotLines] = 0xE5E9EEFF;
    color[ImGuiCol_PlotLinesHovered] = 0xFF6D59FF;
    color[ImGuiCol_PopupBg] = 0xEAEAEAEF;
    color[ImGuiCol_ResizeGrip] = 0x1616163F;
    color[ImGuiCol_ResizeGripActive] = 0x898989F2;
    color[ImGuiCol_ResizeGripHovered] = 0x303030AA;
    color[ImGuiCol_ScrollbarBg] = 0xF9F9F987;
    color[ImGuiCol_ScrollbarGrab] = 0xAFAFAFFF;
    color[ImGuiCol_ScrollbarGrabActive] = 0x7C7C7CFF;
    color[ImGuiCol_ScrollbarGrabHovered] = 0x686868FF;
    color[ImGuiCol_Separator] = 0x91917F7F;
    color[ImGuiCol_SeparatorActive] = 0x7C7C7CFF;
    color[ImGuiCol_SeparatorHovered] = 0x474747C6;
    color[ImGuiCol_SliderGrab] = 0x3D84E0FF;
    color[ImGuiCol_SliderGrabActive] = 0x9d0c05FF;
    color[ImGuiCol_Tab] = 0xD1A56BDB;
    color[ImGuiCol_TabActive] = 0xCC9651FF;
    color[ImGuiCol_TabHovered] = 0xBC6805CC;
    color[ImGuiCol_TabUnfocused] = 0xEDE4D9F7;
    color[ImGuiCol_TabUnfocusedActive] = 0xDCBC92FF;
    color[ImGuiCol_TableBorderLight] = 0xC4C4BFFF;
    color[ImGuiCol_TableBorderStrong] = 0xAFAFA5FF;
    color[ImGuiCol_TableHeaderBg] = 0xCECECCFF;
    color[ImGuiCol_TableRowBg] = 0x00000000;
    color[ImGuiCol_TableRowBgAlt] = 0x0000000F;
    color[ImGuiCol_Text] = 0x000000FF;
    color[ImGuiCol_TextDisabled] = 0x7F7F7FFF;
    color[ImGuiCol_TextSelectedBg] = 0x21212159;
    color[ImGuiCol_TitleBg] = 0xF4F4F4FF;
    color[ImGuiCol_TitleBgActive] = 0xB5B5B5FF;
    color[ImGuiCol_TitleBgCollapsed] = 0x00000082;
    color[ImGuiCol_WindowBg] = 0xB6B6B6EF;
    color[ImGuiCol_ClearColor] = 0x9AA6BAFF;
    color[ImGuiCol_WaterfallBackground] = 0x4A5973FF;
}

static void theme_internal_Deep_Blue(unsigned int color[])
{
    color[ImGuiCol_Border] = 0x6D6D7F7F;
    color[ImGuiCol_BorderShadow] = 0x00000000;
    color[ImGuiCol_Button] = 0x70707066;
    color[ImGuiCol_ButtonActive] = 0x40020066;
    color[ImGuiCol_ButtonHovered] = 0x70707072;
    color[ImGuiCol_CheckMark] = 0x3D84E0FF;
    color[ImGuiCol_ChildBg] = 0xFFFFFF00;
    color[ImGuiCol_DragDropTarget] = 0xFFFF00E5;
    color[ImGuiCol_FrameBg] = 0x4A4A4A89;
    color[ImGuiCol_FrameBgActive] = 0x40020089;
    color[ImGuiCol_FrameBgHovered] = 0x0F0F1089;
    color[ImGuiCol_Header] = 0xA0A0B24F;
    color[ImGuiCol_HeaderActive] = 0xA0A0B24F;
    color[ImGuiCol_HeaderHovered] = 0xA0A0B266;
    color[ImGuiCol_MenuBarBg] = 0x232323FF;
    color[ImGuiCol_ModalWindowDimBg] = 0x00000080;
    color[ImGuiCol_NavHighlight] = 0x999999FF;
    color[ImGuiCol_NavWindowingDimBg] = 0xCCCCCC33;
    color[ImGuiCol_NavWindowingHighlight] = 0xFFFFFFB2;
    color[ImGuiCol_PlotHistogram] = 0xBA9926FF;
    color[ImGuiCol_PlotHistogramHovered] = 0xFF9900FF;
    color[ImGuiCol_PlotLines] = 0xB1D7F2FF;
    color[ImGuiCol_PlotLinesHovered] = 0xFF6D59FF;
    color[ImGuiCol_PopupBg] = 0x141414EF;
    color[ImGuiCol_ResizeGrip] = 0xE8E8E83F;
    color[ImGuiCol_ResizeGripActive] = 0x757575F2;
    color[ImGuiCol_ResizeGripHovered] = 0xCECECEAA;
    color[ImGuiCol_ScrollbarBg] = 0x05050587;
    color[ImGuiCol_ScrollbarGrab] = 0x4F4F4FFF;
    color[ImGuiCol_ScrollbarGrabActive] = 0x828282FF;
    color[ImGuiCol_ScrollbarGrabHovered] = 0x686868FF;
    color[ImGuiCol_Separator] = 0x6D6D7F7F;
    color[ImGuiCol_SeparatorActive] = 0x828282FF;
    color[ImGuiCol_SeparatorHovered] = 0xB7B7B7C6;
    color[ImGuiCol_SliderGrab] = 0x3D84E0FF;
    color[ImGuiCol_SliderGrabActive] = 0x9d0c05FF;
    color[ImGuiCol_Tab] = 0x2D5993DB;
    color[ImGuiCol_TabActive] = 0x3268ADFF;
    color[ImGuiCol_TabHovered] = 0x4296F9CC;
    color[ImGuiCol_TabUnfocused] = 0x111A25F7;
    color[ImGuiCol_TabUnfocusedActive] = 0x22426CFF;
    color[ImGuiCol_TableBorderLight] = 0x3A3A3FFF;
    color[ImGuiCol_TableBorderStrong] = 0x4F4F59FF;
    color[ImGuiCol_TableHeaderBg] = 0x303033FF;
    color[ImGuiCol_TableRowBg] = 0x00000000;
    color[ImGuiCol_TableRowBgAlt] = 0xFFFFFF0F;
    color[ImGuiCol_Text] = 0xFFFFFFFF;
    color[ImGuiCol_TextDisabled] = 0x7F7F7FFF;
    color[ImGuiCol_TextSelectedBg] = 0xDDDDDD59;
    color[ImGuiCol_TitleBg] = 0x0A0A0AFF;
    color[ImGuiCol_TitleBgActive] = 0x494949FF;
    color[ImGuiCol_TitleBgCollapsed] = 0x00000082;
    color[ImGuiCol_WindowBg] = 0x0F0F0FEF;
    color[ImGuiCol_ClearColor] = 0x0C182EFF;
    color[ImGuiCol_WaterfallBackground] = 0x000000FF;
}

static void theme_internal_Dark(unsigned int color[])
{
    color[ImGuiCol_Border] = 0x6D6D7F7F;
    color[ImGuiCol_BorderShadow] = 0x00000000;
    color[ImGuiCol_Button] = 0x70707066;
    color[ImGuiCol_ButtonActive] = 0x70707066;
    color[ImGuiCol_ButtonHovered] = 0x70707072;
    color[ImGuiCol_CheckMark] = 0x3D84E0FF;
    color[ImGuiCol_ChildBg] = 0xFFFFFF00;
    color[ImGuiCol_DragDropTarget] = 0xFFFF00E5;
    color[ImGuiCol_FFTHoldColor] = 0xFFFF00FF;
    color[ImGuiCol_FrameBg] = 0x33353889;
    color[ImGuiCol_FrameBgActive] = 0x33353889;
    color[ImGuiCol_FrameBgHovered] = 0x33353889;
    color[ImGuiCol_Header] = 0xA0A0B24F;
    color[ImGuiCol_HeaderActive] = 0xA0A0B24F;
    color[ImGuiCol_HeaderHovered] = 0xA0A0B266;
    color[ImGuiCol_MenuBarBg] = 0x232323FF;
    color[ImGuiCol_ModalWindowDimBg] = 0x00000080;
    color[ImGuiCol_NavHighlight] = 0x999999FF;
    color[ImGuiCol_NavWindowingDimBg] = 0xCCCCCC33;
    color[ImGuiCol_NavWindowingHighlight] = 0xFFFFFFB2;
    color[ImGuiCol_PlotHistogram] = 0xBA9926FF;
    color[ImGuiCol_PlotHistogramHovered] = 0xFF9900FF;
    color[ImGuiCol_PlotLines] = 0x66E5FFFF;
    color[ImGuiCol_PlotLinesHovered] = 0xFF6D59FF;
    color[ImGuiCol_PopupBg] = 0x141414EF;
    color[ImGuiCol_ResizeGrip] = 0xE8E8E83F;
    color[ImGuiCol_ResizeGripActive] = 0x757575F2;
    color[ImGuiCol_ResizeGripHovered] = 0xCECECEAA;
    color[ImGuiCol_ScrollbarBg] = 0x05050587;
    color[ImGuiCol_ScrollbarGrab] = 0x4F4F4FFF;
    color[ImGuiCol_ScrollbarGrabActive] = 0x828282FF;
    color[ImGuiCol_ScrollbarGrabHovered] = 0x686868FF;
    color[ImGuiCol_Separator] = 0x6D6D7F7F;
    color[ImGuiCol_SeparatorActive] = 0x828282FF;
    color[ImGuiCol_SeparatorHovered] = 0xB7B7B7C6;
    color[ImGuiCol_SliderGrab] = 0x3D84E0FF;
    color[ImGuiCol_SliderGrabActive] = 0x4296F9FF;
    color[ImGuiCol_Tab] = 0x2D5993DB;
    color[ImGuiCol_TabActive] = 0x3268ADFF;
    color[ImGuiCol_TabHovered] = 0x4296F9CC;
    color[ImGuiCol_TabUnfocused] = 0x111A25F7;
    color[ImGuiCol_TabUnfocusedActive] = 0x22426CFF;
    color[ImGuiCol_TableBorderLight] = 0x3A3A3FFF;
    color[ImGuiCol_TableBorderStrong] = 0x4F4F59FF;
    color[ImGuiCol_TableHeaderBg] = 0x303033FF;
    color[ImGuiCol_TableRowBg] = 0x00000000;
    color[ImGuiCol_TableRowBgAlt] = 0xFFFFFF0F;
    color[ImGuiCol_Text] = 0xFFFFFFFF;
    color[ImGuiCol_TextDisabled] = 0x7F7F7FFF;
    color[ImGuiCol_TextSelectedBg] = 0xDDDDDD59;
    color[ImGuiCol_TitleBg] = 0x0A0A0AFF;
    color[ImGuiCol_TitleBgActive] = 0x494949FF;
    color[ImGuiCol_TitleBgCollapsed] = 0x00000082;
    color[ImGuiCol_WindowBg] = 0x0F0F0FEF;
    color[ImGuiCol_ClearColor] = 0x111111FF;
    color[ImGuiCol_WaterfallBackground] = 0x000000FF;
}

static void theme_internal_Army_Green(unsigned int color[])
{//Army Green
    color[ImGuiCol_Border] = 0x6D6D7F7F;
    color[ImGuiCol_BorderShadow] = 0x00000000;
    color[ImGuiCol_Button] = 0x70707066;
    color[ImGuiCol_ButtonActive] = 0x40020066;
    color[ImGuiCol_ButtonHovered] = 0x70707072;
    color[ImGuiCol_CheckMark] = 0x3D84E0FF;
    color[ImGuiCol_ChildBg] = 0xFFFFFF00;
    color[ImGuiCol_DragDropTarget] = 0xFFFF00E5;
    color[ImGuiCol_FrameBg] = 0x4A4A4A89;
    color[ImGuiCol_FrameBgActive] = 0x40020089;
    color[ImGuiCol_FrameBgHovered] = 0x0F0F1089;
    color[ImGuiCol_Header] = 0xA0A0B24F;
    color[ImGuiCol_HeaderActive] = 0xA0A0B24F;
    color[ImGuiCol_HeaderHovered] = 0xA0A0B266;
    color[ImGuiCol_MenuBarBg] = 0x232323FF;
    color[ImGuiCol_ModalWindowDimBg] = 0x00000080;
    color[ImGuiCol_NavHighlight] = 0x999999FF;
    color[ImGuiCol_NavWindowingDimBg] = 0xCCCCCC33;
    color[ImGuiCol_NavWindowingHighlight] = 0xFFFFFFB2;
    color[ImGuiCol_PlotHistogram] = 0xBA9926FF;
    color[ImGuiCol_PlotHistogramHovered] = 0xFF9900FF;
    color[ImGuiCol_PlotLines] = 0xB1D7F2FF;
    color[ImGuiCol_PlotLinesHovered] = 0xFF6D59FF;
    color[ImGuiCol_PopupBg] = 0x141414EF;
    color[ImGuiCol_ResizeGrip] = 0xE8E8E83F;
    color[ImGuiCol_ResizeGripActive] = 0x757575F2;
    color[ImGuiCol_ResizeGripHovered] = 0xCECECEAA;
    color[ImGuiCol_ScrollbarBg] = 0x05050587;
    color[ImGuiCol_ScrollbarGrab] = 0x4F4F4FFF;
    color[ImGuiCol_ScrollbarGrabActive] = 0x828282FF;
    color[ImGuiCol_ScrollbarGrabHovered] = 0x686868FF;
    color[ImGuiCol_Separator] = 0x6D6D7F7F;
    color[ImGuiCol_SeparatorActive] = 0x828282FF;
    color[ImGuiCol_SeparatorHovered] = 0xB7B7B7C6;
    color[ImGuiCol_SliderGrab] = 0x3D84E0FF;
    color[ImGuiCol_SliderGrabActive] = 0x9d0c05FF;
    color[ImGuiCol_Tab] = 0x2D5993DB;
    color[ImGuiCol_TabActive] = 0x3268ADFF;
    color[ImGuiCol_TabHovered] = 0x4296F9CC;
    color[ImGuiCol_TabUnfocused] = 0x111A25F7;
    color[ImGuiCol_TabUnfocusedActive] = 0x22426CFF;
    color[ImGuiCol_TableBorderLight] = 0x3A3A3FFF;
    color[ImGuiCol_TableBorderStrong] = 0x4F4F59FF;
    color[ImGuiCol_TableHeaderBg] = 0x303033FF;
    color[ImGuiCol_TableRowBg] = 0x00000000;
    color[ImGuiCol_TableRowBgAlt] = 0xFFFFFF0F;
    color[ImGuiCol_Text] = 0xFFFFFFFF;
    color[ImGuiCol_TextDisabled] = 0x7F7F7FFF;
    color[ImGuiCol_TextSelectedBg] = 0xDDDDDD59;
    color[ImGuiCol_TitleBg] = 0x0A0A0AFF;
    color[ImGuiCol_TitleBgActive] = 0x494949FF;
    color[ImGuiCol_TitleBgCollapsed] = 0x00000082;
    color[ImGuiCol_WindowBg] = 0x0F0F0FEF;
    color[ImGuiCol_ClearColor] = 0x121703FF;
    color[ImGuiCol_WaterfallBackground] = 0x000000FF;
}

static void theme_internal_Android(unsigned int color[])
{//Android Dark;
    color[ImGuiCol_Border] = 0x00000000;
    color[ImGuiCol_BorderShadow] = 0x00000000;
    color[ImGuiCol_Button] = 0x4A4458FF;
    color[ImGuiCol_ButtonActive] = 0x5D566BFF;
    color[ImGuiCol_ButtonHovered] = 0x575065FF;
    color[ImGuiCol_CheckMark] = 0xA8C7FAFF;
    color[ImGuiCol_ChildBg] = 0xFFFFFF00;
    color[ImGuiCol_DragDropTarget] = 0xA8C7FAE5;
    color[ImGuiCol_FFTHoldColor] = 0xFFFF00FF;
    color[ImGuiCol_FrameBg] = 0x36343BFF;
    color[ImGuiCol_FrameBgActive] = 0x4B4950FF;
    color[ImGuiCol_FrameBgHovered] = 0x444249FF;
    color[ImGuiCol_Header] = 0x2B2930FF;
    color[ImGuiCol_HeaderActive] = 0x413F46FF;
    color[ImGuiCol_HeaderHovered] = 0x3A383FFF;
    color[ImGuiCol_MenuBarBg] = 0x211F26FF;
    color[ImGuiCol_ModalWindowDimBg] = 0x00000066;
    color[ImGuiCol_NavHighlight] = 0xA8C7FAFF;
    color[ImGuiCol_NavWindowingDimBg] = 0xCCCCCC33;
    color[ImGuiCol_NavWindowingHighlight] = 0xFFFFFFB2;
    color[ImGuiCol_PlotHistogram] = 0xBA9926FF;
    color[ImGuiCol_PlotHistogramHovered] = 0xFF9900FF;
    color[ImGuiCol_PlotLines] = 0xA8C7FAFF;
    color[ImGuiCol_PlotLinesHovered] = 0xF2B8B5FF;
    color[ImGuiCol_PopupBg] = 0x2B2930FF;
    color[ImGuiCol_ResizeGrip] = 0x938F9933;
    color[ImGuiCol_ResizeGripActive] = 0xA8C7FA99;
    color[ImGuiCol_ResizeGripHovered] = 0x938F9966;
    color[ImGuiCol_ScrollbarBg] = 0x00000000;
    color[ImGuiCol_ScrollbarGrab] = 0x938F9980;
    color[ImGuiCol_ScrollbarGrabActive] = 0x938F99E6;
    color[ImGuiCol_ScrollbarGrabHovered] = 0x938F99B3;
    color[ImGuiCol_Separator] = 0x49454FFF;
    color[ImGuiCol_SeparatorActive] = 0xA8C7FAFF;
    color[ImGuiCol_SeparatorHovered] = 0x938F99FF;
    color[ImGuiCol_SliderGrab] = 0xA8C7FAFF;
    color[ImGuiCol_SliderGrabActive] = 0xB2CEFBFF;
    color[ImGuiCol_Tab] = 0x211F26FF;
    color[ImGuiCol_TabActive] = 0x2A4364FF;
    color[ImGuiCol_TabHovered] = 0x312E36FF;
    color[ImGuiCol_TabUnfocused] = 0x1D1B20FF;
    color[ImGuiCol_TabUnfocusedActive] = 0x23324AFF;
    color[ImGuiCol_TableBorderLight] = 0x36343BFF;
    color[ImGuiCol_TableBorderStrong] = 0x49454FFF;
    color[ImGuiCol_TableHeaderBg] = 0x211F26FF;
    color[ImGuiCol_TableRowBg] = 0x00000000;
    color[ImGuiCol_TableRowBgAlt] = 0xE6E0E90A;
    color[ImGuiCol_Text] = 0xE6E0E9FF;
    color[ImGuiCol_TextDisabled] = 0x938F99FF;
    color[ImGuiCol_TextSelectedBg] = 0xA8C7FA4D;
    color[ImGuiCol_TitleBg] = 0x141218FF;
    color[ImGuiCol_TitleBgActive] = 0x211F26FF;
    color[ImGuiCol_TitleBgCollapsed] = 0x14121880;
    color[ImGuiCol_WindowBg] = 0x141218FF;
    color[ImGuiCol_ClearColor] = 0x141218FF;
    color[ImGuiCol_WaterfallBackground] = 0x000000FF;
}

static void theme_internal_Light(unsigned int color[])
{
    color[ImGuiCol_Border] = 0x91917F7F;
    color[ImGuiCol_BorderShadow] = 0xFFFFFF00;
    color[ImGuiCol_Button] = 0x8E8E8E66;
    color[ImGuiCol_ButtonActive] = 0x8E8E8E66;
    color[ImGuiCol_ButtonHovered] = 0x8E8E8E72;
    color[ImGuiCol_CheckMark] = 0x3D84E0FF;
    color[ImGuiCol_ChildBg] = 0x00000000;
    color[ImGuiCol_DragDropTarget] = 0x0000FFE5;
    color[ImGuiCol_FFTHoldColor] = 0xC08000FF;
    color[ImGuiCol_FrameBg] = 0xACA7A389;
    color[ImGuiCol_FrameBgActive] = 0xACA7A389;
    color[ImGuiCol_FrameBgHovered] = 0xACA7A389;
    color[ImGuiCol_Header] = 0x5E5E4C4F;
    color[ImGuiCol_HeaderActive] = 0x5E5E4C4F;
    color[ImGuiCol_HeaderHovered] = 0x5E5E4C66;
    color[ImGuiCol_MenuBarBg] = 0xDBDBDBFF;
    color[ImGuiCol_ModalWindowDimBg] = 0x33333359;
    color[ImGuiCol_NavHighlight] = 0x666666FF;
    color[ImGuiCol_NavWindowingDimBg] = 0x33333333;
    color[ImGuiCol_NavWindowingHighlight] = 0x000000B2;
    color[ImGuiCol_PlotHistogram] = 0x4466D8FF;
    color[ImGuiCol_PlotHistogramHovered] = 0x0066FFFF;
    color[ImGuiCol_PlotLines] = 0x0022FFFF;
    color[ImGuiCol_PlotLinesHovered] = 0x0091A5FF;
    color[ImGuiCol_PopupBg] = 0xEAEAEAEF;
    color[ImGuiCol_ResizeGrip] = 0x1616163F;
    color[ImGuiCol_ResizeGripActive] = 0x898989F2;
    color[ImGuiCol_ResizeGripHovered] = 0x303030AA;
    color[ImGuiCol_ScrollbarBg] = 0xF9F9F987;
    color[ImGuiCol_ScrollbarGrab] = 0xAFAFAFFF;
    color[ImGuiCol_ScrollbarGrabActive] = 0x7C7C7CFF;
    color[ImGuiCol_ScrollbarGrabHovered] = 0x969696FF;
    color[ImGuiCol_Separator] = 0x91917F7F;
    color[ImGuiCol_SeparatorActive] = 0x7C7C7CFF;
    color[ImGuiCol_SeparatorHovered] = 0x474747C6;
    color[ImGuiCol_SliderGrab] = 0x3D84E0FF;
    color[ImGuiCol_SliderGrabActive] = 0x4296F9FF;
    color[ImGuiCol_Tab] = 0xD1A56BDB;
    color[ImGuiCol_TabActive] = 0xCC9651FF;
    color[ImGuiCol_TabHovered] = 0xBC6805CC;
    color[ImGuiCol_TabUnfocused] = 0xEDE4D9F7;
    color[ImGuiCol_TabUnfocusedActive] = 0xDCBC92FF;
    color[ImGuiCol_TableBorderLight] = 0xC4C4BFFF;
    color[ImGuiCol_TableBorderStrong] = 0xAFAFA5FF;
    color[ImGuiCol_TableHeaderBg] = 0xCECECCFF;
    color[ImGuiCol_TableRowBg] = 0xFFFFFF00;
    color[ImGuiCol_TableRowBgAlt] = 0x0000000F;
    color[ImGuiCol_Text] = 0x000000FF;
    color[ImGuiCol_TextDisabled] = 0x7F7F7FFF;
    color[ImGuiCol_TextSelectedBg] = 0x21212159;
    color[ImGuiCol_TitleBg] = 0xF4F4F4FF;
    color[ImGuiCol_TitleBgActive] = 0xB5B5B5FF;
    color[ImGuiCol_TitleBgCollapsed] = 0xFFFFFF82;
    color[ImGuiCol_WindowBg] = 0xEFEFEFEF;
    color[ImGuiCol_ClearColor] = 0xE6E6E6FF;
    color[ImGuiCol_WaterfallBackground] = 0xFFFFFFFF;
}

void ThemeManager::internalGetNames(std::vector<std::string>& names) const
{
    names.push_back("Android Dark");
    names.push_back("Army Green");
    names.push_back("Dark");
    names.push_back("Deep Blue");
    names.push_back("Light");
    names.push_back("Marine Grey");
}

bool ThemeManager::loadInternal(const std::string& name)
{
    unsigned int color[ImGuiCol_COUNT_Ex];
    for(int i = 0; i < ImGuiCol_COUNT_Ex; i++)
        color[i] = 0xFF000000;
    if(name == "Light")
    {
        theme_internal_Light(color);
    }else if(name == "Android Dark")
    {
        theme_internal_Android(color);
    }else if(name == "Army Green")
    {
        theme_internal_Army_Green(color);
    } else if(name == "Dark")
    {
        theme_internal_Dark(color);
    } else if(name == "Deep Blue")
    {
        theme_internal_Deep_Blue(color);
    } else if(name == "Marine Grey")
    {
        theme_internal_Marine_Grey(color);
    } else{
        return false;
    }

    ImGui::StyleColorsDark();

    auto& style = ImGui::GetStyle();

    style.WindowRounding = 0.0f;
    style.ChildRounding = 0.0f;
    style.FrameRounding = 3.0f;
    style.GrabRounding = 3.0f;
    style.PopupRounding = 5.0f;
    style.TabRounding = 3.0f;
    style.ScrollbarRounding = 9.0f;

    ImVec4* style_colors = style.Colors;

    for(int i = 0; i < ImGuiCol_COUNT_Ex; i++)
    {
        if(color[i] == 0xFF000000)
            continue;
        uint8_t* rgba = (uint8_t*)&color[i];

        ImVec4 v = ImVec4((float)rgba[3] / 255.0f, (float)rgba[2] / 255.0f, (float)rgba[1] / 255.0f, (float)rgba[0] / 255.0f);

        if(i == ImGuiCol_WaterfallBackground) {
            ThemeManager::waterfallBg = v;
            continue;
        }

        if(i == ImGuiCol_ClearColor) {
            ThemeManager::clearColor = v;
            continue;
        }

        if(i == ImGuiCol_FFTHoldColor) {
            ThemeManager::fftHoldColor = v;
            continue;
        }
        if(i < ImGuiCol_COUNT) {
            style_colors[i] = v;
        }
    }

    return true;
}

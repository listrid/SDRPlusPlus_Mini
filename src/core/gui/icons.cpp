#include <gui/icons.h>
#include <stdint.h>
#include <config.h>

#define STB_IMAGE_IMPLEMENTATION
#include <imgui/stb_image.h>
#include <filesystem>
#include <utils/flog.h>



#include "./res/png_center_tuning.h"
#include "./res/png_contrast.h"
#include "./res/png_keypad.h"
#include "./res/png_menu.h"
#include "./res/png_muted.h"
#include "./res/png_normal_tuning.h"
#include "./res/png_play.h"
#include "./res/png_sdrpp_mini.h"
#include "./res/png_stop.h"
#include "./res/png_unmuted.h"

namespace icons {
    ImTextureID LOGO;
    ImTextureID PLAY;
    ImTextureID STOP;
    ImTextureID MENU;
    ImTextureID MUTED;
    ImTextureID UNMUTED;
    ImTextureID NORMAL_TUNING;
    ImTextureID CENTER_TUNING;
    ImTextureID KEYPAD;
    ImTextureID CONTRAST;

    GLuint loadTexture(std::string path) {
        int w, h, n;
        stbi_uc* data = stbi_load(path.c_str(), &w, &h, &n, 0);
        GLuint texId;
        glGenTextures(1, &texId);
        glBindTexture(GL_TEXTURE_2D, texId);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, (uint8_t*)data);
        stbi_image_free(data);
        return texId;
    }

    GLuint loadTexture(const void* buf, size_t buf_size) {
        int w, h, n;
        stbi_uc* data = stbi_load_from_memory((const stbi_uc*) buf, (int)buf_size, &w, &h, &n, 0);
        GLuint texId;
        glGenTextures(1, &texId);
        glBindTexture(GL_TEXTURE_2D, texId);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, (uint8_t*)data);
        stbi_image_free(data);
        return texId;
    }

    bool load(std::string resDir) {

        LOGO = (ImTextureID)(uintptr_t)loadTexture(png_sdriak_Bin, png_sdriak_Size*sizeof(int));
        PLAY = (ImTextureID)(uintptr_t)loadTexture(png_play_Bin, png_play_Size*sizeof(int));
        STOP = (ImTextureID)(uintptr_t)loadTexture(png_stop_Bin, png_stop_Size*sizeof(int));
        MENU = (ImTextureID)(uintptr_t)loadTexture(png_menu_Bin, png_menu_Size*sizeof(int));
        MUTED = (ImTextureID)(uintptr_t)loadTexture(png_muted_Bin, png_muted_Size*sizeof(int));
        UNMUTED = (ImTextureID)(uintptr_t)loadTexture(png_unmuted_Bin, png_unmuted_Size*sizeof(int));
        NORMAL_TUNING = (ImTextureID)(uintptr_t)loadTexture(png_normal_tuning_Bin, png_normal_tuning_Size*sizeof(int));
        CENTER_TUNING = (ImTextureID)(uintptr_t)loadTexture(png_center_tuning_Bin, png_center_tuning_Size*sizeof(int));
        KEYPAD = (ImTextureID)(uintptr_t)loadTexture(png_keypad_Bin, png_keypad_Size*sizeof(int));
        CONTRAST = (ImTextureID)(uintptr_t)loadTexture(png_contrast_Bin, png_contrast_Size*sizeof(int));
        return true;
    }
}

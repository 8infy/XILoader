#include <iostream>
#include <string>
#include <vector>

#include <fstream>
#include <XILoader/XILoader.h>

// variables for stbi to write
// image size data to
int x, y, z;

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define PATH_TO(image) XIL_TEST_PATH image
#define AS_INT(x) static_cast<int32_t>(x)

#define CONCAT_(x, y) x##y
#define CONCAT(x, y) CONCAT_(x, y)
#define UNIQUE_VAR(x) CONCAT(x, __LINE__)

#define PRINT_TITLE(str) \
    std::cout << "================================= " \
              << str \
              << " =================================" \
              << std::endl

#define PRINT_END(str) PRINT_TITLE(str) << std::endl

#define ASSERT_LOADED(image) \
    if (!image) \
        std::cout << "Failed! --> Couldn't load the image" << std::endl

#define LOAD_AND_COMPARE_EACH(subject, path_to_image) \
    std::cout << subject "... "; \
    auto UNIQUE_VAR(xil_image) = XILoader::load(path_to_image); \
    auto UNIQUE_VAR(stbi_image) = stbi_load(path_to_image, &x, &y, &z, 0); \
    ASSERT_LOADED(UNIQUE_VAR(xil_image)); \
    compare_each(UNIQUE_VAR(xil_image).data(), UNIQUE_VAR(stbi_image), static_cast<size_t>(x)* y* z)

#define LOAD_AND_COMPARE_EACH_FLIPPED(subject, path_to_image) \
    std::cout << subject "... "; \
    auto UNIQUE_VAR(xil_image) = XILoader::load(path_to_image, true); \
    stbi_set_flip_vertically_on_load(true); \
    auto UNIQUE_VAR(stbi_image) = stbi_load(path_to_image, &x, &y, &z, 0); \
    stbi_set_flip_vertically_on_load(false); \
    ASSERT_LOADED(UNIQUE_VAR(xil_image)); \
    compare_each(UNIQUE_VAR(xil_image).data(), UNIQUE_VAR(stbi_image), static_cast<size_t>(x)* y* z)

void compare_each(uint8_t* l, uint8_t* r, size_t size)
{
    if (size == 0)
        return;

    for (size_t i = 0; i < size; i++)
    {
        if (!(l[i] == r[i]))
        {
            std::cout << "FAILED " << "At pixel[" << i << "]" << " --> ";
            std::cout << "left was == " << AS_INT(l[i]) << ", right was == " << AS_INT(r[i]) << std::endl;
            return;
        }
    }

    std::cout << "PASSED" << std::endl;
}


void TEST_BMP()
{
    PRINT_TITLE("BMP LOADING TEST STARTS");
    LOAD_AND_COMPARE_EACH("1bpp 8x8", PATH_TO("1bpp_8x8.bmp"));
    LOAD_AND_COMPARE_EACH("1bpp 9x9", PATH_TO("1bpp_9x9.bmp"));
    LOAD_AND_COMPARE_EACH("1bpp 260x401", PATH_TO("1bpp_260x401.bmp"));
    LOAD_AND_COMPARE_EACH("1bpp 1419x1001", PATH_TO("1bpp_1419x1001.bmp"));
    LOAD_AND_COMPARE_EACH("flipped 1bpp 260x401", PATH_TO("1bpp_260x401_flipped.bmp"));
    LOAD_AND_COMPARE_EACH_FLIPPED("forced flip 1bpp 260x401", PATH_TO("1bpp_260x401_flipped.bmp"));
    LOAD_AND_COMPARE_EACH("4bpp 1419x1001", PATH_TO("4bpp_1419x1001.bmp"));
    LOAD_AND_COMPARE_EACH("8bpp 1419x1001", PATH_TO("8bpp_1419x1001.bmp"));
    LOAD_AND_COMPARE_EACH("16bpp 1419x1001", PATH_TO("16bpp_1419x1001.bmp"));
    LOAD_AND_COMPARE_EACH("24bpp 1419x1001", PATH_TO("24bpp_1419x1001.bmp"));
    LOAD_AND_COMPARE_EACH("flipped 24bpp 1419x1001", PATH_TO("24bpp_1419x1001_flipped.bmp"));
    LOAD_AND_COMPARE_EACH_FLIPPED("forced flip 24bpp 1419x1001", PATH_TO("24bpp_1419x1001_flipped.bmp"));
    LOAD_AND_COMPARE_EACH("32bpp 1419x1001", PATH_TO("32bpp_1419x1001.bmp"));
    LOAD_AND_COMPARE_EACH("nomask 32bpp 1419x1001", PATH_TO("32bpp_1419x1001_nomask.bmp"));
    PRINT_END("BMP LOADING TEST DONE");

    // Cannot currently run this due to a bug in stb_image
    // https://github.com/nothings/stb/issues/870
    // LOAD_AND_COMPARE_EACH("16bpp 4x4", PATH_TO("16bpp_4x4.bmp"));
}

void TEST_PNG()
{
    PRINT_TITLE("PNG LOADING TEST STARTS");
    LOAD_AND_COMPARE_EACH("8bpc RGB 400x268", PATH_TO("8pbc_rgb_400x268.png"));
    LOAD_AND_COMPARE_EACH("8bpc RGB 1419x1001", PATH_TO("8bpc_rgb_1419x1001.png"));
    LOAD_AND_COMPARE_EACH("8bpc RGBA 4x4", PATH_TO("8bpc_rgba_4x4.png"));
    LOAD_AND_COMPARE_EACH("8bpc RGBA 1473x1854", PATH_TO("8bpc_rgba_1473x1854.png"));
    LOAD_AND_COMPARE_EACH("8bpc RGBA 2816x3088", PATH_TO("8pbc_rgba_2816x3088.png"));
    LOAD_AND_COMPARE_EACH("8bpc RGBA 2816x3088 UNCOMPRESSED", PATH_TO("8bpc_rgba_uncompressed_2816x3088.png"));
    LOAD_AND_COMPARE_EACH_FLIPPED("8bpc RGBA 2816x3088 FLIPPED", PATH_TO("8pbc_rgba_2816x3088.png"));
    LOAD_AND_COMPARE_EACH_FLIPPED("8bpc RGB 1419x1001 FLIPPED", PATH_TO("8bpc_rgb_1419x1001.png"));
    PRINT_END("PNG LOADING TEST DONE");
}

int main(int argc, char** argv)
{
    TEST_BMP();
    TEST_PNG();

    std::cin.get();
    return 0;
}

#include <stdexcept>
#include <XILoader/XILoader.h>

int main()
{
    auto image = XILoader::load("C:\\MARBLES.bmp");

    if (!image)
        throw std::runtime_error("Failed to load the image!");

    // Get the pixel at 0, 0.
    auto pixel = image[0][0];

    // Same as above.
    auto pixel1 = image.at_x(0).at_y(0);

    // Set the 'G' component of the pixel to 25.
    pixel[1] = 25;

    // Returns the number of 8-bit channels.
    // 3 for RGB, 4 for RGBA, etc
    // Can be compared with XImage::RGB/XImage::RGBA.
    auto format = image.channels();

    // Retrieve image size.
    auto width = image.width();
    auto height = image.height();

    // Retrieve pointer to image data.
    // Total size is width * height * format.
    image.data();
}

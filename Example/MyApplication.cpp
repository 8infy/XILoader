#include <assert.h>
#include <XILoader/XILoader.h>

void upload_data(unsigned char* data)
{

}

int main()
{
    auto image = XILoader::load("image.bmp");

    upload_data(image[0][0]);

    assert(image.ok());
}

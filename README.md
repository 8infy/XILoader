# XILoader
A library that handles image loading/decoding in a modern and an easy-to-use way.
## Currently supported image formats (bits-per-pixel)

| Format | 1bpp | 2 bpp| 4 bpp| 8 bpp| 16 bpp| 24 bpp| 32 bpp | Compressed |
|--------|------|------|------|------|-------|-------|------------|----|
|BMP     | :white_check_mark: | :white_check_mark: (untested) |   :white_check_mark:	  |   :white_check_mark:	  |   :white_check_mark:	   |   :white_check_mark:  |     :white_check_mark:    |     :x:    |


## Unit Testing
Currently based on stb (`stb_image.h`) as a reference for error checking, it's also a submodule in the tests directory.
If you want to run tests for yourself make sure you do a recursive clone as it downloads all the submodules as well. 

This can be achieved by running this command: `git clone https://github.com/8infy/XILoader --recursive`

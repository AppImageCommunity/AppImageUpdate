# AppImageUpdate [![Build Status](https://travis-ci.org/AppImage/AppImageUpdate.svg?branch=rewrite)](https://travis-ci.org/AppImage/AppImageUpdate)

This is a rewrite of AppImageUpdate in modern C++ (C++11, to be precise).

This is beta-level code. It works, and most features available work fine.
However, in this state, there is not much real world experience with the
application. Please report any issues on the bug tracker.


## Usage

You should be able to add this repository (with this branch) as a submodule
in your own repository. When using CMake, call `add_subdirectory()` on the
submodule path. The header directories should then be added globally. All
you need to do is link your application against `libappimageupdate`. For
example by calling `target_link_libraries(mytarget libappimageupdate)`.

Note that as mentioned previously, this project uses C++11. However, care
has been taken to make public headers work in older versions of C++ as
well. Therefore, it should not be necessary to set `CMAKE_CXX_STANDARD`,
`-std=c++11` etc. in projects using this library. If you notice that this
stops working, please open an issue.


## Licensing

For details on the licensing of the libraries AppImageUpdate uses, please
see their documentation.

The code of AppImageUpdate is licensed under the terms of the MIT license.
Please see LICENSE.txt for details.

AppImageUpdate GUI is based in part on the work of the FLTK project
(http://www.fltk.org).

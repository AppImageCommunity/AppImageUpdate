# AppImageUpdate

This is a rewrite of AppImageUpdate in modern C++ (C++11, to be precise).

This is a very early state. It currently serves as an API demo, and contains
an application demonstrating the use. Consider it a mockup, the API is
functional and simulates updates, but does not perform any checks or real
work.

**Please do not attempt to use any of the software contained in this
repository in real applications!** There's a lot of work that needs to be
done in order to implement the actual functionality!


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

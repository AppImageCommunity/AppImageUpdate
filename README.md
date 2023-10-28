# AppImageUpdate [![Build Status](https://travis-ci.org/AppImage/AppImageUpdate.svg?branch=rewrite)](https://travis-ci.org/AppImage/AppImageUpdate)

AppImageUpdate lets you update AppImages in a decentral way using information embedded in the AppImage itself. No central repository is involved. This enables upstream application projects to release AppImages that can be updated easily. Since AppImageKit uses delta updates, the downloads are very small and efficient.

This is an implementation of AppImageUpdate in modern C++ (C++11, to be precise).

This is beta-level code. It works, and most features available work fine.
However, in this state, there is not much real world experience with the
application. Please report any issues on the bug tracker.

## Try it out

* Download an AppImage that contains update information (not all AppImages do).
* Wait a couple of hours/days until they have a new continuous build (happens frequently).
* Download AppImageUpdate from [here](https://github.com/AppImage/AppImageUpdate/releases) and [make it executable](https://discourse.appimage.org/t/how-to-make-an-appimage-executable/80).
* Run AppImageUpdate and select your "old" Subsurface AppImage.
* Only the parts that have changed since the original version are downloaded.

Notice how quick the update was. Combined with fully automated continuous or nightly builds, this should make software "fluid", as users can get the latest development versions very rapidly.

If you have the optional appimaged daemon installed, then it can use AppImageUpdate enable right-click updates in the launcher:

![screenshot from 2016-10-15 16-37-05](https://cloud.githubusercontent.com/assets/2480569/19410850/0390fe9c-92f6-11e6-9882-3ca6d360a190.jpg)

## Components

* `AppImageUpdate`: GUI application to update AppImages with
* `appimageupdatetool`: Command line tool to update AppImages with (does the same as the above)
* `validate`: A tool to check signed AppImages (AppImageUpdate has this built in)

## Motivation

### Use cases

Here are some concrete use cases for AppImageUpdate:

 * "As a user, I want to get updates for my AppImages easily, without the need to add repositories to my system like EPEL or ppa. I want to be sure nothing is touched on my system apart from the one application I want to update. And I want to keep the old version until I know for sure that the new version works for me. Of course, I want to check with GPG signatures where the update is coming from.
 * As an application developer, I want to push out nightly or continuous builds to testers easily without having to go though complicated repository setups for multiple distributions. Since I want to support as many distributions as possible, I am looking for something simple and distribution independent.
 * As a server operator, I want to reduce my download traffic by having delta updates. So that users don't have to download the same Qt libs over and over again with each application release, even though they are bundled with the application.

### The problem space

With AppImages allowing upstream software developers to publish applications directly to end users, we need a way to easily update these applications. While we could accomplish this by putting AppImages into traditional `deb` and `rpm` packages and setting up a repository for these, this would have several disadvantages:

 * Setting up multiple repositories for multiple distributions is cumbersome (although some platforms like the openSUSE Build Service make this easier)
 * As the package formats supported by package managers are based on archive formats, the AppImages would have to be archived which means they would have to be extracted/installed - a step that takes additional time
 * Delta updates would be complicated to impossible which means a lot of bandwidth would be wasted (depending on what you consider "a lot")
 * Users would need to configure repositories in their systems (which is cumbersome) and would need root rights to update AppImages

AppImageUpdate to the rescue.

### Objectives

AppImageUpdate has been created with specific objectives in mind.

 1. __Be Simple__. Like using AppImages, updating them should be really easy. Also updates should be easy to understand, create, and manage.
 2. __Be decentral__. Do not rely on central repositories or distributions. While you _can_ use repositories like Bintray with AppImageUpdate, this is purely optional. As long as your webserver supports range requests, you can host your updates entirely yourself.
 3. __Be Fast__. Increase velocity by making software updates really fast. This means using delta updates, and allow leveraging existing content delivery networks (CDNs).
 4. __Be extensible__. Allow for other transport and distribution mechanisms (like peer-to-peer) in the future.
 5. __Inherit the intensions of the AppImage format__.

## Update information overview

In order for AppImageUpdate to do its magic, __Update information__ must be embedded inside the AppImage. Update information is what tells AppImageUpdate vital data such as:
 * How can I find out the latest version (e.g., the URL that tells me the latest version)?
 * How can I find out the delta (the portions of the applications that have changed) between my local version and the latest version?
 * How can I download the delta between my local version and the latest version (e.g., the URL of the download server)?

While all of this information could simply be put inside the AppImage, this could be a bit inconvenient since that would mean changing the download server location would require the AppImage to be re-created. Hence, this information is not put into the file system inside the AppImage, but rather embedded into the AppImage in a way that makes it very easy to change this information should it be required, e.g., if you put the files onto a different download server. As you will probably know, an AppImage is both an ISO 9660 file (that you can mount and examine) and an ELF executable (that you can execute). We are using the ISO 9660 Volume Descriptor #1 field "Application Used" to store this information.

## Projects using AppImageUpdate

* [Open Build Service](http://openbuildservice.org/) automatically injects update information into every AppImage [built there](https://build.opensuse.org/)
* [linuxdeployqt](https://github.com/probonopd/linuxdeployqt), a tool that makes it easy to build AppImages, automatically injects update information into every AppImage built on [Travis CI](https://travis-ci.org/)

## Building

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

The sample AppImageUpdate GUI is based in part on the work of the FLTK project
(http://www.fltk.org). We are interested in adding Qt and Gtk+ versions.

## TODO

* On this page, describe or link to a description of how to add update information and how to upload the files, similar to the old version located at https://github.com/AppImage/AppImageUpdate/blob/master/README.md

## Contact

If you have questions, the developers are on `#AppImage` on `irc.libera.chat`.

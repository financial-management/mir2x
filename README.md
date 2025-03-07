# mir2x

<a href="https://scan.coverity.com/projects/etorth-mir2x">
  <img alt="Coverity Scan Build Status"
       src="https://scan.coverity.com/projects/9270/badge.svg"/>
</a>
<a href="https://ci.appveyor.com/project/etorth/mir2x">
  <img alt="Appveyor Build Status"
       src="https://ci.appveyor.com/api/projects/status/github/etorth/mir2x?svg=true"/>
</a>
<a href="https://travis-ci.org/github/etorth/mir2x">
  <img alt="Travis CI Build Status"
       src="https://travis-ci.org/etorth/mir2x.svg?branch=master"/>
</a>
<a href="https://gitter.im/mir2x/community?utm_source=share-link&utm_medium=link&utm_campaign=share-link">
  <img alt="Gitter chat"
       src="https://badges.gitter.im/org.png"/>
</a>

With C++20 coroutine feature support, you need a compiler supports c++20 to compile.

mir2x is a c/s based mir2ei implementation with various platforms supported. It contains all need components for game players and developers:

  - client
  - monoserver
  - pkgviewer
  - animaker
  - mapeditor

YouTube links: [1](https://youtu.be/3Xne8UHlOl0) [2](https://youtu.be/jl1LPxe2EAA) [3](https://youtu.be/TtGONA83Mb8)

<https://user-images.githubusercontent.com/1754214/148706980-5d0b5d53-10cf-41f2-b9bb-0ba2b2e301fe.mp4>

<img src="https://github.com/etorth/mir2x/raw/master/readme/screencapture6.gif" width="800" height="600"/>
<img src="https://github.com/etorth/mir2x/raw/master/readme/mapeditor.png" width="800" height="520"/>


### Windows

For windows please download binaries from appveyor
```
https://ci.appveyor.com/project/etorth/mir2x/build/artifacts
```
If complains missing dll, you may need to copy .dll files from mir2x/bin to mir2x/client and mir2x/server.

### Building from source

mir2x requires [cmake](https://cmake.org/) v3.12 and [gcc](https://gcc.gnu.org/) support c++20 to run. Mir2x needs some pre-installed packages before compile:

```sh
libsdl2-dev
libsdl2-image-dev
libsdl2-mixer-dev
libsdl2-ttf-dev
libsdl2-gfx-dev
libpng-dev
liblua5.3-dev
libfltk1.3-dev
```

Cmake complains if libs are missing. After install all these dependencies, clone and compile the repo. By default cmake tries to install in /usr/local. use ``CMAKE_INSTALL_PREFIX" to customize.

```sh
$ git clone https://github.com/etorth/mir2x.git
$ cd mir2x
$ mkdir b
$ cd b
$ cmake .. -DCMAKE_INSTALL_PREFIX=install
$ make
$ make install
```
### First time run
The above steps install binaries in mir2x/b/install, to start the monoserver, click menu server/launch to start the service before start client:

```sh
$ cd mir2x/b/install/server
$ ./monoserver
```

Start client, currently you can use default account (id = test, pwd = 123456) to try it:

```sh
$ cd mir2x/b/install/client
$ ./client
```

### Code style

Global variables:

1. no globals of builtin types, they are lacking of multithread access control.
2. no globals of class instances, use pointer instead, for better construction/destruction control.
3. all member functions of globals should be:
    - simple
    - thread-safe
    - atomic operations
4. name all global pointers as g_XXXX and use them by extern, and
    - allocate them at beginning of main()
    - remain valid during the whole run, and ONLY free them at process exit.

Error handling:
1. use exception for good/bad path control, force catch at exit of main() or clone().
2. do strict parameters checking before doing actual logic, no assumptions.
3. let the crash happen ASAP if any fatal error detected

General rules:
1. make all member functions self-contained, avoid first/second half logic.
2. don't do optimization too early, perfer simple/clear logic.

### Packages

mir2x uses a number of open source projects to work properly, and of course itself is open source with a public repository on github, please remind me if I missed anything.

* [SDL2](https://www.libsdl.org/) - A cross-platform development library designed to provide a hardware abstraction layer.
* [FLTK](http://www.fltk.org) - A cross-platform C++ GUI toolkit for UNIX®/Linux® (X11), Microsoft® Windows®, and MacOS® X.
* [asio](http://www.think-async.com/) - A cross-platform C++ library for network and low-level I/O programming.
* [g3log](https://github.com/KjellKod/g3log) - An asynchronous, "crash safe", logger that is easy to use.
* [lua](https://www.lua.org/) - A powerful, efficient, lightweight, embeddable scripting language.
* [sol2](https://github.com/ThePhD/sol2) - A fast, simple C++ and Lua binding.
* [tinyxml2](http://www.grinninglizard.com/tinyxml2/) - A simple, small, efficient, C++ XML parser.
* [utf8-cpp](http://utfcpp.sourceforge.net/) - A simple, portable and lightweigt C++ library for UTF-8 string handling.
* [libpng](http://www.libpng.org/pub/png/libpng.html) - The official PNG reference library.
* [ThreadPool](https://github.com/progschj/ThreadPool) - A simple C++11 Thread Pool implementation.
* [astar-algorithm](https://github.com/justinhj/astar-algorithm-cpp) - Implementation of the A* algorithm in C++ and C#.
* [SQLiteCpp](https://github.com/SRombauts/SQLiteCpp) - SQLiteC++ (SQLiteCpp) is a smart and easy to use C++ SQLite3 wrapper.

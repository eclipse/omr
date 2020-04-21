# Building Compiler on RISC-V

## Native build

*Note:* Following steps have been tested on Debian system creates using
[debian-riscv][1]. The image built using these scripts should contain everything
needed to build OMR on RISC-V. It should be possible to build OMR on RISC-V
using Fedora images but this has not been tried.

*Note:* You will need `riscv.h` and `riscv-opc.h` headers from GNU binutils. You
can get them using (the image created using [debian-riscv][1] contains them):

    sudo wget "-O/usr/local/include/riscv.h" 'https://sourceware.org/git/gitweb.cgi?p=binutils-gdb.git;a=blob_plain;f=include/opcode/riscv.h;hb=HEAD'
    sudo wget "-O/usr/local/include/riscv-opc.h" 'https://sourceware.org/git/gitweb.cgi?p=binutils-gdb.git;a=blob_plain;f=include/opcode/riscv-opc.h;hb=HEAD'

### Building

```
git clone https://github.com/eclipse/omr
cd omr
mkdir build
cd build
cmake .. \
      -Wdev -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
      -DOMR_COMPILER=ON -DOMR_TEST_COMPILER=ON \
      -DOMR_JITBUILDER=ON -DOMR_JITBUILDER_TEST=ON
make -j4
```

Notes:

 * You *must* return `cmake ...` when you add/remove/rename files (generally, when you change any `CMakeLists.txt`).
 * `-DCMAKE_BUILD_TYPE=Debug` is necessary to produce debug symbols
 * `-DCMAKE_EXPORT_COMPILE_COMMANDS=ON` is to produce a `compile_commands.json` used by Clang-based tools. Should not harm.

### Compiler tests

To see what tests are in minimal testsuite run (assume you're in `build` directory)

    cd fvtest/compilertest
    ./compilertest --gtest_list_tests --gtest_filter=MinimalTest*

 To run specific test, say `MeaningOfLife`:

    ./compilertest --gtest_filter=MinimalTest.MeaningOfLife

 To run all tests in minimal test suite:

    ./compilertest --gtest_filter=MinimalTest.*

### TRIL tests

To run selected TRIL tests:

    cd fvtest/compilertriltest
    ./comptest --gtest_filter=*Arithmetic*/Int32*:LogicalTest/Int32*Unary*

To run *ALL* TRIL tests:

    cd fvtest/compilertriltest
    TR_Options=softFailOnAssume ./comptest

*Note*: At the time of writing, some opcodes used in TRIL tests are still unimplemented, causing `TR_ASSERT()` failure. `TR_Options=songftFailOnAssume` causes this (and all others) assertion to just abort the compilation. This way, hitting such and unimplemented opcode does not terminate whole test run.



## Cross compiling

*Note:* Following steps are for (Debian 10 "buster")[2]. It should work on Debian
Testing ("bullseye"). It may work on other debian-based distributions, perhaps
with some modification. However, it has been tested only on Debian 10 "buster".

### Preparing the cross-compilation environment

 1. Install RISCV64 toolchain:

        sudo apt install gcc-riscv64-linux-gnu g++-riscv64-linux-gnu libgcc1-riscv64-cross libc6-dev-riscv64-cross libstdc++-8-dev-riscv64-cross linux-libc-dev-riscv64-cross

 2. Download required `riscv.h` and `riscv-opc.h` from GNU binutils:

        sudo wget "-O/usr/riscv64-linux-gnu/include/riscv.h" 'https://sourceware.org/git/gitweb.cgi?p=binutils-gdb.git;a=blob_plain;f=include/opcode/riscv.h;hb=HEAD'
        sudo wget "-O/usr/riscv64-linux-gnu/include/riscv-opc.h" 'https://sourceware.org/git/gitweb.cgi?p=binutils-gdb.git;a=blob_plain;f=include/opcode/riscv-opc.h;hb=HEAD'

 3. Create a "root filesystem" containing all required libraries and tools to link and run OMR. Hhere we build it into `~/riscv-debian/rootfs`, modify according your taste):

        cd ~
        git clone https://github.com/janvrany/riscv-debian.git
        cd riscv-debian
        mkdir rootfs
        ./debian-mk-rootfs.sh

   **!!!BIG FAT WARNING!!!**
   Script `./debian-mk-rootfs.sh` uses `sudo` a lot. IF THERE"S A BUG, IT MAY WIPE OUT YOUR SYSTEM. DO NOT RUN THIS SCRIPTS WITHOUT READING IT CAREFULLY FIRST.

 4. Build latest QEMU for RISC-V. This is needed to run cross-compiled tests. Hhere we build it into `~/qemu`, modify according your taste):

        git clone https://git.qemu.org/git/qemu.git
        ./configure --target-list=riscv64-linux-user
        make -j4

    Once build finishes, QEMU executable is at `./riscv64-linux-user/qemu-riscv64`

    *Note:* when running cross-compiled binaries, use this QEMU, not QEMU shipped with
    Debian is it hangs up (at the time of writing). The reason is unknown.


### Building

First, clone the repository:

    git clone https://github.com/shingarov/omr -b riscv
    cd omr

Then build native version of OMR. This is required to have native versions of
various custom tools:

    mkdir build_native
    cd build_native
    cmake ..
    make -j4

Finally, cross-compile for RISC-V

    mkdir build_riscv64
    cd build_riscv64
    cmake .. \
      -Wdev -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
      -DOMR_COMPILER=ON -DOMR_TEST_COMPILER=ON \
      -DOMR_JITBUILDER=ON -DOMR_JITBUILDER_TEST=ON \
      -DCMAKE_FIND_ROOT_PATH=/home/jv/Projects/riscv-debian/debian \
      -DCMAKE_TOOLCHAIN_FILE=../cmake/toolchains/riscv64-linux-cross.cmake \
      -DOMR_TOOLS_IMPORTFILE=../build_native/tools/ImportTools.cmake
    make -j4

*Note*: you should point `-DCMAKE_FIND_ROOT_PATH=/home/jv/Projects/riscv-debian/rootfs` to the directory where you have Debian rootfs (see section *Preparing the cross-compilation environment*, step 3).

### Compiler tests

To see what tests are in minimal testsuite run:

    cd fvtest/compilertest
    ~/qemu/riscv64-linux-user/qemu-riscv64 -L ~/riscv-debian/rootfs ./compilertest --gtest_list_tests --gtest_filter=MinimalTest*

The above command assumes that:

 * you're in `build_riscv64` directory,
 * QEMU is at `~/qemu/riscv64-linux-user/qemu-riscv64` (see section *Preparing the cross-compilation environment*, step 4).
 * Debian root fs is at `/home/jv/Projects/riscv-debian/rootfs` (see section *Preparing the cross-compilation environment*, step 3).

 To run specific test, say `MeaningOfLife`:

    ~/qemu/riscv64-linux-user/qemu-riscv64 -L ~/riscv-debian/rootfs ./compilertest --gtest_filter=MinimalTest.MeaningOfLife

 To run all tests in minimal test suite:

    ~/qemu/riscv64-linux-user/qemu-riscv64 -L ~/riscv-debian/rootfs ./compilertest --gtest_filter=MinimalTest.*

### TRIL tests

To run selected TRIL tests:

    cd fvtest/compilertriltest
    ~/qemu/riscv64-linux-user/qemu-riscv64 -L ~/riscv-debian/rootfs ./comptest --gtest_filter=*Arithmetic*/Int32*:LogicalTest/Int32*Unary*

[1]: https://github.com/janvrany/riscv-debian
[2]: https://www.debian.org/News/2019/20190706
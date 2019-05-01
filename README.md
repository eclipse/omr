<!--
Copyright (c) 2016, 2018 IBM Corp. and others

This program and the accompanying materials are made available under
the terms of the Eclipse Public License 2.0 which accompanies this
distribution and is available at https://www.eclipse.org/legal/epl-2.0/
or the Apache License, Version 2.0 which accompanies this distribution and
is available at https://www.apache.org/licenses/LICENSE-2.0.

This Source Code may also be made available under the following
Secondary Licenses when the conditions for such availability set
forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
General Public License, version 2 with the GNU Classpath
Exception [1] and GNU General Public License, version 2 with the
OpenJDK Assembly Exception [2].

[1] https://www.gnu.org/software/classpath/license.html
[2] http://openjdk.java.net/legal/assembly-exception.html

SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
-->

# Eclipse OMR

| Build | Status |
| ---------------------- | -------------------- |
| **Linux x86-64 Lint (Travis)** | [![Travis Overall Status](https://api.travis-ci.org/eclipse/omr.svg?branch=master)](https://travis-ci.org/eclipse/omr) |
| **Windows x86 (Appveyor)** | [![Appveyor Status](https://ci.appveyor.com/api/projects/status/github/eclipse/omr?svg=true&branch=master)](https://ci.appveyor.com/project/eclipsewebmaster/omr) |
| **Windows x86-64** | [![Windows x86-64 Status](https://ci.eclipse.org/omr/job/Build-win_x86-64/badge/icon)](https://ci.eclipse.org/omr/job/Build-win_x86-64/) |
| **Linux x86** | [![Build Status](https://ci.eclipse.org/omr/job/Build-linux_x86/badge/icon)](https://ci.eclipse.org/omr/job/Build-linux_x86/) |
| **Linux x86-64** | [![Linux x86-64 Status](https://ci.eclipse.org/omr/job/Build-linux_x86-64/badge/icon)](https://ci.eclipse.org/omr/job/Build-linux_x86-64/) |
| **Linux x86-64 Compressed Pointers** | [![Build Status](https://ci.eclipse.org/omr/job/Build-linux_x86-64_cmprssptrs/badge/icon)](https://ci.eclipse.org/omr/job/Build-linux_x86-64_cmprssptrs/) |
| **Linux AArch64 (ARM 64-bit)** | [![Build-linux_aarch64 Status](https://ci.eclipse.org/omr/job/Build-linux_aarch64/badge/icon)](https://ci.eclipse.org/omr/job/Build-linux_aarch64/) |
| **Linux ARM 32-bit** | [![Build-linux_arm Status](https://ci.eclipse.org/omr/job/Build-linux_arm/badge/icon)](https://ci.eclipse.org/omr/job/Build-linux_arm/) |
| **OSX x86-64** | [![Build Status](https://ci.eclipse.org/omr/job/Build-osx_x86-64/badge/icon)](https://ci.eclipse.org/omr/job/Build-osx_x86-64/) |
| **Linux Power 64-bit** | [![Build-linux_ppc-64_le_gcc Status](https://ci.eclipse.org/omr/job/Build-linux_ppc-64_le_gcc/badge/icon)](https://ci.eclipse.org/omr/job/Build-linux_ppc-64_le_gcc/) |
| **AIX Power 64-bit** | [![Build-aix_ppc-64 Status](https://ci.eclipse.org/omr/job/Build-aix_ppc-64/badge/icon)](https://ci.eclipse.org/omr/job/Build-aix_ppc-64/) |
| **Linux Z (s390x) 64-bit** | [![Build-linux_390-64 Status](https://ci.eclipse.org/omr/job/Build-linux_390-64/badge/icon)](https://ci.eclipse.org/omr/job/Build-linux_390-64/) |


What Is Eclipse OMR?
====================

The Eclipse OMR project is a set of open source C and C++ components that can
be used to build robust language runtimes that support many different hardware
and operating system platforms.

Our current components are:

* **`gc`**:             Garbage collection framework for managed heaps
* **`compiler`**:       Components for building compiler technology, such as JIT
                        compilers.
* **`jitbuilder`**:     An easy to use high level abstraction on top of the
                        compiler technology.
* **`port`**:           Platform porting library
* **`thread`**:         A cross platform pthread-like threading library
* **`util`**:           general utilities useful for building cross platform
                        runtimes
* **`omrsigcompat`**:   Signal handling compatibility library
* **`omrtrace`**:       Tracing library for communication with IBM Health Center
                        monitoring tools
* **`tool`**:           Code generation tools for the build system
* **`vm`**:             APIs to manage per-interpreter and per-thread contexts
* **`example`**:        Demonstration code to show how a language runtime might
                        consume some Eclipse OMR components
* **`fvtest`**:         A language-independent test framework so that Eclipse
                        OMR components can be tested outside of a language runtime

What's the goal?
================

The long term goal for the Eclipse OMR project is to foster an open ecosystem of
language runtime developers to collaborate and collectively innovate with
hardware platform designers, operating system developers, as well as tool and
framework developers and to provide a robust runtime technology platform so that
language implementers can much more quickly and easily create more fully
featured languages to enrich the options available to programmers.

It is our community's fervent goal to be one of active contribution, improvement,
and continual consumption.

We will be operating under the [Eclipse Code of Conduct][coc]
to promote fairness, openness, and inclusion.

[coc]: https://eclipse.org/org/documents/Community_Code_of_Conduct.php

Who is using Eclipse OMR?
=========================

* The most comprehensive consumer of the Eclipse OMR technology is the [Eclipse
  OpenJ9 Virtual Machine](https://github.com/eclipse/openj9): a high
  performance, scalable, enterprise class Java Virtual Machine implementation
  representing hundreds of person years of effort, built on top of the core
  technologies provided by Eclipse OMR.
* The Ruby+OMR Technology Preview has used Eclipse OMR components to add a JIT
  compiler to the CRuby implementation, and to experiment with replacing the
  garbage collector in CRuby.
* A SOM++ Smalltalk runtime has also been modified to use Eclipse OMR
  componentry.
* An experimental version of CPython using Eclipse OMR components
  has also been created but is not yet available in the open. (Our focus
  has been dominated by getting this code out into the open!)


What's the licence?
===================
[![License](https://img.shields.io/badge/License-EPL%202.0-green.svg)](https://opensource.org/licenses/EPL-2.0)
[![License](https://img.shields.io/badge/License-APL%202.0-green.svg)](https://opensource.org/licenses/Apache-2.0)

All Eclipse OMR project materials are made available under the Eclipse Public
License 2.0 and the Apache 2.0 license. You can choose which license you wish
to follow.  Please see our LICENSE file for more details.


What's being worked on?
======================

There are some active contribution projects underway right now:

* Documentation: code comments are great, but we need more overview documentation
  so we're writing that
* FAQ: Frequently Asked Questions from real people's questions (request: ask
  questions!)
* [Beginner issues][beg]: relatively simple but useful work items meant for
  people new to the project.
* `diag`: more diagnostic support for language runtimes to aid developers and users
* `hcagent`: the core code for the IBM Health Center agent for interfacing to a runtime
* `gc`: adding generational and other GC policies

[beg]: https://github.com/eclipse/omr/issues?q=is%3Aopen+is%3Aissue+label%3Abeginner


How Do I Interact With the Community?
=====================================

* Join the Eclipse OMR developer community [mailing list](https://accounts.eclipse.org/mailing-list/omr-dev).
The community typically uses this list for project announcements and administrative
discussions amongst committers.  Questions are welcome here as well.

* Join the Eclipse OMR community [Slack workspace](https://join.slack.com/t/eclipse-omr/shared_invite/enQtMzg2ODIwODc4MTAyLTk4ZjJjNTZlZmMyMGRmYTczOTkzMGJiNTQ4NTA3YTA1NGU4MmJjNWI4NTBjOGNkNmNjMWQ3MmFmYjA4OGZjZjM).  You can join channels
that interest you, ask questions, and receive answers from subject matter experts.

* Ask a question or start a discussion via a [GitHub issue](https://github.com/eclipse/omr/issues).


How Do I Use it?
================

## How to Build Standalone Eclipse OMR

The best way to get an initial understanding of the Eclipse OMR technology is to
look at a 'standalone' build, which hooks Eclipse OMR up to the its testing system
only.

### Basic configuration and compile
To build standalone Eclipse OMR, run the following commands from the root of the
source tree. For more detailed instructions please read [BuildingWithCMake.md](doc/BuildingWithCMake.md).

    # Create a build directory and cd into it
    mkdir build
    cd build
    
    #generate the build system using cmake
    cmake ..

    # Build (you can optionally compile in parallel by adding -j<N> to the make command)
    make

    # Run tests (note that no contribution should cause new test failures in testing).
    # Use the `-V` option to see verbose output from the tests.
    ctest [-V]

### Building Eclipse OMR on Windows using Visual Studio
The following instructions below demonstrate the steps to build Eclipse OMR on Windows
using Visual Studios. In the example Visual Studio 11 2012 Win64 is being used.
You can easily switch this to the version of Visual Studio you would like to use.

    # Create a build directory and cd into it
    mkdir build
    cd build
    
    #generate the build system using cmake
    cmake -G "Visual Studio 11 2012 Win64" ..

    # Build
    cmake --build .

    # Run tests (note that no contribution should cause new test failures in "make test")
    ctest


Where can I learn more?
===============================

Presentations about Eclipse OMR
-------------------------------

* Mark Stoodley's talk at the JVM Languages Summit in August, 2015:
  [A VM is a VM is a VM: The Secret Path to High Performance Multi-Language Runtimes](https://www.youtube.com/watch?v=kOnyJurioyw)

* Daryl Maier's slides from Java One in October, 2015:
  [Beyond the Coffee Cup: Leveraging Java Runtime Technologies for the Polyglot](http://www.slideshare.net/0xdaryl/javaone-2015-con7547-beyond-the-coffee-cup-leveraging-java-runtime-technologies-for-polyglot?related=1)

* Charlie Gracie's slides from Java One in October, 2015:
  [What's in an Object? Java Garbage Collection for the Polyglot](http://www.slideshare.net/charliegracie1/javaone-whats-in-an-object)

* Angela Lin, Robert Young, Craig Lehmann and Xiaoli Liang CASCON Workshop in November, 2015
  [Building Your Own Runtime](https://ibm.box.com/s/7xdg25we2ezmdjjbqdys30d7dl1iyo49)
  * Note: these slides have been modified since the original presentation to use the Eclipse OMR project name

* Charlie Gracie's talk from FOSDEM in February, 2016:
  [Ruby and OMR: Experiments in utilizing OMR technologies in MRI](http://bofh.nikhef.nl/events/FOSDEM/2016/h2213/ruby-and-omr.mp4)

* Charlie Gracie's slides from jFokus in February, 2016
  [A JVMs Journey into Polyglot Runtimes](https://t.co/efCKf6aCB4)

* Mark Stoodley's slides from EclipseCON in March, 2016
  [Eclipse OMR: a modern toolkit for building language runtimes](http://www.slideshare.net/MarkStoodley/omr-a-modern-toolkit-for-building-language-runtimes)

Blog Posts about OMR technologies
---------------------------------

* [Introducing Eclipse OMR: Building Language Runtimes](https://developer.ibm.com/open/2016/03/08/introducing-omr-building-language-runtimes/)
* [JitBuilder Library and Eclipse OMR: Just-In-Time Compilers made easy](https://developer.ibm.com/open/2016/07/19/jitbuilder-library-and-eclipse-omr-just-in-time-compilers-made-easy/)

(c) Copyright IBM Corp. 2016, 2018

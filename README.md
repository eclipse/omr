bLAH BLAH


[![Build Status](https://api.travis-ci.org/eclipse/omr.svg?branch=master)](https://travis-ci.org/eclipse/omr)
What Is Eclipse OMR?
====================

The Eclipse OMR project is a set of open source C and C++ components that can
be used to build robust language runtimes that support many different hardware
and operating system platforms.

Our current components are:

* **`gc`**:             Garbage collection framework for managed heaps
* **`compiler`**:       Components for builing compiler technology, such as JIT
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

* The most comprehensive consumer of the Eclipse OMR technology is the IBM J9
  Virtual Machine: a high performance, scalable, enterprise class Java Virtual
  Machine implementation representing hundreds of person years of effort, built
  using the core implementations provided by Eclipse OMR. IBM is working actively
  to open source J9.
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

All Eclipse OMR project materials are made available under the Eclipse Public
License V1.0 and the Apache 2.0 license. You can choose which license you wish
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

How Do I Use it?
================

## How to Build Standalone Eclipse OMR

The best way to get an initial understanding of the Eclipse OMR technology is to
look at a 'standalone' build, which hooks Eclipse OMR up to the its testing system
only. 

### Basic configuration and compile
To build standalone Eclipse OMR, run the following commands from the top of the
source tree. The top of the Eclipse OMR source tree is the directory that contains
run_configure.mk.

    # Generate autotools makefiles with SPEC-specific presets
    make -f run_configure.mk SPEC=linux_x86-64 OMRGLUE=./example/glue

    # Build
    make

    # Run tests (note that no contribution should cause new test failures in "make test")
    make test

Run `make -f run_configure.mk help` for a list of configure makefile targets.
Run `make help` for a list of build targets.

### Building Eclipse OMR on Windows
A shell script interpreter, such as bash, is required to run configure.

### How to Configure with Custom Options
Run `./configure --help` to see the full list of configure command-line
options.

To run configure using both `SPEC` presets and custom options, pass the
`EXTRA_CONFIGURE_ARGS` option to `run_configure.mk`.

For example, to disable optimizations, run configure like this:

    # Example configure
    make -f run_configure.mk SPEC=linux_x86-64 OMRGLUE=./example/glue 'EXTRA_CONFIGURE_ARGS=--disable-optimized' clean all

To disable building fvtests, run configure like this:

    # Example configure disabling fvtests
    make -f run_configure.mk SPEC=linux_x86-64 OMRGLUE=./example/glue 'EXTRA_CONFIGURE_ARGS=--disable-fvtest' clean all

Note that the `clean` target of `run_configure.mk` deletes the header files and
makefiles generated by configure. Invoking the `clean all` targets ensures that
the header files and makefiles are regenerated using the custom options.

The minimal invocation of configure is:

    # Basic configure example
    $ ./configure OMRGLUE=./example/glue

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


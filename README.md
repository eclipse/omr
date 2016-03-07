# Welcome to the OMR Project!

The OMR project consists of a highly integrated set of open source C and C++ 
components that can be used to build robust language runtimes that will support 
many different hardware and operating system platforms. These components include
but are not limited to: memory management, threading, platform port (abstraction)
library, diagnostic support, monitoring support, garbage collection, and native 
Just In Time compilation.


The long term goal for the OMR project is to foster an open ecosystem of 
language runtime developers to collaborate and collectively innovate with
hardware platform designers, operating system developers, as well as tool and
framework developers and to provide a robust runtime technology platform so that
language implementers can much more quickly and easily create more fully
featured languages to enrich the options available to programmers.


All OMR project materials are made available under the Eclipse Public License V1.0
and the Apache 2.0 license. You can choose which license you wish to follow.
Please see our LICENSE file for more details.


The initial set of components in the OMR project include:
* port: platform porting library
* thread: a cross platform pthread-like threading library
* util: general utilities useful for building cross platform runtimes
* omrsigcompat: signal handling compatibility library
* omrtrace: tracing library for communication with IBM Health Center monitoring tools
* tool: code generation tools for the build system
* gc: garbage collection framework for managed heaps
* vm: APIs to manage per-interpreter and per-thread contexts
* example: demonstration code to show how a language runtime might consume some OMR components
* fvtest: a language-independent test framework so that OMR components can be tested outside of a language runtime


It is our community's fervent goal to be one of active contribution, improvement,
and continual consumption. We will be operating under the [Eclipse Code of Conduct](https://eclipse.org/org/documents/Community_Code_of_Conduct.php)
to promote fairness, openness, and inclusion.



There are some active contribution projects underway right now:

* documentation: code comments are great, but we need more overview documentation so we're writing that
* faq: Frequently Asked Questions from real people's questions (request: ask questions!)
* OMR "sliders": relatively simple but useful work items meant for people new to the project
* diag: more diagnostic support for language runtimes to aid developers and users
* hcagent: the core code for the IBM Health Center agent for interfacing to a runtime
* jit: Just In Time compiler for generating native code for several platforms into managed code caches
* gc: adding generational and other GC policies


The most comprehensive consumer of the OMR technology is the IBM J9 Virtual Machine:
a high performance, scalable, enterprise class Java Virtual Machine implementation
representing hundreds of person years of effort. While not an open source project
itself, J9 directly implements capabilites for Java using the core implementations
provided by the OMR project. The Ruby+OMR Technology Preview project also consumes
the OMR components by modifying the CRuby project. A SOM++ Smalltalk runtime has
also been modified to use OMR componentry.  An experimental version of CPython
using OMR componentry has also been created but is not currently available in the
open. (Our focus has been dominated by getting this code out into the open!)

## Recent Presentations about OMR

* Mark Stoodley's talk at the JVM Languages Summit in August, 2015:
  [A VM is a VM is a VM: The Secret Path to High Performance Multi-Language Runtimes](https://www.youtube.com/watch?v=kOnyJurioyw)

* Daryl Maier's slides from Java One in October, 2015:
  [Beyond the Coffee Cup: Leveraging Java Runtime Technologies for the Polyglot](http://www.slideshare.net/0xdaryl/javaone-2015-con7547-beyond-the-coffee-cup-leveraging-java-runtime-technologies-for-polyglot?related=1)

* Charlie Gracie's slides from Java One in October, 2015:
  [What's in an Object? Java Garbage Collection for the Polyglot](http://www.slideshare.net/charliegracie1/javaone-whats-in-an-object)

* Angela Lin, Robert Young, Craig Lehmann and Xiaoli Liang CASCON Workshop in November, 2015
  [Building Your Own Runtime](https://ibm.box.com/s/7xdg25we2ezmdjjbqdys30d7dl1iyo49)
  * Note: these slides have been modified since the original presentation to use the OMR project name

* Charlie Gracie's talk from FOSDEM in February, 2016:
  [Ruby and OMR: Experiments in utilizing OMR technologies in MRI](http://bofh.nikhef.nl/events/FOSDEM/2016/h2213/ruby-and-omr.mp4)

* Charlie Gracie's slides from jFokus in February, 2016
  [A JVMs Journey into Polyglot Runtimes](https://t.co/efCKf6aCB4)

* Mark Stoodley's slides from EclipseCON in March, 2016
  OMR: A modern toolkit for building language runtimes, link when available
  
## How to Build Standalone OMR
### Basic configuration and compile
To build standalone OMR, run the following commands from the top of the OMR
source tree. The top of the OMR source tree is the directory that contains
run_configure.mk.

    # Generate autotools makefiles with SPEC-specific presets
    make -f run_configure.mk SPEC=linux_x86-64 OMRGLUE=./example/glue

    # Build
    make

    # Run tests (note that no contribution should cause new test failures in "make test")
    make test
    
Run "make -f run_configure.mk help" for a list of configure makefile targets.
Run "make help" for a list of build targets.

#### Building OMR on Windows
A shell script interpreter, such as bash, is required to run configure.

### How to Configure with Custom Options
Run "./configure --help" to see the full list of configure command-line
options.

To run configure using both SPEC presets and custom options, pass the
EXTRA_CONFIGURE_ARGS option to run_configure.mk.

For example, to disable optimizations, run configure like this:

    # Example configure
    make -f run_configure.mk SPEC=linux_x86-64 OMRGLUE=./example/glue 'EXTRA_CONFIGURE_ARGS=--disable-optimized' clean all
  
To disable building fvtests, run configure like this:

    # Example configure disabling fvtests
    make -f run_configure.mk SPEC=linux_x86-64 OMRGLUE=./example/glue 'EXTRA_CONFIGURE_ARGS=--disable-fvtest' clean all

Note that the "clean" target of run_configure.mk deletes the header files and
makefiles generated by configure. Invoking the "clean all" targets ensures that
the header files and makefiles are regenerated using the custom options.

The minimal invocation of configure is:

    # Basic configure example
    $ ./configure OMRGLUE=./example/glue


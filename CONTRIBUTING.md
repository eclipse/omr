# Contributing to Eclipse OMR
Thanks for your interest in this project.

You can propose contributions by sending pull requests through GitHub.

## Project description
The Eclipse OMR project consists of a highly integrated set of open source C
and C++ components that can be used to build robust language runtimes that will
support many different hardware and operating system platforms. These components
include but are not limited to: memory management, threading, platform port
(abstraction) library, diagnostic file support, monitoring support,
garbage collection, and native Just In Time compilation. The long term goal for 
the Eclipse OMR project is to foster an open ecosystem of language runtime
developers to collaborate and collectively innovate with hardware platform
designers, operating system developers, as well as tool and framework developers
and to provide a robust runtime technology platform so that language
implementers can much more quickly and easily create more fully featured
languages to enrich the options available to programmers.

- [https://projects.eclipse.org/projects/technology.omr](https://projects.eclipse.org/projects/technology.omr)


## Legal considerations

Please read the [Eclipse Foundation policy on accepting contributions via Git](http://wiki.eclipse.org/Development_Resources/Contributing_via_Git).

Your contribution cannot be accepted unless you have an [Eclipse Foundation Contributor License Agreement](http://www.eclipse.org/legal/CLA.php) in place.

Here is the checklist for contributions to be _acceptable_:

1. [Create an account at Eclipse](https://dev.eclipse.org/site_login/createaccount.php).
2. Add your GitHub user name in your account settings.
3. [Log into the project's portal](https://projects.eclipse.org/) and sign the ["Eclipse CLA"](https://projects.eclipse.org/user/sign/cla).
4. Ensure that you [_sign-off_](https://wiki.eclipse.org/Development_Resources/Contributing_via_Git#Signing_off_on_a_commit) your Git commits.
5. Ensure that you use the _same_ email address as your Eclipse account in commits.
6. Include the appropriate copyright notice and license at the top of each file.

### Copyright Notice and Licensing Requirements

**It is the responsibility of each contributor to obtain legal advice, and
to ensure that their contributions fulfill the legal requirements of their
organization. This document is not legal advice.**

OMR is dual-licensed under the Eclipse Public License v1.0 and the Apache
License v2.0. Any previously unlicensed contribution should be released under
the same license.

* If you wish to contribute code under a different license, you must consult
with a committer before contributing.
* For any scenario not covered by this document, please discuss the copyright
notice and licensing requirements with a committer before contributing.

The template for the copyright notice and dual-license is as follows:
```c
/*******************************************************************************
 *
 * <Insert appropriate copyright notice here>
 *
 *  This program and the accompanying materials are made available
 *  under the terms of the Eclipse Public License v1.0 and
 *  Apache License v2.0 which accompanies this distribution.
 *
 *      The Eclipse Public License is available at
 *      http://www.eclipse.org/legal/epl-v10.html
 *
 *      The Apache License v2.0 is available at
 *      http://www.opensource.org/licenses/apache2.0.php
 *
 * Contributors:
 *    <First author> - initial implementation and documentation
 *******************************************************************************/
```

## Technical considerations

1. Please follow the coding style and format of the code you are modifying.
2. Please ensure that "make test" passes all tests before you submit a Pull Request.


## GitHub Issues

This project uses GitHub Issues to track ongoing development and issues. Be sure
to search for existing bugs before you create another one. Contributions are always welcome!

- [https://github.com/eclipse/omr/issues](https://github.com/eclipse/omr/issues)


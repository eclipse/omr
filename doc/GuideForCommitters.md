<!--
Copyright (c) 2018, 2019 IBM Corp. and others

This program and the accompanying materials are made available under
the terms of the Eclipse Public License 2.0 which accompanies this
distribution and is available at http://eclipse.org/legal/epl-2.0
or the Apache License, Version 2.0 which accompanies this distribution
and is available at https://www.apache.org/licenses/LICENSE-2.0.

This Source Code may also be made available under the following Secondary
Licenses when the conditions for such availability set forth in the
Eclipse Public License, v. 2.0 are satisfied: GNU General Public License,
version 2 with the GNU Classpath Exception [1] and GNU General Public
License, version 2 with the OpenJDK Assembly Exception [2].

[1] https://www.gnu.org/software/classpath/license.html
[2] http://openjdk.java.net/legal/assembly-exception.html

SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
-->

# Eclipse OMR Committer Guide

This document outlines some general rules for Eclipse OMR committers to
follow when reviewing and merging pull requests and issues.  It also provides
a checklist of items that committers must ensure have been completed prior to
merging a pull request.


## General Guidelines

* Committers should not merge their own pull requests.

* Do not merge a pull request if the Assignee is someone other than yourself.

* Do not merge a pull request if its title is prefixed with `WIP:` (indicating
it's a work-in-progress)

* If a review or feedback was requested by the author by `@mentioning` particular
developers in a pull request, do not merge until all have provided the input
requested.

* Do not merge a pull request until all validation checks and continuous
integration builds have passed.  If there are circumstances where you believe
this rule should be violated (for example, a known test failure unrelated to
this change) then the justification must be documented in the pull request
prior to merging.

* If a pull request is to revert a previous commit, ensure there is a reasonable
explanation in the pull request from the author of the rationale along with a
description of the problem if the commit is not reverted.

* If a pull request modifies the [Contribution Guidelines](https://github.com/eclipse/omr/blob/master/CONTRIBUTING.md),
request the author to post a detailed summary of the changes on the
`omr-dev@eclipse.org` mailing list after the pull request is merged.


## Pre-Merge Checklist

* Ensure the pull request adheres to all the Eclipse OMR [Contribution Guidelines](https://github.com/eclipse/omr/blob/master/CONTRIBUTING.md)

* Ensure pull requests and issues are annotated with descriptive metadata by
attaching GitHub labels.  The current set of labels can be found [here](https://github.com/eclipse/omr/labels).

* If you will be the primary committer, change the Assignee of the pull request
to yourself.  Being the primary committer does not necessarily mean you have to
review the pull request in detail.  You may delegate a deeper technical review
to a developer with expertise in a particular area by `@mentioning` them.  Even
if you delegate a technical review you should look through the requested changes
anyway as you will ultimately be responsible for merging them.

* Regardless of the simplicity of the pull request, explicitly Approve the
changes in the pull request.

* Be sure to validate the commit title and message on **each** commit in the PR (not
just the pull request message) and ensure they describe the contents of the commit.

* Ensure the commits in the pull request are squashed into only as few commits as
is logically required to represent the changes contributed (in other words, superfluous
commits are discouraged without justification).  This is difficult to quantify
objectively, but the committer should verify that each commit contains distinct
changes that should not otherwise be logically squashed with other commits in the
same pull request.

* When commits are pushed to a pull request, TravisCI and AppVeyor builds launch
automatically to test the changes on x86 Linux, macOS, and Windows 32-bit platforms.
If the change affects multiple platforms or 64-bit Windows, you must initiate a pull
request build on all affected platforms prior to merging.  To launch a pull request
build, add a comment to the pull request that follows the syntax:
   ```
   @genie-omr build [ all | {platform-list} ]
   ```
Where `{platform-list}` is a comma-separated list of platforms chosen from the
following tags:

| Tag      | Platform |
| :----    | :------- |
| zlinux   | 64-bit Linux on Z |
| zos      | 64-bit zOS |
| plinux   | 64-bit Power Linux LE |
| aix      | 64-bit AIX |
| x32linux | 32-bit Linux |
| xlinux   | 64-bit Linux |
| xcrlinux | 64-bit Compressed References Linux |
| win      | 64-bit Windows |
| osx      | 64-bit macOS |
| arm      | 32-bit AArch32 (cross-compile build only) |
| aarch64  | 64-bit AArch64 (cross-compile build only) |

   For example, to launch a pull request build on all platforms:
   ```
   @genie-omr build all
   ```
   To launch a pull request build on Z-specific platforms only:
   ```
   @genie-omr build zos,zlinux
   ```
   If testing is only warranted on a subset of platforms (for example, only files
built on x86 are modified) then pull request testing can be limited to only those
platforms.  However, a comment from the committer providing justification is
mandatory.

* Ensure the author has updated the copyright date on each file modified to the
current year.

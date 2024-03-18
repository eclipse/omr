<!--
Copyright IBM Corp. and others 2018

This program and the accompanying materials are made available under
the terms of the Eclipse Public License 2.0 which accompanies this
distribution and is available at https://www.eclipse.org/legal/epl-2.0/
or the Apache License, Version 2.0 which accompanies this distribution
and is available at https://www.apache.org/licenses/LICENSE-2.0.

This Source Code may also be made available under the following Secondary
Licenses when the conditions for such availability set forth in the
Eclipse Public License, v. 2.0 are satisfied: GNU General Public License,
version 2 with the GNU Classpath Exception [1] and GNU General Public
License, version 2 with the OpenJDK Assembly Exception [2].

[1] https://www.gnu.org/software/classpath/license.html
[2] https://openjdk.org/legal/assembly-exception.html

SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
-->

# Eclipse OMR Committer Guide

This document provides information useful for Eclipse OMR committers.

It outlines general rules for committers to follow when reviewing and merging
pull requests and issues and provides a checklist of items that committers must
ensure are completed prior to merging a pull request.

It also documents the minimum participation requirements that Eclipse OMR
committers must meet to maintain their active committer status.

## Reviewing and Merging Pull Requests

### General Guidelines

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


### Pre-Merge Checklist

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

* When commits are pushed to a pull request, Azure builds launch
automatically to test the changes on x86 Linux, macOS, and Windows platforms.
If the change affects multiple platforms, you must initiate a pull
request build on all affected platforms prior to merging.  To launch a pull request
build, add a comment to the pull request that follows the syntax:
   ```
   Jenkins build [ all | {platform-list} ]
   ```
Where `{platform-list}` is a comma-separated list of platforms chosen from the
following names and aliases:

| Spec Name           | Alias    | Architecture   | Platform                                           |
| :--------           | :----    | :-----------   | :-------                                           |
| linux_aarch64       | aarch64  | ARM            | 64-bit Linux on AArch64 (cross-compile build only) |
| linux_arm           | arm      | ARM            | 32-bit Linux on AArch32 (cross-compile build only) |
| osx_aarch64         | amac     | ARM            | 64-bit macOS on AArch64                            |
| aix_ppc-64          | aix      | PPC            | 64-bit AIX                                         |
| linux_ppc-64_le_gcc | plinux   | PPC            | 64-bit Linux on Power LE                           |
| linux_riscv64_cross | riscv    | RISC-V         | 64-bit Linux on RISC-V (cross-compile build only)  |
| linux_x86-64        | xlinux   | x64            | 64-bit Linux on x64                                |
| osx_x86-64          | osx      | x64            | 64-bit macOS                                       |
| win_x86-64          | win      | x64            | 64-bit Windows                                     |
| linux_x86           | x32linux | x86            | 32-bit Linux on x86                                |
| linux_390-64        | zlinux   | Z              | 64-bit Linux on Z                                  |
| zos_390-64          | zos      | Z              | 64-bit zOS                                         |

   For example, to launch a pull request build on all platforms:
   ```
   Jenkins build all
   ```
   To launch a pull request build on Z-specific platforms only:
   ```
   Jenkins build zos,zlinux
   ```
   If testing is only warranted on a subset of platforms (for example, only files
built on x86 are modified) then pull request testing can be limited to only those
platforms.  However, a comment from the committer providing justification is
mandatory.

### Pull Request Builds: Advanced Options

The advanced options provide finer control over the PR builds.

   Syntax to launch PR builds with the advanced options:
   ```
   jenkins build xlinux(OPTION1,OPTION2,...)
   ```

#### Advanced Options

| Option | Description |
| :----- | :---------- |
| cgroupv1 | Run the build on a cgroup.v1 node. <br />Only applies to the Linux operating system. <br />Example: `jenkins build xlinux(cgroupv1)`. |
| !cgroupv1 | Remove the cgroup.v1 label before running the build. <br />Only applies to the Linux operating system. <br />Example: `jenkins build xlinux(!cgroupv1)`. |
| cgroupv2 | Run the build on a cgroup.v2 node. <br />Only applies to the Linux operating system. <br />Example: `jenkins build xlinux(cgroupv2)`. |
| !cgroupv2 | Remove the cgroup.v2 label before running the build. <br />Only applies to the Linux operating system. <br />Example: `jenkins build xlinux(!cgroupv2)`. |
| docker | Run the build inside a Docker container. <br />Only applicable if Docker is installed on the machines. <br />Example: `jenkins build xlinux(cgroupv2,docker)`. |
| !docker | Do not run the build inside a Docker container. <br />Example: `jenkins build xlinux(cgroupv1,!docker)`. |
| cmake:'ARGS' | ARGS will be appended during the configure phase. <br />Example: `jenkins build xlinux(cmake:'-DCMAKE_C_FLAGS=-DDUMP_DBG')`. |
| compile:'ARGS' | ARGS will be appended during the compile phase. <br />Example: `jenkins build xlinux(compile:'-d')`. |
| test:'ARGS' | ARGS will be appended while running the tests. <br />Example: `jenkins build xlinux(test:'-R porttest')`. |
| env:'VAR1=VAL1,VAR2=VAL2' | Environment variables will be added to the build environment. <br />Example: `jenkins build xlinux(env:'GTEST_FILTER=PortDumpTest.*')`. |

#### More Examples

* **Example 1**: `jenkins build xlinux(<OPTIONS_A>),all`. <br />In this example, the xlinux
PR build will use OPTIONS_A whereas all other PR builds will use their default
settings/options.
* **Example 2**: `jenkins build xlinux(<OPTIONS_A>),all(<OPTIONS_B>)`. <br />In this example,
the xlinux PR build will use OPTIONS_A whereas all other PR builds will use OPTIONS_B.
* **Example 3**: `jenkins build xlinux(<OPTIONS_A>),all,linux_x86-64(<OPTIONS_B>)`. <br />In
this example, xlinux is an alias for linux_x86-64. Two different sets of options are
provided for the same PR build specification. The set of options of provided at the end
will be enforced. In this example, OPTIONS_B will be used whereas OPTIONS_A will be
ignored. The same analogy is used if N-sets of options are specified for a PR build
specification.

#### Default Options

| Platform | Default Option |
| :------- | :------------- |
| x32linux | Run on cgroup.v1 nodes inside a Docker container. |
| xlinux | Run on cgroup.v2 nodes inside a Docker container. |
| plinux | Run on cgroup.v2 nodes. |

## Minimum Committer Participation Standard

Eclipse OMR committers are expected to meet at least **one** of the following
criteria within a consecutive 9-month period in order to maintain their active
committer status:

* Review and merge at least one pull request, or
* Author and contribute at least one pull request, or
* Participate in at least 25% of
[OMR Architecture Meetings](https://github.com/eclipse/omr/issues/2316), or
* Other documented contributions not reflected in the above may be considered
on a case-by-case basis by Eclipse OMR project leadership.  For example, significant
contributions to the design of a feature, or assistance resolving a defect,
or project advocacy work may apply.

Committers that do not meet the minimum participation standard will be
retired as committers by project leadership following notification on the `omr-dev`
mailing list and a two week grace period.

Committers may voluntarily retire their committer status by visiting the
[Eclipse OMR project page](https://projects.eclipse.org/projects/technology.omr/who),
clicking on their picture, and selecting and submitting the retirement box.  More
details are available in the
[Eclipse Project Handbook](https://www.eclipse.org/projects/handbook/#elections-retire-cm).

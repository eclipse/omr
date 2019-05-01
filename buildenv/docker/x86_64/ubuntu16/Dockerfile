# Copyright (c) 2018, 2018 IBM Corp. and others
#
# This program and the accompanying materials are made available under
# the terms of the Eclipse Public License 2.0 which accompanies this
# distribution and is available at https://www.eclipse.org/legal/epl-2.0/
# or the Apache License, Version 2.0 which accompanies this distribution and
# is available at https://www.apache.org/licenses/LICENSE-2.0.
# 
# This Source Code may also be made available under the following
# Secondary Licenses when the conditions for such availability set
# forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
# General Public License, version 2 with the GNU Classpath
# Exception [1] and GNU General Public License, version 2 with the
# OpenJDK Assembly Exception [2].
# 
# [1] https://www.gnu.org/software/classpath/license.html
# [2] http://openjdk.java.net/legal/assembly-exception.html
# 
# SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
#
#
# Create OMR build environment with Ubuntu 16.04
# Basic usage (adapt to your environment and scenario):
#   git clone https://github.com/eclipse/omr (or cd to a directory that contains an OMR repository)
#   cd omr/buildenv/docker && docker build -f x86_64/ubuntu16/Dockerfile -t buildomr .
#   cd ../../../
#   docker run --privileged -v $PWD/omr:/omr -it buildomr
#
# The container will automatically run cmake to configure itself and then initiate the build.
# Once the build completes, you'll be left sitting at a prompt in the container so you can
# run more commands.

FROM ubuntu:16.04

# build requirements are everything before "gdb"....others are useful for working test failures
RUN apt-get update -y \
  && apt-get install -y \
	python3 \
	git \
	cmake \
	bison \
	flex \
	libelf-dev \
	libdwarf-dev \
	gcc-4.8 \
	g++-4.8-multilib \
	gdb \
	vim emacs \
  && ln -s /usr/bin/g++-4.8 /usr/bin/g++

ARG OMRDIR
ENV OMRDIR ${OMRDIR:-/omr}

COPY doBuild.sh /doBuild.sh
RUN chmod +x /doBuild.sh

# Uncomment this line to copy your host omr repository into the docker container
# (can help build go faster but changes NOT persisted to host file system)
#COPY omr $OMRDIR

CMD ["/bin/bash", "--init-file", "/doBuild.sh"]

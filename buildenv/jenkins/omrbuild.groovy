/*******************************************************************************
 * Copyright IBM Corp. and others 2020
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at https://www.eclipse.org/legal/epl-2.0/
 * or the Apache License, Version 2.0 which accompanies this distribution and
 * is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code may also be made available under the following
 * Secondary Licenses when the conditions for such availability set
 * forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
 * General Public License, version 2 with the GNU Classpath
 * Exception [1] and GNU General Public License, version 2 with the
 * OpenJDK Assembly Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 *******************************************************************************/

defaultCompile = 'make -j4'
defaultReference = '${HOME}/gitcache'
autoconfBuildDir = '.'
cmakeBuildDir = 'build'
workspaceName = 'Build'
scmVars = null
customWorkspace = null

dockerImage = null
dockerImageName = (params.IMAGE_NAME) ? params.IMAGE_NAME : "buildomr"

/**
 * Move the below parameters into SPECS while implementing a generic
 * approach to support Dockerfiles.
 */
os = (params.OS) ? params.OS : "ubuntu20"
arch = (params.ARCH) ? params.ARCH : "x86_64"

buildSpec = (params.BUILDSPEC) ? params.BUILDSPEC : error("BUILDSPEC not specified")
cmdLine = params.ghprbCommentBody
pullId = params.ghprbPullId

cgroupV1Specs = ["linux_x86"]
cgroupV2Specs = ["linux_x86-64", "linux_ppc-64_le_gcc"]
dockerSpecs = ["linux_x86", "linux_x86-64", "linux_riscv64_cross"]

nodeLabels = []
runInDocker = false

cmakeAppend = ""
compileAppend = ""
testAppend = ""
envAppend = []

SPECS = [
    'aix_ppc-64' : [
        'alias': 'aix',
        'label' : 'compile:aix',
        'reference' : defaultReference,
        'environment' : [
            'PATH+TOOLS=/home/u0020236/tools',
            'LIBPATH=.',
            'CCACHE_CPP2=1',
            'GTEST_COLOR=0'
        ],
        'ccache' : true,
        'buildSystem' : 'cmake',
        'builds' : [
            [
                'buildDir' : cmakeBuildDir,
                'configureArgs' : '-Wdev -DCMAKE_C_COMPILER=xlc_r -DCMAKE_CXX_COMPILER=xlC_r -DCMAKE_XL_CreateExportList="" -DOMR_DDR=OFF -C../cmake/caches/Travis.cmake',
                'compile' : 'export CCACHE_EXTRAFILES="$PWD/omrcfg.h" && make -j8'
            ]
        ],
        'test' : true,
        'testArgs' : '',
        'junitPublish' : true
    ],
    'linux_390-64' : [
        'alias': 'zlinux',
        'label': 'compile:zlinux',
        'reference' : defaultReference,
        'environment' : [
            'PATH+CCACHE=/usr/lib/ccache/',
            'GTEST_COLOR=0'
        ],
        'ccache' : true,
        'buildSystem' : 'cmake',
        'builds' : [
            [
                'buildDir' : cmakeBuildDir,
                'configureArgs' : '-Wdev -C../cmake/caches/Travis.cmake',
                'compile' : defaultCompile
            ]
        ],
        'test' : true,
        'testArgs' : '',
        'junitPublish' : true
    ],
    'linux_aarch64' : [
        'alias': 'aarch64',
        'label' : 'compile:aarch64:cross',
        'reference' : defaultReference,
        'environment' : [
            'PATH+CCACHE=/usr/lib/ccache/:/home/jenkins/aarch64/toolchain/bin',
            'PLATFORM=aarch64-linux-gcc',
            'CHOST=aarch64-linux-gnu'
        ],
        'ccache' : true,
        'buildSystem' : 'autoconf',
        'builds' : [
            [
                'buildDir' : autoconfBuildDir,
                'configureArgs' : 'SPEC=linux_aarch64',
                'compile' : defaultCompile
            ]
        ],
        'test' : false
    ],
    'linux_arm' : [
        'alias': 'arm',
        'label' : 'compile:arm:cross',
        'reference' : defaultReference,
        'environment' : [
            'PATH+CCACHE=/usr/lib/ccache/:/home/jenkins/arm/toolchain/bin',
            'PLATFORM=arm-linux-gcc',
            'CHOST=arm-linux-gnueabihf'
        ],
        'ccache' : true,
        'buildSystem' : 'autoconf',
        'builds' : [
            [
                'buildDir' : autoconfBuildDir,
                'configureArgs' : 'SPEC=linux_arm',
                'compile' : defaultCompile
            ]
        ],
        'test' : false
    ],
    'linux_ppc-64_le_gcc' : [
        'alias': 'plinux',
        'label' : 'compile:plinux',
        'reference' : defaultReference,
        'environment' : [
            'PATH+CCACHE=/usr/lib/ccache/',
            'GTEST_COLOR=0'
        ],
        'ccache' : true,
        'buildSystem' : 'cmake',
        'builds' : [
            [
                'buildDir' : cmakeBuildDir,
                'configureArgs' : '-Wdev -C../cmake/caches/Travis.cmake',
                'compile' : defaultCompile
            ]
        ],
        'test' : true,
        'testArgs' : '',
        'junitPublish' : true
    ],
    'linux_riscv64_cross' : [
        'alias': 'riscv_cross',
        'label' : 'compile:riscv64:cross',
        'reference' : defaultReference,
        'environment' : [
            'PATH+CCACHE=/usr/lib/ccache/'
        ],
        'ccache' : true,
        'buildSystem' : 'cmake',
        'builds' : [
            [
                'buildDir' : cmakeBuildDir,
                'configureArgs' : '-Wdev -C../cmake/caches/Travis.cmake -DOMR_DDR=OFF  -DCMAKE_TOOLCHAIN_FILE=../cmake/toolchains/riscv64-linux-cross.cmake "-DOMR_EXE_LAUNCHER=qemu-riscv64-static;-L;${CROSS_SYSROOT_RISCV64}" "-DCMAKE_SYSROOT=${CROSS_SYSROOT_RISCV64}"',
                'compile' : defaultCompile
            ]
        ],
        'test' : true,
        'testArgs' : '',
        'junitPublish' : true
    ],
    'linux_riscv64' : [
        'alias': 'riscv',
        'label' : 'hw.arch.riscv64',
        'reference' : defaultReference,
        'environment' : [
            'PATH+CCACHE=/usr/lib/ccache/',
            /*
             * As of now, native RISC-V build agents are running as a Docker containers
             * so we need to set OMR_RUNNING_IN_DOCKER even though the build itself does not
             * use Docker. Otherwise sysinfo_is_running_in_container would fail.
             */
            'OMR_RUNNING_IN_DOCKER=1'
        ],
        'ccache' : true,
        'buildSystem' : 'cmake',
        'builds' : [
            [
                'buildDir' : cmakeBuildDir,
                'configureArgs' : '-Wdev -C../cmake/caches/Travis.cmake',
                'compile' : defaultCompile
            ]
        ],
        'test' : true,
        'testArgs' : '',
        'junitPublish' : true
    ],
    'linux_x86' : [
        'alias': 'x32linux',
        'label' : 'compile:xlinux',
        'reference' : defaultReference,
        'environment' : [
            'PATH+CCACHE=/usr/lib/ccache/',
            'GTEST_COLOR=0'
        ],
        'ccache' : true,
        'buildSystem' : 'cmake',
        'builds' : [
            [
                'buildDir' : cmakeBuildDir,
                'configureArgs' : '-Wdev -G Ninja -DOMR_ENV_DATA32=ON -DOMR_DDR=OFF -DOMR_JITBUILDER=OFF -C../cmake/caches/Travis.cmake',
                'compile' : 'ninja'
            ]
        ],
        'test' : true,
        'testArgs' : '',
        'junitPublish' : true
    ],
    'linux_x86-64' : [
        'alias': 'xlinux',
        'label' : 'compile:xlinux',
        'reference' : defaultReference,
        'environment' : [
            'PATH+CCACHE=/usr/lib/ccache/',
            'GTEST_COLOR=0'
        ],
        'ccache' : true,
        'buildSystem' : 'cmake',
        'builds' : [
            [
                'buildDir' : cmakeBuildDir,
                'configureArgs' : '-Wdev -C../cmake/caches/Travis.cmake -DOMR_OPT_CUDA=ON -DOMR_CUDA_HOME=/usr/local/cuda',
                'compile' : defaultCompile
            ]
        ],
        'test' : true,
        'testArgs' : '',
        'junitPublish' : true
    ],
    'osx_x86-64' : [
        'alias': 'osx',
        'label' : 'compile:xosx',
        'reference' : defaultReference,
        'environment' : [
            'GTEST_FILTER=-*dump_test_create_dump_*:*NumaSetAffinity:*NumaSetAffinitySuspended:*DeathTest*',
            'PATH+CCACHE=/usr/local/opt/ccache/libexec',
            'GTEST_COLOR=0'
        ],
        'ccache' : true,
        'buildSystem' : 'cmake',
        'builds' : [
            [
                'buildDir' : cmakeBuildDir,
                'configureArgs' : '-Wdev -C../cmake/caches/Travis.cmake',
                'compile' : defaultCompile
            ]
        ],
        'test' : true,
        'testArgs' : '',
        'junitPublish' : true
    ],

    'osx_aarch64' : [
        'alias' : 'amac',
        'label' : 'compile:amac',
        'reference' : defaultReference,
        'environment' : [
            'GTEST_FILTER=-*dump_test_create_dump_*:*NumaSetAffinity:*NumaSetAffinitySuspended:*DeathTest*',
            'PATH+CCACHE=/usr/local/opt/ccache/libexec',
            'GTEST_COLOR=0'
        ],
        'ccache' : true,
        'buildSystem' : 'cmake',
        'builds' : [
            [
                'buildDir' : cmakeBuildDir,
                'configureArgs' : '-Wdev -C../cmake/caches/Travis.cmake',
                'compile' : defaultCompile
            ]
        ],
        'test' : true,
        'testArgs' : '',
        'junitPublish' : true
    ],

    'win_x86-64' : [
        'alias': 'win',
        'label' : 'compile:xwindows',
        'reference' : defaultReference,
        'environment' : [
            'GTEST_FILTER=-*dump_test_create_dump_*:*NumaSetAffinity:*NumaSetAffinitySuspended:PortSysinfoTest.sysinfo_test_get_tmp3:ThreadExtendedTest.TestOtherThreadCputime',
            'GTEST_COLOR=0',
            'PATH+TOOLS=/cygdrive/c/CMake/bin/:/cygdrive/c/Program Files (x86)/Microsoft Visual Studio/2017/Community/MSBuild/15.0/Bin/:/cygdrive/c/Program Files (x86)/Windows Kits/10/bin/x86'
        ],
        'ccache' : false,
        'buildSystem' : 'cmake',
        'builds' : [
            [
                'buildDir' : cmakeBuildDir,
                'configureArgs' : '-Wdev -G "Visual Studio 15 2017" -A x64 -C../cmake/caches/Windows.cmake',
                'compile' : 'cmake --build . -- /m'
            ]
        ],
        'test' : true,
        'testArgs' : '-C Debug -j1',
        'junitPublish' : true
    ],
    'zos_390-64' : [
        'alias': 'zos',
        'label' : 'compile:zos',
        'reference' : '',
        'environment' : [
            "LIBPATH+EXTRA=/u/user1/workspace/${workspaceName}/build",
            "_C89_ACCEPTABLE_RC=0",
            "_CXX_ACCEPTABLE_RC=0"
        ],
        'ccache' : false,
        'buildSystem' : 'cmake',
        'builds' : [
            [
                'buildDir' : cmakeBuildDir,
                'configureArgs' : '-Wdev -C../cmake/caches/Travis.cmake -DCMAKE_C_COMPILER=/bin/c89 -DCMAKE_CXX_COMPILER=/bin/xlc -DOMR_DDR=OFF -DOMR_THR_FORK_SUPPORT=0',
                'compile' : defaultCompile
            ]
        ],
        'test' : true,
        'testArgs' : '',
        'junitPublish' : false
    ]
]

def setBuildStatus(String message, String state, String sha) {
    context = "continuous-integration/eclipse-omr/branch/" + buildSpec
    step([
        $class: "GitHubCommitStatusSetter",
        contextSource: [$class: "ManuallyEnteredCommitContextSource", context: context],
        errorHandlers: [[$class: "ChangingBuildStatusErrorHandler", result: "UNSTABLE"]],
        commitShaSource: [$class: "ManuallyEnteredShaSource", sha: sha ],
        statusBackrefSource: [$class: "ManuallyEnteredBackrefSource", backref: "${BUILD_URL}flowGraphTable/"],
        statusResultSource: [$class: "ConditionalStatusResultSource", results: [[$class: "AnyBuildResult", message: message, state: state]] ]
    ]);
}

def getSources() {
    stage('Get Sources') {
        def gitConfig = scm.getUserRemoteConfigs().get(0)
        def refspec = (gitConfig.getRefspec()) ? gitConfig.getRefspec() : ''
        def ret = false
        retry(10) {
            if (ret) {
                sleep time: 60, unit: 'SECONDS'
            } else {
                ret = true
            }
            scmVars = checkout poll: false,
                scm: [$class: 'GitSCM',
                branches: [[name: "${scm.branches[0].name}"]],
                extensions: [[$class: 'CloneOption', honorRefspec: true, timeout: 30, reference: SPECS[buildSpec].reference]],
                userRemoteConfigs: [[name: 'origin',
                    refspec: "${refspec}",
                    url: "${gitConfig.getUrl()}"]
                ]
            ]
        }
        if (!pullId) {
            setBuildStatus("In Progress","PENDING","${scmVars.GIT_COMMIT}")
        }
    }
}

def build() {
    stage('Build') {
        if (SPECS[buildSpec].ccache) {
            echo 'Output CCACHE stats before running and clear them'
            sh 'ccache -s -z'
        }

        for (build in SPECS[buildSpec].builds) {
            dir("${build.buildDir}") {
                echo 'Configure...'
                switch (SPECS[buildSpec].buildSystem) {
                    case 'cmake':
                        sh "cmake ${build.configureArgs} ${cmakeAppend} .."
                        break
                    case 'autoconf':
                        sh "make -f run_configure.mk OMRGLUE=./example/glue ${build.configureArgs} ${cmakeAppend}"
                        break
                    default:
                        error("Unknown buildSystem type")
                }

                echo 'Compile...'
                sh "${build.compile} ${compileAppend}"
            }
        }

        if (SPECS[buildSpec].ccache) {
            echo 'Output CCACHE stats after running'
            sh 'ccache -s'
        }
    }
}

def test() {
    stage('Test') {
        if (SPECS[buildSpec].test) {
            echo 'Sanity Test...'
            switch (SPECS[buildSpec].buildSystem) {
                case 'cmake':
                    dir("${cmakeBuildDir}") {
                        sh "ctest -V ${SPECS[buildSpec].testArgs} ${testAppend}"
                        if (SPECS[buildSpec].junitPublish) {
                            junit '**/*results.xml'
                        }
                    }
                    break
                case 'autoconf':
                    sh "make test ${SPECS[buildSpec].testArgs} ${testAppend}"
                    break
                default:
                    error("Unknown buildSystem type")
            }
        } else {
            echo "Currently no sanity tests..."
        }
    }
}

def cleanDockerContainers() {
    stage("Docker Remove Containers") {
        println("Listing docker containers to attempt removal")
        sh "docker ps -a"
        def containers = sh (script: "docker ps -a --format \"{{.ID}}\"", returnStdout: true).trim()
        if (containers) {
            println("Stop all docker containers")
            sh "docker ps -a --format \"{{.ID}}\" | xargs docker stop"
            println("Remove all docker containers")
            sh "docker ps -a --format \"{{.ID}}\" | xargs docker rm --force"
        }
    }
}

def buildDockerImage() {
    stage("Docker Build") {
        dir("buildenv/docker/${arch}/${os}") {
            dockerImage = docker.build(dockerImageName)
        }
    }
}

def allStages() {
    try {
        timeout(time: 2, unit: 'HOURS') {
            def tmpDesc = (currentBuild.description) ? currentBuild.description + "<br>" : ""
            currentBuild.description = tmpDesc + "<a href=${JENKINS_URL}computer/${NODE_NAME}>${NODE_NAME}</a>"

            def environmentVars = SPECS[buildSpec].environment

            if (!envAppend.isEmpty()) {
                environmentVars.addAll(envAppend)
            }

            if (runInDocker) {
                environmentVars += "OMR_RUNNING_IN_DOCKER=1"
            }

            println "env vars: " + environmentVars

            withEnv(environmentVars) {
                sh 'printenv'
                /* Source already populated in runAllStagesInDocker while creating
                 * the Docker image. Avoid this redundant operation while running in a
                 * Docker container.
                 */
                if (!runInDocker) {
                    getSources()
                }
                build()
                test()
            }
        }
    } finally {
        if (!pullId && scmVars) {
            setBuildStatus("Complete", currentBuild.currentResult, "${scmVars.GIT_COMMIT}")
        }
    }
}

def runAllStages() {
    ws(customWorkspace) {
        try {
            allStages()
        } finally {
            cleanWs()
        }
    }
}

def runAllStagesInDocker() {
    ws(customWorkspace) {
        try {
            cleanDockerContainers()
            getSources()
            buildDockerImage()
            dockerImage.inside("-v /home/jenkins:/home/jenkins:rw,z") {
                dir(customWorkspace) {
                    allStages()
                }
            }
        } finally {
            cleanWs()
        }
    }
}

def parseCmdLine() {
    def selectedMatcher = null

    def specMatcher = (cmdLine =~ /.*jenkins build.*${buildSpec}?(?<match>\(.*\))?.*/)

    def alias = SPECS[buildSpec].alias
    def aliasMatcher = (cmdLine =~ /.*jenkins build.*${alias}?(?<match>\(.*\))?.*/)

    def allMatcher = (cmdLine =~ /.*jenkins build.*all?(?<match>\(.*\))?.*/)

    def specIndex = -1
    if (specMatcher.find()) {
        specIndex = specMatcher.start("match")
    }

    def aliasIndex = -1
    if (aliasMatcher.find()) {
        aliasIndex = aliasMatcher.start("match")
    }

    def allIndex = -1
    if (allMatcher.find()) {
        allIndex = allMatcher.start("match")
    }

    if ((specIndex != -1) || (aliasIndex != -1)) {
        if (specIndex > aliasIndex) {
            selectedMatcher = specMatcher
        } else {
            selectedMatcher = aliasMatcher
        }
    } else if (allIndex != -1) {
        selectedMatcher = allMatcher
    }

    println "custom option matcher: " + selectedMatcher

    if (selectedMatcher != null) {
        def customConfig = selectedMatcher.group("match")
        println "custom options: " + customConfig

        if (customConfig != null) {
            def cmakeMatcher = (customConfig =~ /cmake:'(?<match>.*?)'/)
            if (cmakeMatcher.find()) {
                cmakeAppend += cmakeMatcher.group("match")
                customConfig -= ~/cmake:'(?<match>.*?)'/
            }
            println "cmake append: " + cmakeAppend

            def compileMatcher = (customConfig =~ /compile:'(?<match>.*?)'/)
            if (compileMatcher.find()) {
                compileAppend += compileMatcher.group("match")
                customConfig -= ~/compile:'(?<match>.*?)'/
            }
            println "compile append: " + compileAppend

            def testMatcher = (customConfig =~ /test:'(?<match>.*?)'/)
            if (testMatcher.find()) {
                testAppend += testMatcher.group("match")
                customConfig -= ~/test:'(?<match>.*?)'/
            }
            println "test append: " + testAppend

            def envMatcher = (customConfig =~ /env:'(?<match>.*?)'/)
            if (envMatcher.find()) {
                envAppend.addAll(envMatcher.group("match").tokenize(","))
                customConfig -= ~/env:'(?<match>.*?)'/
            }
            println "env append: " + envAppend
            println "remaining custom options: " + customConfig

            def options = customConfig.replace("(", "").replace(")", "").split(",")
            for (option in options) {
                if (option == "cgroupv1") {
                    nodeLabels += "cgroup.v1"
                    nodeLabels -= "cgroup.v2"
                }

                if (option == "cgroupv2") {
                    nodeLabels += "cgroup.v2"
                    nodeLabels -= "cgroup.v1"
                }

                if (option == "!cgroupv1") {
                    nodeLabels -= "cgroup.v1"
                }

                if (option == "!cgroupv2") {
                    nodeLabels -= "cgroup.v2"
                }

                if (option == "docker") {
                    runInDocker = true
                }

                if (option == "!docker") {
                    runInDocker = false
                }
            }
        }
    }
}

timestamps {
    timeout(time: 8, unit: 'HOURS') {
        stage('Queue') {
            nodeLabels += SPECS[buildSpec].label
            runInDocker = dockerSpecs.contains(buildSpec)

            if (cgroupV1Specs.contains(buildSpec)) {
                nodeLabels += "cgroup.v1"
            }

            if (cgroupV2Specs.contains(buildSpec)) {
                nodeLabels += "cgroup.v2"
            }

            parseCmdLine()

            nodeLabels = nodeLabels.unique()

            println nodeLabels
            println "run in docker: " + runInDocker

            assert !(nodeLabels.contains("cgroup.v1") && nodeLabels.contains("cgroup.v2"))

            node(nodeLabels.join(" && ")) {
                /* Use a custom workspace name.
                 * This is becasue the LIBPATH on z/os includes the build's workspace.
                 * Since we cannot add a variable to the 'environment' in the SPECS, as
                 * it will get evaluated early, we'll use a custom ws in order to
                 * hardcode the value in the SPECS.
                 */
                customWorkspace = WORKSPACE - "/${JOB_NAME}" + "/${workspaceName}"
                println "customWorkspace: " + customWorkspace

                if (runInDocker) {
                    runAllStagesInDocker()
                } else {
                    runAllStages()
                }
            }
        }
    }
}

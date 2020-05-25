def setBuildStatus(String message, String state, String sha) {
    context = "continuous-integration/eclipse-omr/branch/${params.BUILDSPEC}"
    step([
        $class: "GitHubCommitStatusSetter",
        contextSource: [$class: "ManuallyEnteredCommitContextSource", context: context],
        errorHandlers: [[$class: "ChangingBuildStatusErrorHandler", result: "UNSTABLE"]],
        commitShaSource: [$class: "ManuallyEnteredShaSource", sha: sha ],
        statusBackrefSource: [$class: "ManuallyEnteredBackrefSource", backref: "${BUILD_URL}flowGraphTable/"],
        statusResultSource: [$class: "ConditionalStatusResultSource", results: [[$class: "AnyBuildResult", message: message, state: state]] ]
    ]);
}

def defaultCompile = 'make -j4'
def autoconfBuildDir = '.'
def cmakeBuildDir = 'build'
def workspaceName = 'Build'

def SPECS = [
    'aix_ppc-64' : [
        'label' : 'aix && ppc',
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
        'label': 'Linux && 390',
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
        'label' : 'Linux && x86 && compile:aarch64',
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
        'label' : 'Linux && x86 && compile:arm',
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
        'label' : 'Linux && PPCLE',
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
    'linux_riscv64' : [
        'label' : 'Linux && riscv64',
        'environment' : [
            'PATH+CCACHE=/usr/lib/ccache/'
        ],
        'ccache' : true,
        'buildSystem' : 'cmake',
        'builds' : [
            [
                'buildDir' : autoconfBuildDir,
                'configureArgs' : '-Wdev -C../cmake/caches/Travis.cmake',
                'compile' : defaultCompile
            ]
        ],
        'test' : true,
        'testArgs' : '',
        'junitPublish' : true
    ],
    'linux_riscv64_cross' : [
        'label' : 'Linux && x86 && compile:riscv64',
        'environment' : [
            'PATH+CCACHE=/usr/lib/ccache/'
        ],
        'ccache' : true,
        'buildSystem' : 'cmake',
        'builds' : [
            [
                'buildDir' : 'build_native',
                'configureArgs' : '-DOMR_THREAD=OFF -DOMR_PORT=OFF -DOMR_OMRSIG=OFF -DOMR_GC=OFF -DOMR_FVTEST=OFF',
                'compile' : defaultCompile
            ],
            [
                'buildDir' : cmakeBuildDir,
                'configureArgs' : '-Wdev -C../cmake/caches/Travis.cmake -DCMAKE_FIND_ROOT_PATH=${CROSS_SYSROOT_RISCV64} -DCMAKE_TOOLCHAIN_FILE=../cmake/toolchains/riscv64-linux-cross.cmake -DOMR_TOOLS_IMPORTFILE=../build_native/tools/ImportTools.cmake',
                'compile' : defaultCompile
            ]
        ],
        'test' : false
    ],
    'linux_x86' : [
        'label' : 'Linux && x86',
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
        'label' : 'Linux && x86',
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
    'linux_x86-64_cmprssptrs' : [
        'label' : 'Linux && x86',
        'environment' : [
            'PATH+CCACHE=/usr/lib/ccache/',
            'EXTRA_CONFIGURE_ARGS=--enable-DDR'
        ],
        'ccache' : true,
        'buildSystem' : 'autoconf',
        'builds' : [
            [
                'buildDir' : autoconfBuildDir,
                'configureArgs': 'SPEC=linux_x86-64_cmprssptrs PLATFORM=amd64-linux64-gcc HAS_AUTOCONF=1 all',
                'compile' : defaultCompile
            ]
        ],
        'test' : true,
        'testArgs' : '',
        'junitPublish' : true
    ],
    'osx_x86-64' : [
        'label' : 'OSX && x86',
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
        'label' : 'Windows && x86',
        'environment' : [
            'GTEST_FILTER=-*dump_test_create_dump_*:*NumaSetAffinity:*NumaSetAffinitySuspended:PortSysinfoTest.sysinfo_test_get_tmp3:ThreadExtendedTest.TestOtherThreadCputime',
            'GTEST_COLOR=0'
        ],
        'ccache' : false,
        'buildSystem' : 'cmake',
        'builds' : [
            [
                'buildDir' : cmakeBuildDir,
                'configureArgs' : '-Wdev -G "Visual Studio 11 2012 Win64" -C../cmake/caches/AppVeyor.cmake',
                'compile' : 'cmake --build . -- /m'
            ]
        ],
        'test' : true,
        'testArgs' : '-C Debug -j1',
        'junitPublish' : true
    ],
    'zos_390-64' : [
        'label' : 'zOS && 390',
        'environment' : [
            "LIBPATH+EXTRA=/openzdk/jenkins/workspace/${workspaceName}/build"
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

timestamps {
    timeout(time: 8, unit: 'HOURS') {
        stage('Queue') {
            node(SPECS[params.BUILDSPEC].label) {
                /*
                 * Use a custom workspace name.
                 * This is becasue the LIBPATH on z/os includes the build's workspace.
                 * Since we cannot add a variable to the 'environment' in the SPECS, as
                 * it will get evaluated early, we'll use a custom ws in order to
                 * hardcode the value in the SPECS.
                 */
                def customWorkspace = WORKSPACE - "/${JOB_NAME}" + "/${workspaceName}"
                ws(customWorkspace) {
                    try {
                        timeout(time: 1, unit: 'HOURS') {
                            def tmpDesc = (currentBuild.description) ? currentBuild.description + "<br>" : ""
                            currentBuild.description = tmpDesc + "<a href=${JENKINS_URL}computer/${NODE_NAME}>${NODE_NAME}</a>"

                            withEnv(SPECS[params.BUILDSPEC].environment) {
                                sh 'printenv'
                                stage('Get Sources') {
                                    if (params.ghprbPullId) {
                                        def gitConfig = scm.getUserRemoteConfigs().get(0)
                                        checkout poll: false,
                                            scm: [$class: 'GitSCM',
                                            branches: [[name: '${sha1}']],
                                            extensions: [[$class: 'CloneOption', honorRefspec: true]],
                                            userRemoteConfigs: [[name: 'origin',
                                                refspec: "+refs/pull/${params.ghprbPullId}/merge:refs/remotes/origin/pr/${params.ghprbPullId}/merge",
                                                url: "${gitConfig.getUrl()}"]
                                            ]
                                        ]
                                    } else {
                                        scmVars = checkout scm
                                        setBuildStatus("In Progress","PENDING","${scmVars.GIT_COMMIT}")
                                    }
                                }
                                stage('Build') {
                                    if (SPECS[params.BUILDSPEC].ccache) {
                                        echo 'Output CCACHE stats before running and clear them'
                                        sh 'ccache -s -z'
                                    }

                                    for (build in SPECS[params.BUILDSPEC].builds) {
                                        dir("${build.buildDir}") {
                                            echo 'Configure...'
                                            switch (SPECS[params.BUILDSPEC].buildSystem) {
                                                case 'cmake':
                                                    sh "cmake ${build.configureArgs} .."
                                                    break
                                                case 'autoconf':
                                                    sh "make -f run_configure.mk OMRGLUE=./example/glue ${build.configureArgs}"
                                                    break
                                                default:
                                                    error("Unknown buildSystem type")
                                            }

                                            echo 'Compile...'
                                            sh "${build.compile}"
                                        }
                                    }

                                    if (SPECS[params.BUILDSPEC].ccache) {
                                        echo 'Output CCACHE stats after running'
                                        sh 'ccache -s'
                                    }
                                }
                                stage('Test') {
                                    if (SPECS[params.BUILDSPEC].test) {
                                        echo 'Sanity Test...'
                                        switch (SPECS[params.BUILDSPEC].buildSystem) {
                                            case 'cmake':
                                                dir("${cmakeBuildDir}") {
                                                    sh "ctest -V ${SPECS[params.BUILDSPEC].testArgs}"
                                                    if (SPECS[params.BUILDSPEC].junitPublish) {
                                                        junit '**/*results.xml'
                                                    }
                                                }
                                                break
                                            case 'autoconf':
                                                sh "make test ${SPECS[params.BUILDSPEC].testArgs}"
                                                break
                                            default:
                                                error("Unknown buildSystem type")
                                        }
                                    } else {
                                        echo "Currently no sanity tests..."
                                    }
                                }
                            }
                        }
                    } finally {
                        if (!params.ghprbPullId) {
                            setBuildStatus("Complete", currentBuild.currentResult, "${scmVars.GIT_COMMIT}")
                        }
                        cleanWs()
                    }
                }
            }
        }
    }
}

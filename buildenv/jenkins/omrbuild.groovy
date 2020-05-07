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

class CMake {
    def buildDir = 'build'
    def configureArgs = '-Wdev -C../cmake/caches/Travis.cmake'

    def configure = {
        steps.sh "cmake ${configureArgs} .."
    }

    def compile = {
        steps.sh "make -j4"
    }

    def test = {
        steps.sh "ctest -V"
        steps.junit '**/*results.xml'
    }

    def static steps = null;
}

CMake.steps = this;

class Autotools {
    def buildDir = '.'
    def configureArgs = ''

    def configure = {
        steps.sh "make -f run_configure.mk OMRGLUE=./example/glue ${configureArgs}"
    }

    def compile = {
        steps.sh "make -j4"
    }

    def test = {
        steps.sh "make test"
    }

    def static steps = null;
}

Autotools.steps = this;

SPECS = [
    'aix_ppc-64' : [
        'label' : 'aix && ppc',
        'environment' : [
            'PATH+TOOLS=/home/u0020236/tools',
            'CCACHE_CPP2=1'
        ],
        'ccache' : true,
        'builds' : [
            new CMake(
                configureArgs: '-Wdev -DCMAKE_C_COMPILER=xlc_r -DCMAKE_CXX_COMPILER=xlC_r -DCMAKE_XL_CreateExportList="/opt/IBM/xlC/13.1.3/bin/CreateExportList -X64" -DOMR_DDR=OFF -C../cmake/caches/Travis.cmake',
                compile: { sh 'export CCACHE_EXTRAFILES="$PWD/omrcfg.h" && make -j8' }
            )
        ],
    ],
    'linux_390-64' : [
        'label': 'Linux && 390',
        'environment' : [
            'PATH+CCACHE=/usr/lib/ccache/'
        ],
        'ccache' : true,
        'builds' : [
            new CMake()
        ],
    ],
    'linux_aarch64' : [
        'label' : 'Linux && x86 && compile:aarch64',
        'environment' : [
            'PATH+CCACHE=/usr/lib/ccache/:/home/jenkins/aarch64/toolchain/bin',
            'PLATFORM=aarch64-linux-gcc',
            'CHOST=aarch64-linux-gnu'
        ],
        'ccache' : true,
        'builds' : [
            new Autotools(
                configureArgs: 'SPEC=linux_aarch64',
                test: { echo "Skip tests as this is a cross-compilation" }
            )
        ],
    ],
    'linux_arm' : [
        'label' : 'Linux && x86 && compile:arm',
        'environment' : [
            'PATH+CCACHE=/usr/lib/ccache/:/home/jenkins/arm/toolchain/bin',
            'PLATFORM=arm-linux-gcc',
            'CHOST=arm-linux-gnueabihf'
        ],
        'ccache' : true,
        'builds' : [
            new Autotools(
                configureArgs: 'SPEC=linux_arm',
                test: { echo "Skip tests as this is a cross-compilation" }
            )
        ],
    ],
    'linux_ppc-64_le_gcc' : [
        'label' : 'Linux && PPCLE',
        'environment' : [
            'PATH+CCACHE=/usr/lib/ccache/'
        ],
        'ccache' : true,
        'builds' : [
            new CMake()
        ],
    ],
    'linux_riscv64' : [
        'label' : 'Linux && riscv64',
        'environment' : [
            'PATH+CCACHE=/usr/lib/ccache/'
        ],
        'ccache' : true,
        'builds' : [
            new CMake()
        ],
    ],
    'linux_riscv64_cross' : [
        'label' : 'Linux && x86 && compile:riscv64',
        'environment' : [
            'PATH+CCACHE=/usr/lib/ccache/'
        ],
        'ccache' : true,
        'builds' : [
            new CMake(
                buildDir: 'build_native',
                configureArgs: '-DOMR_THREAD=OFF -DOMR_PORT=OFF -DOMR_OMRSIG=OFF -DOMR_GC=OFF -DOMR_FVTEST=OFF',
            ),
            new CMake(
                configureArgs: '-Wdev -C../cmake/caches/Travis.cmake -DCMAKE_FIND_ROOT_PATH=${CROSS_SYSROOT_RISCV64} -DCMAKE_TOOLCHAIN_FILE=../cmake/toolchains/riscv64-linux-cross.cmake -DOMR_TOOLS_IMPORTFILE=../build_native/tools/ImportTools.cmake',
                test: { echo "Skip tests as this is a cross-compilation" }
            )
        ],
    ],
    'linux_x86' : [
        'label' : 'Linux && x86',
        'environment' : [
            'PATH+CCACHE=/usr/lib/ccache/'
        ],
        'ccache' : true,
        'builds' : [
            new CMake(
                /*
                 * Here we use ninja to build rather than make. The reason is
                 * to test whether it works with ninja as it is more strict
                 */
                configureArgs: '-Wdev -G Ninja -DOMR_ENV_DATA32=ON -DOMR_DDR=OFF -DOMR_JITBUILDER=OFF -C../cmake/caches/Travis.cmake',
                compile: { sh 'ninja' }
            )
        ],
    ],
    'linux_x86-64' : [
        'label' : 'Linux && x86',
        'environment' : [
            'PATH+CCACHE=/usr/lib/ccache/'
        ],
        'ccache' : true,
        'builds' : [
            new CMake(
                configureArgs: '-Wdev -C../cmake/caches/Travis.cmake -DOMR_OPT_CUDA=ON -DOMR_CUDA_HOME=/usr/local/cuda'
            )
        ],
    ],
    'linux_x86-64_cmprssptrs' : [
        'label' : 'Linux && x86',
        'environment' : [
            'PATH+CCACHE=/usr/lib/ccache/',
            'EXTRA_CONFIGURE_ARGS=--enable-DDR'
        ],
        'ccache' : true,
        'builds' : [
            new Autotools(
                configureArgs: 'SPEC=linux_x86-64_cmprssptrs PLATFORM=amd64-linux64-gcc HAS_AUTOCONF=1 all'
            )
        ],
    ],
    'osx_x86-64' : [
        'label' : 'OSX && x86',
        'environment' : [
            'GTEST_FILTER=-*dump_test_create_dump_*:*NumaSetAffinity:*NumaSetAffinitySuspended:*DeathTest*',
            'PATH+CCACHE=/usr/local/opt/ccache/libexec'
        ],
        'ccache' : true,
        'builds' : [
            new CMake()
        ],
    ],
    'win_x86-64' : [
        'label' : 'Windows && x86',
        'environment' : [
            'GTEST_FILTER=-*dump_test_create_dump_*:*NumaSetAffinity:*NumaSetAffinitySuspended:PortSysinfoTest.sysinfo_test_get_tmp3:ThreadExtendedTest.TestOtherThreadCputime'
        ],
        'ccache' : false,
        'builds' : [
            new CMake(
                configureArgs: '-Wdev -G "Visual Studio 11 2012 Win64" -C../cmake/caches/AppVeyor.cmake',
                compile: { sh 'cmake --build . -- /m' },
                test: { sh "ctest -V -C Debug -j1" }
            )
        ],
    ],
    'zos_390-64' : [
        'label' : 'zOS && 390',
        'environment' : [
        ],
        'ccache' : false,
        'builds' : [
            new CMake(
                configureArgs: '-Wdev -C../cmake/caches/Travis.cmake -DCMAKE_C_COMPILER=/bin/c89 -DCMAKE_CXX_COMPILER=/bin/xlc -DOMR_DDR=OFF -DOMR_THR_FORK_SUPPORT=0',
                /*
                 * Run tests but don't record results since files
                 * are in ebcdic and need to be recoded. See PR #5060.
                 */
                test: { sh "ctest -V" }
            )
        ],
    ]
]

def launch(target) {
spec = SPECS[target]
timestamps {
    timeout(time: 8, unit: 'HOURS') {
        stage('Queue') {
            node(spec.label) {
                ws() {
                    try {
                        timeout(time: 3, unit: 'HOURS') {
                            def tmpDesc = (currentBuild.description) ? currentBuild.description + "<br>" : ""
                            currentBuild.description = tmpDesc + "<a href=${JENKINS_URL}computer/${NODE_NAME}>${NODE_NAME}</a>"
                            /*
                             * ZOS and AIX needs to to add workspace to LIBPATH. Here we add
                             * it to LIBPATH unconditionally on all platforms on the
                             * assumption that other platforms either don't use LIBPATH
                             * (e.g, Linux, macOS).
                             */
                            withEnv(spec.environment + ["LIBPATH+WORKSPACE=${WORKSPACE}", 'GTEST_COLOR=0']) {
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
                                    if (spec.ccache) {
                                        echo 'Output CCACHE stats before running and clear them'
                                        sh 'ccache -s -z'
                                    }

                                    for (build in spec.builds) {
                                        dir("${build.buildDir}") {
                                            echo 'Configure...'
                                            build.configure(this)
                                            echo 'Compile...'
                                            build.compile(this)
                                        }
                                    }

                                    if (spec.ccache) {
                                        echo 'Output CCACHE stats after running'
                                        sh 'ccache -s'
                                    }
                                }
                                stage('Test') {
                                    dir("${spec.builds.last().buildDir}") {
                                        spec.builds.last().test(this)
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
}

/*
 * Following is here to support both, old-style pipelines
 * (buildenv/jenkins/jobs/builds/* and buildenv/jenkins/jobs/pull-requests/*)
 * and new-style pipeline (omrbuild.groovy with BUILDSPEC param)
 */
if (params.BUILDSPEC != null) {
    launch(params.BUILDSPEC)
}

return this
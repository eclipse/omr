evaluate(new File('omrbuild.groovy'))

def writeJob(String filename, String target) {
    file = new File(filename)
    file.write( """
                pipeline = null;
                node {
                    ws {
                        checkout scm
                        pipeline = load "buildenv/jenkins/omrbuild.groovy"
                    }
                }
                pipeline.launch(\"${target}\")
                """.stripIndent())
}


for (target in SPECS.keySet()) {
    writeJob("jobs/builds/Build-${target}", target)
    writeJob("jobs/pull-requests/PullRequest-${target}", target)
}
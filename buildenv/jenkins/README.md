<!--
Copyright (c) 2019, 2019 IBM Corp. and others

This program and the accompanying materials are made available under
the terms of the Eclipse Public License 2.0 which accompanies this
distribution and is available at https://www.eclipse.org/legal/epl-2.0/
or the Apache License, Version 2.0 which accompanies this distribution and
is available at https://www.apache.org/licenses/LICENSE-2.0.

This Source Code may also be made available under the following
Secondary Licenses when the conditions for such availability set
forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
General Public License, version 2 with the GNU Classpath
Exception [1] and GNU General Public License, version 2 with the
OpenJDK Assembly Exception [2].

[1] https://www.gnu.org/software/classpath/license.html
[2] http://openjdk.java.net/legal/assembly-exception.html

SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
-->

[Eclipse OMR Jenkins Builds](https://ci.eclipse.org/omr/)

This folder contains Jenkins pipeline scripts that are used in the OMR Jenkins builds.

### Setting up a Jenkins job

#### Merge Builds

From the [Builds view](https://ci.eclipse.org/omr/view/Builds/), on the left menu select [New Item](https://ci.eclipse.org/omr/view/Builds/newJob). Name the job based on the following convention `Build-<SPEC>` (eg. `Build-linux_x86-64`. See [omrbuild.groovy](./omrbuild.groovy) for full `SPEC` list). Select `Pipeline` as the job type and then click `OK`. Setup the following in the job config.

1. General
	1. Discard old builds -> Max # of builds to keep: `25`
	1. GitHub project -> Project url: `https://github.com/eclipse/omr/`
	1. This project is parameterized -> Choice Parameter -> Name: `BUILDSPEC` -> Choices: `<SPEC>` (The matching SPEC name)
1. Build Triggers
	1. Generic Webhook Trigger (defaults should be fine)
1. Pipeline
	1. Definition: Pipeline script from SCM
	1. SCM: Git
		1. Repositories -> Repository URL: `https://github.com/eclipse/omr.git`
		1. Branches to build: `/refs/heads/master`
	1. Script Path: `buildenv/jenkins/omrbuild.groovy`
	1. Lightweight checkout: `true`

#### Pull Request Builds

From the [Pull Requests view](https://ci.eclipse.org/omr/view/Pull%20Requests/), on the left menu select [New Item](https://ci.eclipse.org/omr/view/Pull%20Requests/newJob). Name the job based on the following convention `PullRequest-<SPEC>` (eg. `PullRequest-linux_x86-64`. See [omrbuild.groovy](./omrbuild.groovy) for full `SPEC` list). Select `Pipeline` as the job type and then click `OK`. Setup the following in the job config.

1. General
	1. GitHub project -> Project url: `https://github.com/eclipse/omr/`
	1. This project is parameterized -> Choice Parameter -> Name: `BUILDSPEC` -> Choices: `<SPEC>` (The matching SPEC name)
1. Build Triggers
	1. GitHub Pull Request Builder
		1. Admin list: `<Committers' Github IDs>` (Should be auto filled from Global Config)
		1. Use github hooks for build triggering: `true`
		1. Trigger phrase: `.*genie-omr build.*(aix|all).*` (Replace aix with spec shorthand. Where aix is a shorthand for aix_ppc-64)
		1. White list: `<List of Github IDs allowed to launch PR builds>`
		1. Trigger Setup
			1. Add -> Update commit status during build -> Commit Status Context: `continuous-integration/eclipse-omr/pr/<SPEC>`
			1. Add -> Cancel build on update
1. Pipeline
	1. Definition: Pipeline script from SCM
	1. SCM: Git
		1. Repositories
			1. Repository URL: `https://github.com/eclipse/omr.git`
			1. Advanced -> Refspec: `+refs/pull/${ghprbPullId}/merge:refs/remotes/origin/pr/${ghprbPullId}/merge`
		1. Branches to build: `${sha1}`
		1. Additional Behaviours
			1. Clean before checkout
			1. Advanced clone behaviours
				1. Fetch tags: `false`
				1. Honor refspec on initial clone: `true`
	1. Script Path: `buildenv/jenkins/omrbuild.groovy`
	1. Lightweight checkout: `false`

#### Configuring the Github repo hooks

Eclipse admins must do this via a [Bugzilla](https://bugs.eclipse.org/bugs/enter_bug.cgi) request. Product: Community, Component: GitHub

1. Generic Webhook for Merge builds<br>
	Payload URL: `https://ci.eclipse.org/omr/generic-webhook-trigger/invoke`<br>
	Content type: `application/json`<br>
	Enable SSL verification: `true`<br>
	Which events would you like to trigger this webhook?: `Just the push event`<br>
	Active: `true`<br>

1. Github PullRequest Trigger Webhook<br>
	Payload URL: `https://ci.eclipse.org/omr/ghprbhook/`<br>
	Content type: `application/x-www-form-urlencoded`<br>
	Enable SSL verification: `true`<br>
	Which events would you like to trigger this webhook?: `Let me select individual events` -> `Issue comments`, `Pull requests`, `Pushes`<br>
	Active: `true`<br>

### OS and arch specific setup

#### Linux - x86_64

##### Installing CUDA SDK

1. Download CUDA 10.0 SDK from NVIDIA:

   ```
   wget https://developer.nvidia.com/compute/cuda/10.0/Prod/local_installers/cuda_10.0.130_410.48_linux
   ```

   **Please note** that you need CUDA 10.0 as of 2020-04-13, with newer versions port tests may fail.

1. Install the SDK into `/usr/local/cuda` (this is where CI scripts will look):

    ```
    bash cuda_10.0.130_410.48_linux --toolkit --toolkitpath=/tmp/cuda --no-drm --override --silent
    sudo mv /tmp/cuda /usr/local
    ```

#### z/OS

##### Pipeline script from SCM subtleties with Rocket Git for z/OS

Configuring Jenkins to pull the pipeline scripts from Git on z/OS is a non-trivial effort. The Rocket Git for z/OS [1] port does not support _https_ transport protocol, thus specifying the repository in the _Repository URL_ with an `https://` prefix will not work as Jenkins will encounter errors when trying to execute commands on the slave:

```
Fetching changes from the remote Git repository
Cleaning workspace
ERROR: Error fetching remote repo 'origin'
hudson.plugins.git.GitException: Failed to fetch from https://github.com/eclipse/omr.git
	at hudson.plugins.git.GitSCM.fetchFrom(GitSCM.java:894)
	at hudson.plugins.git.GitSCM.retrieveChanges(GitSCM.java:1161)
	at hudson.plugins.git.GitSCM.checkout(GitSCM.java:1192)
	at org.jenkinsci.plugins.workflow.steps.scm.SCMStep.checkout(SCMStep.java:120)
	at org.jenkinsci.plugins.workflow.steps.scm.SCMStep$StepExecutionImpl.run(SCMStep.java:90)
	at org.jenkinsci.plugins.workflow.steps.scm.SCMStep$StepExecutionImpl.run(SCMStep.java:77)
	at org.jenkinsci.plugins.workflow.steps.SynchronousNonBlockingStepExecution.lambda$start$0(SynchronousNonBlockingStepExecution.java:47)
	at java.util.concurrent.Executors$RunnableAdapter.call(Executors.java:511)
	at java.util.concurrent.FutureTask.run(FutureTask.java:266)
	at java.util.concurrent.ThreadPoolExecutor.runWorker(ThreadPoolExecutor.java:1149)
	at java.util.concurrent.ThreadPoolExecutor$Worker.run(ThreadPoolExecutor.java:624)
	at java.lang.Thread.run(Thread.java:748)
Caused by: hudson.plugins.git.GitException: Command "git fetch --tags --progress https://github.com/eclipse/omr.git +refs/heads/*:refs/remotes/origin/*" returned status code 128:
stdout: 
stderr: fatal: Unable to find remote helper for 'https'
```

Thus we must use the `git://` prefix to specify the _Repository URL_ in the Jenkins Git Plugin. This is a supported configuration [2]. However one may run into issues when using the `git://` transport protocol when attempting to launch a Jenkins build on z/OS:

```
hudson.plugins.git.GitException: Command "git fetch --tags --progress origin +refs/heads/master:refs/remotes/origin/master --prune" returned status code 128:
stdout: 
stderr: fatal: unable to connect to github.com:
github.com[0: 192.30.253.113]: errno=Connection timed out


	at org.jenkinsci.plugins.gitclient.CliGitAPIImpl.launchCommandIn(CliGitAPIImpl.java:2042)
	at org.jenkinsci.plugins.gitclient.CliGitAPIImpl.launchCommandWithCredentials(CliGitAPIImpl.java:1761)
	at org.jenkinsci.plugins.gitclient.CliGitAPIImpl.access$400(CliGitAPIImpl.java:72)
	at org.jenkinsci.plugins.gitclient.CliGitAPIImpl$1.execute(CliGitAPIImpl.java:442)
	at jenkins.plugins.git.GitSCMFileSystem$BuilderImpl.build(GitSCMFileSystem.java:351)
	at jenkins.scm.api.SCMFileSystem.of(SCMFileSystem.java:198)
	at jenkins.scm.api.SCMFileSystem.of(SCMFileSystem.java:174)
	at org.jenkinsci.plugins.workflow.cps.CpsScmFlowDefinition.create(CpsScmFlowDefinition.java:108)
	at org.jenkinsci.plugins.workflow.cps.CpsScmFlowDefinition.create(CpsScmFlowDefinition.java:67)
	at org.jenkinsci.plugins.workflow.job.WorkflowRun.run(WorkflowRun.java:293)
	at hudson.model.ResourceController.execute(ResourceController.java:97)
	at hudson.model.Executor.run(Executor.java:429)
Finished: FAILURE
```

The reason this happens is because the native `git://` transport uses TCP port 9418 which may not be open on the Jenkins master, hence why we end up timing out. To fix this there are two options:

1. Open port 9418 on the Jenkins master
2. Alias `git://` transport protocol to `https://` [3]

   This can be achieved by navigating to the master node _Script Console_ in Jenkins and executing the following Groovy script:

   ```
   println "git config --global url.https://.insteadOf git://".execute().text
   ```

[1] https://www.rocketsoftware.com/product-categories/mainframe/git-for-zos
[2] https://wiki.jenkins.io/display/JENKINS/Git+Plugin
[3] https://github.com/angular/angular-phonecat/issues/141#issuecomment-40481120
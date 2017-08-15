# Contributing to Eclipse OMR

Thanks for your interest in this project.

We welcome and enourage all kinds of contribution to the project, not only code. 
This includes bug reports, user experience feedback, assistance in reproducing
issues and more.

This project uses GitHub Issues to track ongoing development, discuss project
plans, and keep track of bugs.  Be sure to search for existing issues before
you create another one.

Visit [our Issues page on GitHub to search and submit](https://github.com/eclipse/omr/issues)

## Submitting a contribution

You can propose contributions by sending pull requests through GitHub.
Following these guidelines will help us to merge your pull requests smoothly:

1. If you're not sure your contribution would be accepted, and want to validate
   your approach or idea before writing code, feel free to open an issue. However, 
   not every feature or fix needs an issue. If the problem and fix are cleanly 
   connected, and you have the fix in hand, feel free to just submit a pull request.

2. Your pull request is an opportunity to explain both what changes you'd like
   pulled in, but also _why_ you'd like them added. Providing clarity on why
   you want changes makes it easier to accept, and provides valuable context to
   review.
 
3. Please read carefully and adhere to the legal considerations and
   copyright/license requirements outlined below.

4. Follow the coding style and format of the code you are modifying (see the
   [coding standards](doc/CodingStandard.md)). The code base is yet to be unified
   in style however, so if the file you are editing seems to have a diffferent
   style, defer to the style of the file as you found it. 

5. Follow the commit guidelines found below.

6. We encourage you to open a pull request early, and mark it as "Work In Progress"
   (prefix the PR title with WIP). This allows feedback to start early, and helps
   create a better end product. Committers will wait until after you've removed
   the WIP prefix to merge your changes. 


## Commit Guidelines

The first line describes the change made. It is written in the imperative mood,
and should say what happens when the patch is applied. Keep it short and
simple. The first line should be less than 50 characters, sentence case, and
does not end in a period. Leave a blank line between the first line and the
message body.

The body should be wrapped at 72 characters, where reasonable.

Include as much information in your commit as possible. You may want to include
designs and rationale, examples and code, or issues and next steps. Prefer
copying resources into the body of the commit over providing external links.
Structure large commit messages with headers, references etc. Remember however
that the commit message is always going to be rendered in plain text.

Use the commit footer to place commit metadata. The footer is the last block of
contiguous text in the message. It is separated from the body by one or more
blank lines, and as such cannot contain any blank lines. Lines in the footer are
of the form:

```
Key: Value
```

When a commit has related issues or commits, explain the relation in the message
body. You should also leave an `Issue` tag in the footer. For example:

```
Correct race in frobnicator 

This patch eliminates the race condition in issue #1234.

Issue #1234
```

Sign off on your commit in the footer. By doing this, you assert original
authorship of the commit and that you are permitted to contribute it. This can
be automatically added to your commit by passing `-s` to `git commit`, or by
hand adding the following line to the footer of the commit.

```
Signed-off-by: Full Name <email>
```

Remember, if a blank line is found anywhere after the `Signed-off-by` line, the
`Signed-off-by:` will be considered outside of the footer, and will fail the
automated Signed-off-by validation.

It is important that you read and understand the legal considerations found
below when signing off or contributing any commit.

### Example commits

Here is an example of a *good* commit:

```
Update and expand the commit guidelines

Elaborate on the style guidelines for commit messages. These new
style guidelines reflect the conversation found in #124.

The guidelines are changed to:
- Provide guidance on how to write a good first line.
- Elaborate on formatting requirements.
- Relax the advice on using issues for nontrivial commits.
- Move issue references from the first line to the message footer.
- Encourage contributors to put more information into the commit
  message.

Issue #124
Signed-off-by: Robert Young <rwy0717@gmail.com>
```

The first line is meaningful and imperative. The body contains enough
information that the reader understands the why and how of the commit, and its
relation to any issues. The issue is properly tagged and the commit is signed
off.

The following is a *bad* commit:

```
FIX #124: Changing a couple random things in CONTRIBUTING.md.
Also, there are some bug fixes in the thread library.
```

The commit rolls unrelated changes together in a very bad way. There is not
enough information for the commit message to be useful. The first line is not
meaningful or imperative. The message is not formatted correctly, the issue is
improperly referenced, and the commit is not signed off by the author.

### Other resources for writing good commits

- http://chris.beams.io/posts/git-commit/

### Install a git commit message hook script to validate local commits

We have a script that attempts to enforce some of these commit guidelines. You
can install it by following instructions similar to the ones below. 

```
cd omr_repo_dir
cp scripts/commit-msg .git/hooks
chmod +x .git/hooks/commit-msg
```

If the hook declines your commit, the message will remain in 
`omr_repo_dir/.git/COMMIT_EDITMSG`.

Be sure to update your version of the script occasionally as it may evolve as
our commit guidelines evolve.

## Legal considerations

Please read the [Eclipse Foundation policy on accepting contributions via Git](http://wiki.eclipse.org/Development_Resources/Contributing_via_Git).

Your contribution cannot be accepted unless you have a signed [ECA - Eclipse Foundation Contributor Agreement](http://www.eclipse.org/legal/ECA.php) in place. If you have an active signed Eclipse CLA
([the CLA was updated by the Eclipse Foundation to become the ECA in August 2016](https://mmilinkov.wordpress.com/2016/08/15/contributor-agreement-update/)),
then that signed CLA is sufficient. You will have to sign the ECA once your CLA expires.

Here is the checklist for contributions to be _acceptable_:

1. [Create an account at Eclipse](https://dev.eclipse.org/site_login/createaccount.php).
2. Add your GitHub user name in your account settings.
3. [Log into the project's portal](https://projects.eclipse.org/) and sign the ["Eclipse ECA"](https://projects.eclipse.org/user/sign/cla).
4. Ensure that you [_sign-off_](https://wiki.eclipse.org/Development_Resources/Contributing_via_Git#Signing_off_on_a_commit) your Git commits.
5. Ensure that you use the _same_ email address as your Eclipse account in commits.
6. Include the appropriate copyright notice and license at the top of each file.

Your signing of the ECA will be verified by a webservice called 'ip-validation'
that checks the email address that signed-off on your commits has signed the
ECA. **Note**: This service is case-sensitive, so ensure the email that signed
the ECA and that signed-off on your commits is the same, down to the case. 

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

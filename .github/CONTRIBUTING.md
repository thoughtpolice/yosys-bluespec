# How to Contribute

You're probably reading this using **[GitHub]**, so for the sake of brevity,
this document only covers the most technical aspects of contribution: tailored
to someone who is interacting with the code directly.

[GitHub]: https://github.com

## Opening issues

Occasionally, you might find a bug. That's bad, but there's good news: if you
report what went wrong, one day, maybe, *if* you're lucky, it might get fixed.
But how do you report that information?

* Make sure you have a **[GitHub Account](https://github.com/signup/free)**.
* Then, **[submit an issue](https://github.com/thoughtpolice/yosys-bsv/issues)** to
  the bug tracker - assuming one does not already exist.
  * Clearly describe the issue including steps to reproduce when it is a bug.
  * Include the git repository information, any changes, Nix version, etc.

## Submitting patches

Finally, you may be willing to actually write some code yourself and give it to
us. Maybe there's a simple spelling mistake. Or we forgot an obvious feature.
Perhaps you'd like to submit lots of technical debt that will ruin the project
forever. All of those are welcome and very realistic outcomes!

> **NOTE**: writing patches and contributing code to `yosys-bsv` implies licensing
> those contributions under the terms of **[LICENSE.txt](../LICENSE.txt)**
> (ISC)

There are a couple rules you need to keep in mind before writing source code
patches and submitting them. If it's your first time, don't sweat the details
&mdash; a developer or contributor can help you figure out the specifics.

> **PRO-TIP**: We use the key words described in **[RFC 2119]** to signify
> requirements for patches, design documents, etc. This RFC is very useful to
> keep in mind: if you've never read or heard of it, go read it right now! This
> document will be here when you get back.

[RFC 2119]: https://tools.ietf.org/html/rfc2119

* You **MUST** always run all tests. (Note that the CI system does this for you;
  but please do so yourself!)
* You **SHOULD** keep changes logically separated: one-commit-per-thing (more on
  that below).
* For large structural changes, you **SHOULD NOT** add lots of "superfluous
  commits" for typos, trivial compile errors, etc. You **MUST** rebase these
  changes away. (Smaller, independent patches to fix errors *after* the fact are
  fine, however.)
* The repository **MUST** compile successfully, and **MUST** run all tests
  successfully, *on every single commit to the repository*. This includes every
  individual commit that you author and submit to us for inclusion.

  This allows very useful tools like `git bisect` to work over time.

Regarding point #4 (and #5): for multi-commit requests, your code may get
squashed into the smallest possible logical changes and commiting with author
attribution in some cases.  In general, try to keep the history clean of things
like "fix typo" and "obvious build break fix", and this won't normally be
necessary.  Patches with these kinds of changes in the same series will
generally be rejected, but ask if you're unsure.

### Writing semantic commit messages

The system for writing commit messages is inspired by **[Conventional
Commits]**, which is a system describing how every project commit should be
written and prepared. Following this convention helps tooling keep up with
changes, fixes, etc automatically, and is very important to follow. (Mistakes
happen though, and you shouldn't sweat that!)

[Conventional Commits]: https://www.conventionalcommits.org/

The format is easy to describe in general. Every commit shall look like the
following, which stresses every relevant feature of the format:

```text
feat!(engine/render): add wobbly hats

Wobbling is an attractive feature for hats to have. Look at this hat animation:
it's wobbly as hell. This will make it much easier to sell hats. And that, in
turn, will help DarkNet users who are trying to launder money through Online Hat
Sale Networks(TM).

But *why* do we want to help people launder money online? Well, the answer may surprise you...

BREAKING CHANGE: The deprecated `bugfest` API has been REMOVED. Use the `shiny`
API instead. An ungodly programming horror mandated this removal, and `bugfest`
had to die. The reasons why are far too detailed to explain here. See GH-1234
below for more information on that.

Closes-Issue: GH-1234

Acked-by: Rachel Doe <rachel@example.com>
Authored-by: John Kansas <john@example.com>
Signed-off-by: Austin Seipp <austin@example.com>
Signed-off-by: Rachel Doe <rachel@example.com>
Signed-off-by: John Kansas <john@example.com>
```

Let's break this down

```text
feat: add hat wobble
^--^  ^------------^
|     |
|     +-> Summary in present tense.
|
+-------> Type: chore, docs, feat, fix, refactor, style, or test.
```

Some examples of various **commit types** you can use are:

* `feat`: (new feature for the user, not a new feature for build script)
* `fix`: (bug fix for the user, not a fix to a build script)
* `docs`: (changes to the documentation)
* `style`: (formatting, missing semi colons, etc; no production code change)
* `refactor`: (refactoring production code, eg. renaming a variable)
* `test`: (adding missing tests, refactoring tests; no production code change)
* `chore`: (updating grunt tasks etc; no production code change)

### Prose and messaging

In addition to writing properly formatted commit messages that follow the above
convention, it's important to include relevant information so other developers
can later understand *why* a change was made: the prose is important.

You can, many times, discern exactly why a change was made by following history,
context, etc. While this context usually can be found by digging code, mailing
list/Discourse archives, pull request discussions or upstream changes, it may
require a lot of work to dig all that up.

### Notes on sign-offs, attributions, etc

When you commit, **you must use `-s` to add a Signed-off-by line**.
`Signed-off-by` is interpreted as a statement of ownership, much like Git
itself: by adding it, you make clear that the contributed code abides by the
project license, and you are rightfully contributing it yourself or on behalf of
someone. You should always do this.

This means that if the patch you submit was authored by someone else -- perhaps
a coworker for example that you submit it from or you revive a patch that
someone forgot about a long time ago and resubmit it -- you should also include
their name in the details if possible.

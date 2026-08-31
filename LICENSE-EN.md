# The licence in plain words

**Русская версия: [LICENSE-RU.md](LICENSE-RU.md).**

Only two files have legal force: [`LICENSE`](LICENSE), the verbatim text of
**BSD 2-Clause** covering our code, and
[`LICENSE.upstream`](LICENSE.upstream), the verbatim **ISC** notice covering the
code inherited from cwm. This file adds nothing and takes nothing away; it
explains what we want, and it is not part of the licence agreement. Which file
covers what is set out in [`NOTICE`](NOTICE).

## What you may do

Everything. Take it, change it, put it in a closed product, sell it, package it
for a distribution. The only condition is to keep the licence texts and the
copyright notices: in copies of the source, and in the documentation of binary
builds.

You need not ask permission. You need not report anything. You are not obliged
to send changes back.

## Why the tree holds three licences

digitwm is not a window manager written from nothing. It is a fork of cwm from
the OpenBSD base system, and we do not hide it: the whole upstream history is in
this repository.

- **BSD-2-Clause** — our files: the ribbon, the probe, the FTS harnesses, the
  session, the tools, the documentation.
- **ISC** — 25 files inherited from cwm. We are not entitled to change their
  licence: that is the condition on which we received the code, not caution on
  our part.
- **BSD-3-Clause** — one file, `queue.h`, which is `sys/queue.h` from OpenBSD.

Three permissive licences, compatible with one another. The built `cwm` carries
all three notices at once, and whoever redistributes it keeps all three.

Our lines are in upstream's files too: 135 of 442 in `screen.c`, 211 of 837 in
`calmwm.h`. Those files stay under ISC and we touched not one of their headers.
Changing the identifier there would misstate the provenance of a file that is
three quarters someone else's.

## What we ask

A window manager lives by being installed: a pkgsrc port, a Homebrew formula, a
session entry in a login manager. So the licence here is permissive, and it will
stay that way.

But there is a difference between a person who put digitwm on their laptop and a
company that ships it inside a product or a distribution. Of the second we ask
one thing: **share.** Patches, bug reports, money, a mention — whatever you think
right.

And one request on its own: **send fixes to the inherited files to cwm as well.**
We try not to close that path, and we ask you not to close it either.

This is a request, not an obligation. We deliberately do not turn it into a
licence condition, and here is why.

## Why a request and not a condition

"Large users pay" can be written down — such licences exist (PolyForm Small
Business, BSL, FSL). But a licence with a revenue threshold stops being open by
the OSI definition, and that means:

- the `pkgsrc/wm/digitwm` port would not be accepted into the pkgsrc tree, and
  NetBSD — the reason Xlib was pulled out of the ribbon — becomes unreachable;
- Homebrew and the Linux and BSD distributions are closed for the same reason;
- corporate lawyers block unknown and unapproved licences by default;
- and above all, the path of patches back to cwm closes for good: code under a
  non-free licence will not be taken into OpenBSD.

The last one settles it. A window manager that cannot be packaged and from which
nothing can be returned upstream is not a fork but a dead end.

## How to support us

[digitable.life](https://digitable.life)

## History of the licence

Earlier versions of our code were distributed under **ISC** — inherited from cwm
rather than chosen. A single `LICENSE` described everything at once, and while
our own code was small, that was honest.

The change to BSD 2-Clause concerns **our files only** and applies **forwards**.
Anyone who received the code under ISC keeps their rights under it for good: a
licence once granted cannot be revoked, and we are not trying to. The inherited
files did not change licence at all — they were ISC and they remain ISC, and
`LICENSE.upstream` keeps their notice verbatim, with all nine copyright lines.

There are two reasons for the change, and both are dull.

First: BSD 2-Clause is the licence Digitable publishes its code under generally.
One licence per organisation means one answer to "what do you publish under", and
someone who takes two of our projects does not have to read two different texts.

Second: BSD 2-Clause describes what actually happens here more precisely. ISC was
written for one author and one file — its disclaimer speaks of "THE AUTHOR" in
the singular and says nothing about binary distribution. digitwm is handed out
built, through package managers, and it has more than one author; BSD 2-Clause
names both source and binary redistribution explicitly, and disclaims on behalf
of "COPYRIGHT HOLDERS AND CONTRIBUTORS" in the plural.

Both licences are permissive and OSI-approved. Nothing that was allowed
yesterday is forbidden today.

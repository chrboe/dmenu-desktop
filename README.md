# dmenu-desktop

## What is this?
Essentially a wrapper around [`dmenu`](https://tools.suckless.org/dmenu/) that scans the usual directories to find all [desktop entry](https://standards.freedesktop.org/desktop-entry-spec/latest/) files on your system and displays the names conveniently in `dmenu`.

## Is this usable at all?

No, consider this a 0.0.1-pre-alpha release.

## Goals

Implement a cache so it's nice and (even) quick(er)

## What about i3-dmenu-desktop?

Users of the i3 window manager might be familiar with a tool called `i3-dmenu-desktop`, which does the exact same thing as this tool. What gives?

Basically, the issue is that `i3-dmenu-desktop` is implemented in Perl and therefore dog slow (well, it's not *that* slow, but there is some noticeable delay when opening the menu).

Also, I have big plans to implement a cache so we don't have to parse every single desktop file on the system every time we want to open something (I don't have a lot of desktop files myself, but I imagine this is painfully slow on a system with hundreds of entries).

## Legal stuff

Licensed under GPLv3 (see [LICENSE](LICENSE))

Copyright © 2018 Christoph Böhmwalder

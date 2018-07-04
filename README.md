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

For now, here's a performance comparison between the original script and my
version (each program was modified to exit right before invoking `dmenu`):

```shell
$ perf stat -r 100 ./i3-dmenu-desktop

 Performance counter stats for './i3-dmenu-desktop' (100 runs):

         96,204035      task-clock:u (msec)       #    0,997 CPUs utilized            ( +-  0,60% )
                 0      context-switches:u        #    0,000 K/sec
                 0      cpu-migrations:u          #    0,000 K/sec
             2.548      page-faults:u             #    0,026 M/sec                    ( +-  0,01% )
       281.620.346      cycles:u                  #    2,927 GHz                      ( +-  0,37% )
       195.288.237      stalled-cycles-frontend:u #   69,34% frontend cycles idle     ( +-  0,96% )
       148.689.022      stalled-cycles-backend:u  #   52,80% backend cycles idle      ( +-  1,15% )
       213.939.889      instructions:u            #    0,76  insn per cycle
                                                  #    0,91  stalled cycles per insn  ( +-  0,00% )
        47.426.232      branches:u                #  492,976 M/sec                    ( +-  0,00% )
         2.911.890      branch-misses:u           #    6,14% of all branches          ( +-  0,15% )

       0,096449183 seconds time elapsed                                          ( +-  0,61% )


$ perf stat -r 100 ./dmenu-desktop

 Performance counter stats for './dmenu-desktop' (100 runs):

          0,873389      task-clock:u (msec)       #    0,849 CPUs utilized            ( +-  0,68% )
                 0      context-switches:u        #    0,000 K/sec
                 0      cpu-migrations:u          #    0,000 K/sec
                60      page-faults:u             #    0,069 M/sec                    ( +-  0,30% )
         1.712.357      cycles:u                  #    1,961 GHz                      ( +-  0,79% )
         1.825.015      stalled-cycles-frontend:u #  106,58% frontend cycles idle     ( +-  1,04% )
         1.636.406      stalled-cycles-backend:u  #   95,56% backend cycles idle      ( +-  0,79% )
         2.007.594      instructions:u            #    1,17  insn per cycle
                                                  #    0,91  stalled cycles per insn  ( +-  0,00% )
           438.594      branches:u                #  502,175 M/sec                    ( +-  0,00% )
            17.965      branch-misses:u           #    4,10% of all branches          ( +-  1,83% )

       0,001029115 seconds time elapsed                                          ( +-  0,66% )
```

That's a cool two orders of magnitude worth of improvement.

## Legal stuff

Licensed under GPLv3 (see [LICENSE](LICENSE))

Copyright © 2018 Christoph Böhmwalder

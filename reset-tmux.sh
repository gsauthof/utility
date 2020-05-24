#!/bin/bash

# Reset a garbled tmux session, e.g. after some random bytes
# were accidentally sent to stdout


# source: https://unix.stackexchange.com/a/253369/1131

stty sane
printf '\033k%s\033\\\033]2;%s\007' "`basename "$SHELL"`" "`uname -n`"
tput reset
tmux refresh

tmux list-windows -a | while IFS=: read -r a b c; do
    tmux set-window-option -t "$a:$b" automatic-rename on
done

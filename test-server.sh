#!/bin/sh

AGENT1="$1"
AGENT2="$2"
shift 2

./hex-server -a $AGENT1 -ua 1001 -b $AGENT2 -ub 1002 -t 16 $@

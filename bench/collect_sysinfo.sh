#!/bin/bash
EZPID=$(pgrep -x ezNet)
echo "PID: $EZPID"
echo "---CPU/MEM---"
ps -p $EZPID -o pcpu,pmem,vsz,rss,cmd 2>/dev/null
echo "---FD---"
ls /proc/$EZPID/fd 2>/dev/null | wc -l
echo "---Memory---"
cat /proc/$EZPID/status 2>/dev/null | grep -E 'VmRSS|VmSize|VmPeak|Threads'
echo "---Kernel---"
uname -r

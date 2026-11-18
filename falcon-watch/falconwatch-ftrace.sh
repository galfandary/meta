#!/bin/sh
# falconwatch-ftrace.sh - catch SIGKILL senders with the kernel's own
# tracing. No bpftrace, no perf, nothing to install. Root only.
#
#   start   enable the tracepoints in a private tracing instance
#   dump    print what the ring buffer caught
#   stop    disable the events and remove the instance
#
# Same two roads as the bpftrace version: the kill-family syscall
# tracepoints name a userspace sender exactly; signal_generate fires
# for every SIGKILL the kernel generates, kernel-sent ones included,
# and its code field says which road it was (0=SI_USER, 128=SI_KERNEL).

T=/sys/kernel/tracing
[ -d "$T" ] || T=/sys/kernel/debug/tracing
I=$T/instances/falcon
E=$I/events
EVENTS="signal/signal_generate syscalls/sys_enter_kill syscalls/sys_enter_tgkill syscalls/sys_enter_tkill"

case "$1" in
start)
	mkdir -p "$I"
	echo 0 > "$I/tracing_on"
	echo 2048 > "$I/buffer_size_kb"
	for e in $EVENTS; do
		echo 'sig == 9' > "$E/$e/filter"
		echo 1 > "$E/$e/enable"
	done
	echo 1 > "$I/tracing_on"
	echo "falconwatch: on ($I)";;
dump)
	cat "$I/trace";;
stop)
	echo 0 > "$I/tracing_on"
	for e in $EVENTS; do echo 0 > "$E/$e/enable"; done
	rmdir "$I"
	echo "falconwatch: off";;
*)
	echo "usage: $0 start|dump|stop" >&2; exit 2;;
esac

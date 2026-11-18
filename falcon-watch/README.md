# falcon-watch — catch Falcon in the act

## No bpftrace? Use the ftrace version (zero install)

`falconwatch-ftrace.sh` drives the kernel's own tracing directly —
bpftrace is only a front-end, and everything it would attach to is
already in the kernel. It enables the same tracepoints inside a
private tracing instance (`instances/falcon`), so it doesn't touch
the main trace buffer or anyone else's tracing.

```sh
sudo install -m 755 falconwatch-ftrace.sh /usr/local/sbin/
sudo falconwatch-ftrace.sh start     # then reproduce a kill
sudo falconwatch-ftrace.sh dump      # read what it caught
sudo falconwatch-ftrace.sh stop
```

Or at every boot, via `falcon-watch-ftrace.service`
(`systemctl enable --now falcon-watch-ftrace`). Live view instead of
dump: `cat /sys/kernel/tracing/instances/falcon/trace_pipe`.

Reading its output is the same as below: a `sys_enter_kill` line names
the userspace sender; a `signal_generate` line with code 128
(SI_KERNEL) and no syscall line at that second is a kernel-side kill.
The audit rules further down are unchanged and still complementary —
they're the layer that survives reboots and lands in the permanent
log.

---

Two independent layers, because a kill can travel two different roads:

1. **Userspace road** — a process calls `kill`/`tgkill`/`tkill`.
   The audit rules (or layer 1 of the bpftrace script) record the
   sender's exe, pid and login uid. Attribution is exact.
2. **Kernel road** — the signal is generated inside the kernel, e.g. a
   sensor component using the eBPF `bpf_send_signal` helper. No syscall
   ever happens, so syscall tracing sees nothing. The
   `signal:signal_generate` tracepoint (layer 2 of the bpftrace script)
   fires anyway.

## Quick test (no install)

Run it in a terminal, then reproduce a kill:

```sh
sudo bpftrace falconwatch.bt
```

Needs the `bpftrace` package (RHEL 9 AppStream). Zero-install
fallback for the same tracepoint: `sudo perf record -e signal:signal_generate -a`
then `perf script`.

## Install as a service

```sh
sudo install -D -m 644 falconwatch.bt /usr/local/share/falcon-watch/falconwatch.bt
sudo install -m 644 falcon-watch.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable --now falcon-watch
tail -f /var/log/falcon-watch.log
```

## Install the audit layer (optional, complementary)

```sh
sudo cp falcon-kill.rules /etc/audit/rules.d/
sudo augenrules --load
ausearch -k sigkill -i        # after a kill
```

If audit is in immutable mode (`-e 2`), the new rules load only after
a reboot.

## Reading the log

- A `KILL`/`TGKILL` line at the moment your process died names the
  sender outright. Falcon's agent runs as a `falcon-sensor` /
  `cs.falcon` process — if that comm appears, case closed.
- A `SIGKILL generated` line with **si_code=128 (SI_KERNEL)** and *no*
  matching syscall line at the same second means the kill came from
  kernel context. On the `signal_generate` line, comm/pid then show
  the task context the kernel-side sender ran in, which is itself a
  tell — but the hard fact is the SI_KERNEL code plus the timestamp.
- si_code=0 (SI_USER) always pairs with a syscall line; si_code=-6
  (SI_TKILL) is the tgkill/tkill family.
- Negative controls, as before: the OOM killer is a different animal —
  it writes a `Killed process` line to dmesg and bumps `oom_kill` in
  the cgroup's `memory.events`. Neither moving means it wasn't OOM.
- Expect background noise: systemd and cron legitimately SIGKILL
  things. Correlate by timestamp with the death you're investigating,
  and by target pid where the syscall lines give one.

## Caveats

- Audit logs its own rule changes; a managed sensor may notice new
  audit rules or a bpftrace service. Nothing here hides. It's
  observation, not evasion.
- The bpftrace script was written against the standard tracepoint
  field names (`sys_enter_kill` args `pid`/`sig`, `tgkill`
  `tgid`/`tid`/`sig`, `signal_generate` `sig`/`code`/`comm`/`pid`).
  If your kernel's field layout differs, `bpftrace -l 'tracepoint:signal:*'`
  and `cat /sys/kernel/tracing/events/signal/signal_generate/format`
  show the local truth.

# meta

Files Archer prepares for Guy — the copies that used to go to his work email
as attachments, kept here instead so he can read them at work and see the
full history of every change.

## Contents

- `tracer/` — the ptrace dependency tracer sources (tracer.c for RHEL 8+9,
  tracer9.c for RHEL 9, tracerS.c the strace-engine variant) and build.sh.
  Shipping build: the original tracer compiled and stripped with
  `strip --strip-all`.
- `falcon-watch/` — the kit for catching the Falcon sensor in the act
  (bpftrace and zero-install ftrace variants, systemd units, audit rules),
  plus the unsent Falcon exception-request draft.

# Falcon allowlist exception request — dependency tracer

Draft prepared Sun Nov 15, 2026, evening. Not sent anywhere.
Fill in the bracketed fields from the work machine, then send to the
bank's Falcon/security team. Only needed as insurance: the
`strip --strip-all` build already passes Falcon, so this is a fallback
against a future build getting flagged again.

## Fill in first (work machine)

```sh
sha256sum /path/to/tracer
```

- SHA-256: `[paste output here]`
- Binary path on the work host: `[e.g. /path/to/tracer]`
- Host / host group: `[hostname and Falcon host group]`

Do NOT use any hash from the home workspace: the local copy in
`~/workspace/tracer/tracer` is unstripped (26,584 bytes, `file` reports
"not stripped") and is not the build Falcon tested.

## Ticket / email draft

**Subject:** Allowlist exception request — internal dependency tracer (ptrace, strip-all build)

> Hi,
>
> Please allowlist the internal tool below in Falcon IOC Management
> (SHA-256, action Allow, Linux, host group above).
>
> **What it is:** a dependency tracer I wrote for image builds,
> invoked as `trace <file> <command> [args...]`. It runs a command
> under ptrace and records every file the command depends on, so a
> minimal image containing just those files runs the command
> identically.
>
> **Why it looks the way it does:** it uses ptrace by design, and it
> is built with `strip --strip-all`. An earlier partially-stripped
> build was killed by the sensor; the current strip-all build runs
> clean. I am asking for the exception so a future rebuild is not
> killed the same way.
>
> **Binary**
> - SHA-256: `[hash]`
> - Path: `[path]`
> - Host / host group: `[host / group]`
>
> **Question:** a hash exception dies on every rebuild, since the
> hash changes with the bytes. Is a path/process-pattern (IOA)
> exclusion possible instead, or do you prefer hash-only and I
> re-ticket on each rebuild?
>
> I can supply a falcon-watch log (audit + tracepoint capture of any
> SIGKILL sender) if a kill recurs. Happy to freeze this exact binary
> if hash-only is the route.
>
> Thanks,
> Guy Alfandary

## Notes

- The allowlist lives in CrowdStrike's cloud console (IOC Management),
  administered by the bank's Falcon team — root on the endpoint
  cannot set it locally.
- If the team grants hash-only, freeze that exact binary; any rebuild
  is a new hash and a new ticket.
- If a kill recurs before the exception lands, run the falcon-watch
  kit first (`falconwatch-ftrace.sh start`, reproduce, `dump`) and
  attach the log — timestamped, sender-attributed evidence.

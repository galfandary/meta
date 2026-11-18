#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <sys/ptrace.h>
#include <sys/user.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/uio.h>
#include <sys/syscall.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdint.h>
typedef struct { char **t; int m, u; } S;
static uint64_t djb2(const char *s) {
	auto h = 5381ULL;
	while (*s) h += (h << 5) + (uint8_t)*s++;
	return h;
}
static int has(S *s, const char *k) {
	if (!s->m) return 0;
	auto i = djb2(k) & (s->m - 1);
	while (s->t[i]) { if (!strcmp(s->t[i], k)) return 1; i = (i + 1) & (s->m - 1); }
	return 0;
}
static void ins(S *s, char *k) {
	if (!s->m || (s->u + 1) * 10 >= s->m * 7) {
		int m = s->m ? 2 * s->m : 1024;
		auto t = (char **)calloc(m, sizeof *s->t);
		for (int i = 0; i < s->m; i++) if (s->t[i]) {
			auto j = djb2(s->t[i]) & (m - 1);
			while (t[j]) j = (j + 1) & (m - 1);
			t[j] = s->t[i];
		}
		free(s->t); s->t = t; s->m = m;
	}
	auto i = djb2(k) & (s->m - 1);
	while (s->t[i]) i = (i + 1) & (s->m - 1);
	s->t[i] = k;
	s->u++;
}

static char **V;
static int N, M;

static S VS;

static void add(const char *s) {
	if (!s || !*s || has(&VS, s)) return;
	if (N == M) { M = M ? 2 * M : 256; V = (char **)realloc(V, M * sizeof *V); }
	V[N] = strdup(s);
	ins(&VS, V[N]);
	N++;
}
static int cf(const void *a, const void *b) { return strcmp(*(char *const *)a, *(char *const *)b); }

static char **D;
static int DN, DM;

static void def(const char *s) {
	if (DN == DM) { DM = DM ? 2 * DM : 256; D = (char **)realloc(D, DM * sizeof *D); }
	D[DN++] = strdup(s);
}

static char *lk(const char *p) {
	char b[PATH_MAX];
	auto n = readlink(p, b, sizeof b - 1);
	if (n < 0) return NULL;
	b[n] = 0;
	return strdup(b);
}

static S XS;

static int xin(const char *s) { return s && has(&XS, s); }
static void xadd(const char *s) {
	if (!s || !*s || has(&XS, s)) return;
	ins(&XS, strdup(s));
}

static int NR;

static void put(const char *s) {
	if (!s || !*s || NR || xin(s)) return;
	struct stat z;
	if (lstat(s, &z) || S_ISREG(z.st_mode) || S_ISLNK(z.st_mode)) add(s);
}

static char *jn(const char *a, const char *b) {
	auto x = strlen(a);
	auto y = strlen(b);
	auto r = (char *)malloc(x + y + 2);
	if (!x || (x == 1 && a[0] == '/')) { r[0] = '/'; strcpy(r + 1, b); }
	else sprintf(r, "%s/%s", a, b);
	return r;
}

static int hp;

static char *res2(const char *a, int fin) {
	auto cp = strdup(a);
	auto sv = (char *)NULL;
	auto cur = strdup("");
	for (auto tok = strtok_r(cp, "/", &sv); tok; tok = strtok_r(NULL, "/", &sv)) {
		if (!strcmp(tok, ".")) continue;
		if (!strcmp(tok, "..")) {
			auto sl = strrchr(cur, '/');
			if (sl) *sl = 0;
			continue;
		}
		int last = !*sv;
		auto obj = jn(cur, tok);
		struct stat z;
		if (lstat(obj, &z) || !S_ISLNK(z.st_mode) || (last && !fin)) {
			free(cur);
			cur = obj;
			continue;
		}
		put(obj);
		if (++hp > 40) { free(cur); cur = obj; break; }
		auto tg = lk(obj);
		if (!tg) { free(cur); cur = obj; break; }
		char *na;
		if (tg[0] == '/') na = jn(tg, sv);
		else {
			auto t1 = jn(cur[0] ? cur : "/", tg);
			na = jn(t1, sv);
			free(t1);
		}
		free(cur); free(cp); free(obj); free(tg);
		auto r = res2(na, fin);
		free(na);
		return r;
	}
	free(cp);
	if (!cur[0]) { free(cur); cur = strdup("/"); }
	return cur;
}

static char *res(const char *a, int fin) {
	hp = 0;
	return res2(a, fin);
}

static void xadd2(const char *s) {
	if (!s) return;
	xadd(s);
	NR = 1;
	auto o = res(s, 0);
	NR = 0;
	if (o) { xadd(o); free(o); }
}

static void rec1(char *a) {
	if (!a) return;
	auto d = strstr(a, " (deleted)");
	if (d && !d[10]) *d = 0;
	auto o = res(a, 0);
	if (!o) { free(a); return; }
	struct stat z;
	if (lstat(o, &z)) {
		if (!xin(o) && !xin(a)) add(o);
		free(o); free(a);
		return;
	}
	if (S_ISREG(z.st_mode)) put(o);
	else if (S_ISLNK(z.st_mode)) {
		put(o);
		auto t = res(a, 1);
		if (t) {
			struct stat tz;
			if (strcmp(t, o) && !lstat(t, &tz)) put(t);
			free(t);
		}
	}
	free(o);
	free(a);
}

static char *abs2(pid_t p, int df, const char *s) {
	if (!s || !*s) return NULL;
	if (s[0] == '/') return strdup(s);
	char b[PATH_MAX];
	if (df == AT_FDCWD) snprintf(b, sizeof b, "/proc/%d/cwd", p);
	else snprintf(b, sizeof b, "/proc/%d/fd/%d", p, df);
	auto dir = lk(b);
	if (!dir) return NULL;
	snprintf(b, sizeof b, "%s/%s", dir, s);
	free(dir);
	return strdup(b);
}

static void rec(pid_t p, int df, const char *s) {
	auto a = abs2(p, df, s);
	if (a) rec1(a);
}

static void bang(const char *path) {
	int f = open(path, O_RDONLY);
	if (f < 0) return;
	char b[256];
	auto n = read(f, b, sizeof b - 1);
	close(f);
	if (n < 3 || b[0] != '#' || b[1] != '!') return;
	b[n] = 0;
	auto s = b + 2;
	while (*s == ' ' || *s == '\t') s++;
	auto e = s;
	while (*e && *e != ' ' && *e != '\t' && *e != '\n' && *e != '\r') e++;
	*e = 0;
	if (*s == '/') rec1(strdup(s));
}

static char *rd(pid_t p, uint64_t a) {
	static char b[PATH_MAX];
	auto t = sizeof b - 1;
	auto h = 4096 - (a & 4095);
	if (h > t) h = t;
	struct iovec l = { b, t }, r[2] = { { (void *)a, h }, { (void *)(a + h), t - h } };
	auto n = process_vm_readv(p, &l, 1, r, t > h ? 2 : 1, 0);
	if (n <= 0) return NULL;
	b[n] = 0;
	return b;
}

static uint64_t rdf(pid_t p, uint64_t a) {
	auto v = 0ULL;
	struct iovec l = { &v, 8 }, r = { (void *)a, 8 };
	if (process_vm_readv(p, &l, 1, &r, 1, 0) != 8) return 0;
	return v;
}

static int rdm(pid_t p, uint64_t a, void *d, size_t n) {
	struct iovec l = { d, n }, r = { (void *)a, n };
	return process_vm_readv(p, &l, 1, &r, 1, 0) == (ssize_t)n;
}

static uint64_t base(pid_t p, uint64_t sp) {
	auto a = sp;
	auto ac = rdf(p, a);
	auto ph = a - a;
	if (!ac || ac > 100000) return 0;
	a += 8 * (ac + 2);
	int n = 0;
	while (rdf(p, a)) { a += 8; if (++n > 100000) return 0; }
	a += 8;
	for (n = 0; n < 1000; n++) {
		auto t = rdf(p, a);
		auto v = rdf(p, a + 8);
		if (!t) break;
		if (t == 3) ph = v;
		a += 16;
	}
	if (!ph) return 0;
	for (uint64_t c = ph & ~4095ULL; ph - c <= 65536; c -= 4096) {
		uint8_t h[64];
		if (rdm(p, c, h, sizeof h) && !memcmp(h, "\177ELF", 4) && h[4] == 2) {
			uint64_t po;
			memcpy(&po, h + 32, 8);
			if (c + po == ph) return c;
		}
		if (c < 4096) break;
	}
	return 0;
}

static void interp(pid_t p, uint64_t base) {
	if (!base) return;
	uint8_t h[64];
	if (!rdm(p, base, h, sizeof h) || memcmp(h, "\177ELF", 4) || h[4] != 2) return;
	uint64_t po, bias = 0, ia = 0;
	uint16_t pn;
	memcpy(&po, h + 32, 8);
	memcpy(&pn, h + 56, 2);
	if (pn == 0xffff) return;
	for (int i = 0; i < pn; i++) {
		uint8_t q[56];
		uint32_t t;
		uint64_t off, va;
		if (!rdm(p, base + po + i * sizeof q, q, sizeof q)) return;
		memcpy(&t, q, 4);
		memcpy(&off, q + 8, 8);
		memcpy(&va, q + 16, 8);
		if (t == 1 && !off) bias = base - va;
		else if (t == 3) ia = va;
	}
	if (!ia) return;
	auto s = rd(p, bias + ia);
	if (s) rec1(strdup(s));
}

typedef struct Q Q;
struct Q { pid_t p; int sn, nr, df, cr, wo, tf, td; char *s, *e, *t; Q *x; };
static Q *H;

static Q *get(pid_t p, int mk) {
	for (Q *q = H; q; q = q->x) if (q->p == p) return q;
	if (!mk) return NULL;
	Q *q = (Q *)calloc(1, sizeof *q);
	q->p = p;
	q->nr = -1;
	q->df = AT_FDCWD;
	q->x = H;
	H = q;
	return q;
}
static void del(pid_t p) {
	for (Q **q = &H; *q; q = &(*q)->x)
		if ((*q)->p == p) {
			Q *t = *q;
			*q = t->x;
			free(t->s); free(t->e); free(t->t); free(t);
			return;
		}
}

static void ent(Q *q, pid_t p, long n, const uint64_t *a) {
	int d = AT_FDCWD, x = 0;
	uint64_t pa;
	q->nr = (int)n;
	q->cr = q->wo = q->tf = 0;
	free(q->t);
	q->t = NULL;
	switch (n) {
	case SYS_open: case SYS_creat:
	case SYS_rename: case SYS_link: case SYS_symlink: case SYS_mknod:
		pa = a[0]; break;
	case SYS_openat:
	case SYS_openat2:
	case SYS_renameat:
	case SYS_renameat2:
	case SYS_linkat: case SYS_mknodat:
		d = (int)a[0]; pa = a[1]; break;
	case SYS_symlinkat:
		pa = a[0]; break;
	case SYS_execve: pa = a[0]; x = 1; break;
	case SYS_execveat: d = (int)a[0]; pa = a[1]; x = 1; break;
	default: return;
	}
	auto s = rd(p, pa);
	if (x) {
		free(q->e);
		q->e = s ? abs2(p, d, s) : NULL;
		q->nr = -1;
		return;
	}
	if (!s) { q->nr = -1; return; }
	free(q->s); q->s = strdup(s); q->df = d;
	if (n == SYS_open || n == SYS_openat || n == SYS_creat
	    || n == SYS_openat2
	   ) {
		uint64_t fl;
		if (n == SYS_open) fl = a[1];
		else if (n == SYS_openat) fl = a[2];
		else if (n == SYS_creat) fl = O_WRONLY | O_CREAT | O_TRUNC;
		else fl = rdf(p, a[2]);
		q->wo = (fl & O_ACCMODE) == O_WRONLY && (fl & O_CREAT);
		if (fl & O_CREAT) {
			auto ap = abs2(p, d, s);
			if (ap) {
				struct stat z;
				q->cr = lstat(ap, &z) && errno == ENOENT;
				free(ap);
			}
		}
	}
	switch (n) {
	case SYS_rename: case SYS_link: case SYS_symlink: case SYS_mknod:
	case SYS_renameat:
	case SYS_renameat2:
	case SYS_linkat: case SYS_symlinkat: case SYS_mknodat: {
		auto td = AT_FDCWD;
		uint64_t tp;
		switch (n) {
		case SYS_rename: case SYS_link: case SYS_symlink: tp = a[1]; break;
		case SYS_mknod: tp = a[0]; break;
		case SYS_renameat:
		case SYS_renameat2:
		case SYS_linkat: td = (int)a[2]; tp = a[3]; break;
		case SYS_symlinkat: td = (int)a[1]; tp = a[2]; break;
		default: td = (int)a[0]; tp = a[1]; break;
		}
		auto ts = rd(p, tp);
		if (ts) {
			char *ap = abs2(p, td, ts);
			if (ap) {
				struct stat z;
				q->tf = !lstat(ap, &z) || errno != ENOENT;
				q->t = ap;
				q->td = td;
			}
		}
		if (n == SYS_symlink || n == SYS_symlinkat || n == SYS_mknod || n == SYS_mknodat) {
			free(q->s);
			q->s = q->t ? strdup(q->t) : NULL;
			if (!q->s) q->nr = -1;
		}
		break;
	}
	}
}

static void ext(Q *q, pid_t p, long r) {
	if (!q->s) return;
	int ok;
	switch (q->nr) {
	case SYS_open: case SYS_creat:
	case SYS_openat2:
	case SYS_openat: ok = r >= 0; break;
	default: ok = r == 0; break;
	}
	if (ok) {
		switch (q->nr) {
		case SYS_open: case SYS_creat:
		case SYS_openat2:
		case SYS_openat: {
			auto ap = abs2(p, q->df, q->s);
			if (ap) {
				if (q->cr) xadd2(ap);
				if (!q->cr && !q->wo) rec(p, q->df, q->s);
				free(ap);
			}
			break;
		}
		case SYS_rename: case SYS_link:
		case SYS_renameat:
		case SYS_renameat2:
		case SYS_linkat:
			if (q->t) {
				auto sp = abs2(p, q->df, q->s);
				int sc = 0;
				if (sp) {
					sc = xin(sp);
					if (!sc) {
						NR = 1;
						auto so = res(sp, 0);
						NR = 0;
						if (so) { sc = xin(so); free(so); }
					}
					if (!sc) rec(p, q->df, q->s);
					free(sp);
				}
				if (!q->tf || sc) xadd2(q->t);
			}
			break;
		case SYS_symlink: case SYS_symlinkat: case SYS_mknod: case SYS_mknodat:
			if (q->t && !q->tf) xadd2(q->t);
			break;
		default: rec(p, q->df, q->s); break;
		}
	}
	free(q->s);
	q->s = NULL;
	free(q->t);
	q->t = NULL;
	q->nr = -1;
}

int main(int C, char **A) {
	if (C < 3) { fprintf(stderr, "usage: %s file cmd [arg...]\n", A[0]); return 2; }
	auto p = fork();
	if (p < 0) { perror("fork"); return 1; }
	if (!p) {
		ptrace(PTRACE_TRACEME, 0, 0, 0);
		raise(SIGSTOP);
		execvp(A[2], A + 2);
		perror(A[2]);
		_exit(127);
	}
	signal(SIGINT, SIG_IGN);
	signal(SIGQUIT, SIG_IGN);
	int st, ex = 0, ms = 0;
	waitpid(p, &st, 0);
	ptrace(PTRACE_SETOPTIONS, p, 0,
	       PTRACE_O_TRACESYSGOOD | PTRACE_O_TRACEFORK | PTRACE_O_TRACEVFORK |
	       PTRACE_O_TRACECLONE | PTRACE_O_TRACEEXEC | PTRACE_O_EXITKILL);
	get(p, 1)->sn = 1;
	ptrace(PTRACE_SYSCALL, p, 0, 0);
	for (;;) {
		auto w = waitpid(-1, &st, __WALL);
		if (w < 0) { if (errno == EINTR) continue; break; }
		if (WIFEXITED(st) || WIFSIGNALED(st)) {
			if (w == p) {
				if (WIFEXITED(st)) ex = WEXITSTATUS(st);
				else { ms = WTERMSIG(st); ex = 128 + ms; }
			}
			del(w);
			continue;
		}
		int sig = WSTOPSIG(st);
		auto ev = (uint32_t)st >> 16;
		Q *q = get(w, 1);
		long inj = 0;
		if (sig == (SIGTRAP | 0x80)) {
			struct ptrace_syscall_info I;
			if (ptrace(PTRACE_GET_SYSCALL_INFO, w, (void *)sizeof I, &I) >= 0) {
				if (I.op == PTRACE_SYSCALL_INFO_ENTRY) ent(q, w, I.entry.nr, (uint64_t *)I.entry.args);
				else if (I.op == PTRACE_SYSCALL_INFO_EXIT) ext(q, w, I.exit.rval);
			}
		} else if (sig == SIGTRAP && ev) {
			if (ev == PTRACE_EVENT_FORK || ev == PTRACE_EVENT_VFORK || ev == PTRACE_EVENT_CLONE) {
				uint64_t np;
				ptrace(PTRACE_GETEVENTMSG, w, 0, &np);
				get((pid_t)np, 1);
			} else if (ev == PTRACE_EVENT_EXEC) {
				auto ot = (uint64_t)w;
				ptrace(PTRACE_GETEVENTMSG, w, 0, &ot);
				Q *eq = (pid_t)ot != w ? get((pid_t)ot, 0) : q;
				if (eq && eq->e) { rec1(strdup(eq->e)); def(eq->e); }
				if (eq && eq != q) del((pid_t)ot);
				free(q->e); q->e = NULL;
				free(q->s); q->s = NULL;
				free(q->t); q->t = NULL;
				q->nr = -1;
				char b[64];
				snprintf(b, sizeof b, "/proc/%d/exe", w);
				auto e = lk(b);
				if (e) rec1(strdup(e));
				struct user_regs_struct R;
				interp(w, ptrace(PTRACE_GETREGS, w, 0, &R) >= 0 ? base(w, (uint64_t)R.rsp) : 0);
				free(e);
			}
		} else {
			if (!q->sn) q->sn = 1;
			else inj = sig;
		}
		ptrace(PTRACE_SYSCALL, w, 0, (void *)inj);
	}
	if (DN) {
		qsort(D, DN, sizeof *D, cf);
		for (int i = 0; i < DN; i++) if (!i || strcmp(D[i], D[i - 1])) bang(D[i]);
	}
	auto f = fopen(A[1], "w");
	if (!f) { perror(A[1]); return 1; }
	qsort(V, N, sizeof *V, cf);
	for (int i = 0; i < N; i++) {
		if (i && !strcmp(V[i], V[i - 1])) continue;
		fprintf(f, "%s\n", V[i]);
	}
	fclose(f);
	if (ms) { signal(ms, SIG_DFL); raise(ms); }
	return ex;
}

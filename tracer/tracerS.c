#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include <stdint.h>
#include <time.h>
#include <elf.h>
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

static S SN, IS;

static void sadd(S *s, const char *k) {
    if (k && *k && !has(s, k)) ins(s, strdup(k));
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
    if (*s == '/') { rec1(strdup(s)); sadd(&IS, s); }
}

static void pti(const char *f) {
    auto fd = open(f, O_RDONLY);
    if (fd < 0) return;
    Elf64_Ehdr h;
    if (pread(fd, &h, sizeof h, 0) != (ssize_t)sizeof h || memcmp(h.e_ident, ELFMAG, 4) || h.e_ident[EI_CLASS] != ELFCLASS64 || h.e_phnum == 0xffff) { close(fd); return; }
    for (int i = 0; i < h.e_phnum; i++) {
        Elf64_Phdr q;
        if (pread(fd, &q, sizeof q, h.e_phoff + (off_t)i * sizeof q) != (ssize_t)sizeof q) break;
        if (q.p_type == PT_INTERP) {
            auto b = (char *)malloc(q.p_filesz + 1);
            if (pread(fd, b, q.p_filesz, (off_t)q.p_offset) == (ssize_t)q.p_filesz) { b[q.p_filesz] = 0; rec1(strdup(b)); }
            free(b);
            break;
        }
    }
    close(fd);
}

typedef struct Q Q;
struct Q { pid_t p; char *pd, *cw; Q *x; };
static Q *H;
static pid_t MP;
static char *LC;
static struct timespec T0;
static int EX, MS;

static Q *pq(pid_t p, int mk) {
    for (Q *q = H; q; q = q->x) if (q->p == p) return q;
    if (!mk) return NULL;
    Q *q = (Q *)calloc(1, sizeof *q);
    q->p = p;
    q->x = H;
    H = q;
    return q;
}

static char *span(const char *a, const char *b) {
    while (a < b && (*a == ' ' || *a == '\t')) a++;
    while (b > a && (b[-1] == ' ' || b[-1] == '\t' || b[-1] == '\n')) b--;
    auto r = (char *)malloc(b - a + 1);
    memcpy(r, a, b - a);
    r[b - a] = 0;
    return r;
}

static char *cwdof(pid_t p) {
    Q *q = pq(p, 1);
    if (q->cw) return strdup(q->cw);
    char b[64];
    snprintf(b, sizeof b, "/proc/%d/cwd", (int)p);
    auto d = lk(b);
    if (!d) d = strdup(LC ? LC : "/");
    free(q->cw);
    q->cw = strdup(d);
    return d;
}

static char *dres(pid_t p, const char *tk) {
    if (tk) {
        auto l = strchr(tk, '<');
        if (l) {
            auto r = strrchr(tk, '>');
            if (r > l) return span(l + 1, r);
        }
        if (strcmp(tk, "AT_FDCWD")) {
            char *e;
            auto f = strtol(tk, &e, 10);
            if (e != tk && f > 0) {
                char b[64];
                snprintf(b, sizeof b, "/proc/%d/fd/%ld", (int)p, f);
                auto d = lk(b);
                if (d) return d;
            }
        }
    }
    return cwdof(p);
}

static char *abso(char *dir, const char *s) {
    if (!s) return NULL;
    if (s[0] == '/') return strdup(s);
    if (!dir) return strdup(s);
    return jn(dir, s);
}

static int born(const char *ap) {
    struct statx x;
    if (statx(AT_FDCWD, ap, 0, STATX_BTIME, &x) || !(x.stx_mask & STATX_BTIME)) return -1;
    if (x.stx_btime.tv_sec != T0.tv_sec) return x.stx_btime.tv_sec > T0.tv_sec;
    return x.stx_btime.tv_nsec >= T0.tv_nsec;
}

static int created(const char *ap) {
    auto b = born(ap);
    return b >= 0 ? b : !has(&SN, ap);
}

static int existed(const char *ap) {
    return has(&SN, ap) || born(ap) == 0;
}

static char *nq(char **pp) {
    auto s = strchr(*pp, '"');
    if (!s) return NULL;
    s++;
    auto b = (char *)malloc(strlen(s) + 1);
    auto o = b;
    while (*s && *s != '"') {
        if (*s == '\\' && s[1]) {
            s++;
            if (*s == 'x') {
                unsigned v = 0;
                int n = 0;
                while (n < 2 && isxdigit((uint8_t)s[1])) { v = v * 16 + (isdigit((uint8_t)s[1]) ? s[1] - '0' : tolower((uint8_t)s[1]) - 'a' + 10); s++; n++; }
                *o++ = (char)v;
            } else if (*s >= '0' && *s <= '7') {
                unsigned v = 0;
                int n = 0;
                while (n < 3 && *s >= '0' && *s <= '7') { v = v * 8 + (*s - '0'); s++; n++; }
                *o++ = (char)v;
                s--;
            } else *o++ = *s == 'n' ? '\n' : *s == 't' ? '\t' : *s == 'r' ? '\r' : *s;
            s++;
        } else *o++ = *s++;
    }
    *o = 0;
    *pp = *s == '"' ? s + 1 : s;
    return b;
}

static int resof(char *tx, long *rv, char **rp) {
    for (auto e = tx; (e = strstr(e, " = ")); e += 3) {
        auto v = e + 3;
        if (*v == '?' || *v == '-' || isdigit((uint8_t)*v)) {
            if (*v == '?' ) continue;
            char *t;
            auto n = strtol(v, &t, 10);
            if (t == v) continue;
            *rv = n;
            if (rp) *rp = e;
            return 1;
        }
    }
    return 0;
}

static void ev(pid_t p, const char *nm, char *tx) {
    long rv;
    char *rp = NULL;
    if (!resof(tx, &rv, &rp)) return;
    int opn = !strcmp(nm, "open") || !strcmp(nm, "openat") || !strcmp(nm, "openat2") || !strcmp(nm, "creat");
    int exn = !strcmp(nm, "execve") || !strcmp(nm, "execveat");
    int rnn = !strcmp(nm, "rename") || !strcmp(nm, "renameat") || !strcmp(nm, "renameat2") || !strcmp(nm, "link") || !strcmp(nm, "linkat");
    int crn = !strcmp(nm, "symlink") || !strcmp(nm, "symlinkat") || !strcmp(nm, "mknod") || !strcmp(nm, "mknodat");
    if (!opn && !exn && !rnn && !crn) return;
    auto op = strchr(tx, '(');
    if (!op) return;
    char *q[2] = { 0, 0 }, *e0 = NULL;
    {
        auto t = tx;
        q[0] = nq(&t);
        e0 = t;
        if (q[0] && (rnn || crn)) q[1] = nq(&t);
    }
    if (!q[0]) return;
    int at = strstr(nm, "at") && strcmp(nm, "creat");
    char *d1 = NULL, *d2 = NULL;
    if (at && strcmp(nm, "symlinkat")) {
        auto cm = strchr(op + 1, ',');
        auto lim = strchr(op + 1, '"');
        auto tk = span(op + 1, cm && (!lim || cm < lim) ? cm : lim ? lim : op + strlen(op));
        d1 = dres(p, tk);
        free(tk);
    }
    if (rnn || !strcmp(nm, "symlinkat")) {
        if (at) {
            auto cm = e0 ? strchr(e0, ',') : NULL;
            auto lim = e0 ? strchr(e0, '"') : NULL;
            if (cm) {
                auto tk = span(cm + 1, lim ? lim : cm + strlen(cm));
                d2 = dres(p, tk);
                free(tk);
            } else d2 = cwdof(p);
        }
    }
    if (exn && rv == 0) {
        auto dir = d1 ? d1 : cwdof(p);
        auto ap = abso(dir, q[0]);
        if (q[0][0]) {
            rec1(strdup(ap));
            def(ap);
            sadd(&IS, ap);
            sadd(&SN, ap);
        }
        char b[64];
        snprintf(b, sizeof b, "/proc/%d/exe", (int)p);
        auto e = lk(b);
        if (e) { rec1(strdup(e)); sadd(&IS, e); free(e); }
        free(ap);
    } else if (opn && rv >= 0) {
        auto dir = d1 ? d1 : cwdof(p);
        auto ap = abso(dir, q[0]);
        int cr = !strcmp(nm, "creat"), wo = cr;
        if (!cr) {
            auto fl = e0 && rp && rp > e0 ? span(e0, rp) : NULL;
            cr = fl && strstr(fl, "O_CREAT") != NULL;
            wo = cr && fl && strstr(fl, "O_WRONLY") && !strstr(fl, "O_RDWR");
            free(fl);
        }
        if (cr) {
            if (created(ap)) xadd2(ap);
            else if (!wo) rec1(strdup(ap));
        } else rec1(strdup(ap));
        sadd(&SN, ap);
        free(ap);
    } else if (rnn && rv == 0) {
        auto sdir = d1 ? d1 : cwdof(p);
        auto sp = abso(sdir, q[0]);
        auto tp = q[1] ? abso(d2 ? d2 : sdir, q[1]) : NULL;
        if (tp) {
            int sc = xin(sp);
            if (!sc) {
                NR = 1;
                auto so = res(sp, 0);
                NR = 0;
                if (so) { sc = xin(so); free(so); }
            }
            if (!sc) rec1(strdup(sp));
            if (!existed(tp) || sc) xadd2(tp);
            sadd(&SN, sp);
            sadd(&SN, tp);
            free(tp);
        }
        free(sp);
    } else if (crn && rv == 0) {
        auto tp = q[1] ? abso(d2 ? d2 : d1 ? d1 : cwdof(p), q[1]) : abso(d1 ? d1 : cwdof(p), q[0]);
        if (tp) {
            if (!existed(tp)) xadd2(tp);
            sadd(&SN, tp);
            free(tp);
        }
    }
    free(d1); free(d2);
    free(q[0]); free(q[1]);
}

static void line(char *ln) {
    char *e;
    auto p = (pid_t)strtol(ln, &e, 10);
    if (e == ln) return;
    if (!MP) MP = p;
    auto s = e;
    while (*s == ' ') s++;
    auto ac = strstr(s, "AT_FDCWD<");
    if (ac) {
        auto a2 = ac + 9;
        auto b2 = strchr(a2, '>');
        if (b2) {
            auto d = span(a2, b2);
            Q *q = pq(p, 1);
            free(q->cw); q->cw = d;
            free(LC); LC = strdup(d);
        }
    }
    if (s[0] == '-' && s[1] == '-') return;
    Q *q = pq(p, 1);
    if (s[0] == '+') {
        if (p == MP) {
            auto x = strstr(s, "exited with ");
            auto k = strstr(s, "killed by SIG");
            if (x) { EX = atoi(x + 12); MS = 0; }
            else if (k) {
                k += 13;
                char nb[32];
                int i = 0;
                while (k[i] && k[i] != ' ' && i < 31) { nb[i] = k[i]; i++; }
                nb[i] = 0;
                for (int sg = 1; sg < 65; sg++) {
                    auto ab = sigabbrev_np(sg);
                    if (ab && !strcmp(nb, ab)) { MS = sg; EX = 128 + sg; break; }
                }
            }
        }
        free(q->pd); q->pd = NULL;
        return;
    }
    if (s[0] == '<') {
        auto r = strstr(s, "resumed>");
        if (!r) { free(q->pd); q->pd = NULL; return; }
        auto nb = s + 5;
        auto ne = strstr(nb, " resumed>");
        char nm[32];
        if (!ne || ne - nb > 31) { free(q->pd); q->pd = NULL; return; }
        memcpy(nm, nb, ne - nb);
        nm[ne - nb] = 0;
        if (q->pd) {
            auto tail = r + 8;
            auto full = (char *)malloc(strlen(q->pd) + strlen(tail) + 1);
            strcpy(full, q->pd);
            strcat(full, tail);
            free(q->pd); q->pd = NULL;
            ev(p, nm, full);
            free(full);
        }
        return;
    }
    auto uf = strstr(s, "<unfinished ...>");
    if (uf) {
        *uf = 0;
        free(q->pd);
        q->pd = strdup(s);
        return;
    }
    auto op = strchr(s, '(');
    if (!op || op - s > 31) return;
    char nm[32];
    memcpy(nm, s, op - s);
    nm[op - s] = 0;
    ev(p, nm, s);
}

int main(int C, char **A) {
    if (C < 3) { fprintf(stderr, "usage: %s file cmd [arg...]\n", A[0]); return 2; }
    clock_gettime(CLOCK_REALTIME, &T0);
    EX = 1;
    int pf[2];
    if (pipe(pf)) { perror("pipe"); return 1; }
    char ob[32];
    snprintf(ob, sizeof ob, "/dev/fd/%d", pf[1]);
    auto sp = fork();
    if (sp < 0) { perror("fork"); return 1; }
    if (!sp) {
        close(pf[0]);
        char **S = (char **)malloc(sizeof(char *) * (C + 12));
        int n = 0;
        S[n++] = (char *)"strace";
        S[n++] = (char *)"-f";
        S[n++] = (char *)"-y";
        S[n++] = (char *)"-s";
        S[n++] = (char *)"4096";
        S[n++] = (char *)"-e";
        S[n++] = (char *)"trace=open,openat,openat2,creat,execve,execveat,rename,renameat,renameat2,link,linkat,symlink,symlinkat,mknod,mknodat";
        S[n++] = (char *)"-o";
        S[n++] = ob;
        S[n++] = (char *)"--";
        for (int i = 2; i < C; i++) S[n++] = A[i];
        S[n] = NULL;
        execvp(S[0], S);
        perror("strace");
        _exit(1);
    }
    close(pf[1]);
    signal(SIGINT, SIG_IGN);
    signal(SIGQUIT, SIG_IGN);
    auto fp = fdopen(pf[0], "r");
    char *ln = NULL;
    size_t cap = 0;
    while (getline(&ln, &cap, fp) > 0) line(ln);
    free(ln);
    fclose(fp);
    waitpid(sp, NULL, 0);
    if (!MP) { EX = 127; MS = 0; }
    if (DN) {
        qsort(D, DN, sizeof *D, cf);
        for (int i = 0; i < DN; i++) if (!i || strcmp(D[i], D[i - 1])) bang(D[i]);
    }
    if (IS.m) for (int i = 0; i < IS.m; i++) if (IS.t[i]) pti(IS.t[i]);
    auto f = fopen(A[1], "w");
    if (!f) { perror(A[1]); return 1; }
    qsort(V, N, sizeof *V, cf);
    for (int i = 0; i < N; i++) {
        if (i && !strcmp(V[i], V[i - 1])) continue;
        fprintf(f, "%s\n", V[i]);
    }
    fclose(f);
    if (MS) { signal(MS, SIG_DFL); raise(MS); }
    return EX;
}

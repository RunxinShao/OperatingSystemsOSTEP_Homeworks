#!/usr/bin/env python3
"""Draw the Chapter 29 graphs from data/*.csv (median of the trials).

usage: plot.py      (writes data/*.png)
"""
import csv
import statistics
from collections import defaultdict

import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt

INK, MUTED, GRID = '#1a1a1a', '#6b6b6b', '#e4e4e4'
COLORS = ['#2a78d6', '#eb6834', '#1baf7a', '#eda100', '#e87ba4']
MARKERS = ['o', 's', '^', 'D', 'v']
STYLES = ['-', '--', ':', '-.', '-']
D = 'data'

plt.rcParams.update({'font.size': 9, 'axes.edgecolor': MUTED, 'axes.labelcolor': INK,
                     'xtick.color': MUTED, 'ytick.color': MUTED, 'axes.titlesize': 10})


def median_by(path, keys, val, where=None):
    """{tuple(keys): median(val)} over rows that satisfy where(row)."""
    groups = defaultdict(list)
    for r in csv.DictReader(open(path)):
        if where and not where(r):
            continue
        groups[tuple(r[k] for k in keys)].append(float(r[val]))
    return {k: statistics.median(v) for k, v in groups.items()}


def style(ax, xlabel, ylabel, title=None):
    ax.grid(True, color=GRID, linewidth=0.8)
    ax.set_axisbelow(True)
    for s in ('top', 'right'):
        ax.spines[s].set_visible(False)
    ax.set_xlabel(xlabel)
    ax.set_ylabel(ylabel)
    if title:
        ax.set_title(title, color=INK, loc='left')


def line(ax, xs, ys, i, label):
    ax.plot(xs, ys, color=COLORS[i], marker=MARKERS[i], linestyle=STYLES[i],
            linewidth=2, markersize=5, label=label)


def thread_ticks(ax, ticks=(1, 2, 4, 8, 12, 16)):
    ax.set_xticks(list(ticks))


def series(m, prefix, xidx):
    """From {(..., x): y} pick entries starting with prefix; return sorted xs, ys."""
    pts = sorted((float(k[xidx]), v) for k, v in m.items() if k[:len(prefix)] == prefix)
    return [p[0] for p in pts], [p[1] for p in pts]


# ---- Q2: precise counter ---------------------------------------------------
m = median_by(f'{D}/counter.csv', ['mode', 'threads'], 'seconds')
fig, ax = plt.subplots(figsize=(6.2, 3.6))
for i, (mode, lab) in enumerate([('mutex', 'pthread mutex'), ('spin', 'test-and-set spin lock'),
                                 ('atomic', 'atomic add (no lock)'),
                                 ('none', 'no synchronization (wrong total)')]):
    xs, ys = series(m, (mode,), 1)
    line(ax, xs, ys, i, lab)
ax.set_yscale('log')
style(ax, 'threads (1M increments each)', 'seconds (log scale)',
      'Q2: one shared counter, total time for all increments')
thread_ticks(ax, (1, 2, 4, 6, 8, 12, 16, 18))
ax.legend(frameon=False)
fig.tight_layout()
fig.savefig(f'{D}/counter.png', dpi=200)

m = median_by(f'{D}/counter-cpus.csv', ['cpus', 'mode', 'threads'], 'seconds')
fig, axes = plt.subplots(1, 2, figsize=(7.5, 3.4), sharey=True)
for ax, mode in zip(axes, ['mutex', 'spin']):
    for i, c in enumerate(['1', '2', '4', '18']):
        xs, ys = series(m, (c, mode), 2)
        line(ax, xs, ys, i, f'{c} CPU' + ('s' if c != '1' else ''))
    ax.set_yscale('log')
    style(ax, 'threads', 'seconds (log scale)' if mode == 'mutex' else '',
          f'{"pthread mutex" if mode == "mutex" else "spin lock"}, CPUs limited by taskset')
    thread_ticks(ax, (1, 2, 4, 8, 16))
axes[0].legend(frameon=False)
fig.tight_layout()
fig.savefig(f'{D}/counter-cpus.png', dpi=200)

# ---- Q3: approximate counter ----------------------------------------------
prec = median_by(f'{D}/counter.csv', ['mode', 'threads'], 'seconds')
ap = median_by(f'{D}/approx-threads.csv', ['layout', 'threads'], 'seconds')
th = median_by(f'{D}/approx-threshold.csv', ['threads', 'threshold'], 'seconds')
fig, axes = plt.subplots(1, 2, figsize=(7.8, 3.5))
ax = axes[0]
xs, ys = series(prec, ('mutex',), 1)
line(ax, xs, ys, 0, 'precise (one mutex)')
xs, ys = series(ap, ('padded',), 1)
line(ax, xs, ys, 1, 'approximate, S=1024')
xs, ys = series(ap, ('packed',), 1)
line(ax, xs, ys, 2, 'approximate, locals packed\n(false sharing)')
ax.set_yscale('log')
style(ax, 'threads (1M increments each)', 'seconds (log scale)', '(a) scaling with threads')
thread_ticks(ax, (1, 2, 4, 8, 12, 16, 18))
ax.legend(frameon=False, fontsize=7.5, loc='center right', bbox_to_anchor=(1, 0.38))
ax = axes[1]
for i, t in enumerate(['4', '16']):
    xs, ys = series(th, (t,), 1)
    line(ax, xs, ys, i + 3, f'{t} threads')
ax.set_xscale('log', base=2)
ax.set_yscale('log')
style(ax, 'threshold S (log scale)', 'seconds (log scale)', '(b) effect of the threshold')
ax.legend(frameon=False)
fig.tight_layout()
fig.savefig(f'{D}/approx.png', dpi=200)

# ---- Q4: linked list -------------------------------------------------------
lt = median_by(f'{D}/list-threads.csv', ['mode', 'threads'], 'mops')
lw = median_by(f'{D}/list-work.csv', ['mode', 'threads', 'work'], 'mops')
fig, axes = plt.subplots(1, 2, figsize=(7.8, 3.5))
ax = axes[0]
for i, (mode, lab) in enumerate([('coarse', 'one lock'), ('hoh', 'hand-over-hand')]):
    xs, ys = series(lt, (mode,), 1)
    line(ax, xs, ys, i, lab)
style(ax, 'threads', 'million ops / s', '(a) ~500 nodes, 80% lookups, no extra work')
ax.set_ylim(bottom=0)
thread_ticks(ax)
ax.legend(frameon=False)
ax = axes[1]
works = sorted({int(k[2]) for k in lw})
pos = list(range(len(works)))
for i, t in enumerate(['1', '8', '16']):
    ratio = [lw[('hoh', t, str(w))] / lw[('coarse', t, str(w))] for w in works]
    line(ax, pos, ratio, i + 2, f'{t} thread' + ('s' if t != '1' else ''))
ax.axhline(1, color=MUTED, linewidth=1)
ax.set_xticks(pos, [str(w) for w in works])
style(ax, 'busy-loop iterations per node visited',
      'hand-over-hand / one-lock throughput',
      '(b) above 1 = hand-over-hand wins')
ax.legend(frameon=False)
fig.tight_layout()
fig.savefig(f'{D}/list.png', dpi=200)

# ---- Q5/Q6: B-tree ---------------------------------------------------------
bt = median_by(f'{D}/btree.csv', ['lookup_pct', 'mode', 'threads'], 'mops')
fig, axes = plt.subplots(1, 3, figsize=(9, 3.4), sharey=True)
labels = [('mutex', 'one mutex (Q5)'), ('rwlock', 'one rwlock'),
          ('couple', 'lock coupling'), ('optimistic', 'optimistic coupling'),
          ('partitioned', '64 partitions')]
for ax, rp in zip(axes, ['100', '90', '50']):
    for i, (mode, lab) in enumerate(labels):
        xs, ys = series(bt, (rp, mode), 2)
        line(ax, xs, ys, i, lab)
    style(ax, 'threads', 'million ops / s' if rp == '100' else '', f'{rp}% lookups')
    thread_ticks(ax, (1, 2, 4, 8, 16))
    ax.set_ylim(bottom=0)
axes[0].legend(frameon=False, fontsize=8)
fig.tight_layout()
fig.savefig(f'{D}/btree.png', dpi=200)
print('wrote', D + '/*.png')

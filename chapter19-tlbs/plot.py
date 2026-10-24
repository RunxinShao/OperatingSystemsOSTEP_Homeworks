#!/usr/bin/env python3
"""Q4: plot ns per access against number of pages (median of the 3 runs)."""
import statistics
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt

def load(mode):
    runs = [dict(tuple(map(float, l.split())) for l in open(f'data/{mode}-{r}.dat'))
            for r in (1, 2, 3)]
    pages = sorted(runs[0])
    return pages, [statistics.median(r[p] for r in runs) for p in pages]

SERIES = [('chase',  'dependent, spread (-s -d)', '#2a78d6', 'o', '-'),
          ('stride', 'independent, offset 0 (default)', '#eb6834', 's', '--'),
          ('spread', 'independent, spread (-s)', '#1baf7a', '^', ':')]
INK, MUTED, GRID = '#1a1a1a', '#6b6b6b', '#e4e4e4'

fig, ax = plt.subplots(figsize=(7.5, 4.2), dpi=200)
for mode, label, color, marker, ls in SERIES:
    x, y = load(mode)
    ax.plot(x, y, color=color, lw=2, ls=ls, marker=marker, ms=5, label=label,
            markeredgecolor='white', markeredgewidth=0.8)
for xv, text in [(160, 'L1 DTLB\n~128-256'), (3072, 'L2 TLB\n~3072')]:
    ax.axvline(xv, color=MUTED, lw=0.8, ls=(0, (2, 3)), zorder=0)
    ax.text(xv * 1.08, 23.5, text, color=MUTED, fontsize=8, va='top')
ax.set_xscale('log', base=2)
ax.set_xticks([2 ** k for k in range(0, 16, 1)])
ax.set_xticklabels([str(2 ** k) for k in range(0, 16, 1)], fontsize=7.5)
ax.set_ylim(0, 25)
ax.set_xlabel('Number of pages touched', color=INK)
ax.set_ylabel('Time per access (ns)', color=INK)
ax.set_title('TLB size measurement (aarch64 VM, 4 KB pages, pinned to CPU 0)',
             color=INK, fontsize=10, loc='left')
ax.grid(axis='y', color=GRID, lw=0.8)
for s in ('top', 'right'):
    ax.spines[s].set_visible(False)
for s in ('left', 'bottom'):
    ax.spines[s].set_color(MUTED)
ax.tick_params(colors=MUTED)
ax.legend(frameon=False, fontsize=8, loc='center left', bbox_to_anchor=(0.0, 0.62),
          labelcolor=INK)
fig.tight_layout()
fig.savefig('tlb.png')
print('wrote tlb.png')

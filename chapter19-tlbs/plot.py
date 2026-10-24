#!/usr/bin/env python3
"""Q4: plot ns per access against number of pages (median of the 3 runs).

usage: plot.py data/LABEL [data/LABEL ...]

Writes data/LABEL/tlb.png for each machine (all three access modes), and,
given more than one machine, tlb-machines.png comparing their -s -d curves.
"""
import os
import re
import statistics
import sys

import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt

INK, MUTED, GRID = '#1a1a1a', '#6b6b6b', '#e4e4e4'
COLORS = ['#2a78d6', '#eb6834', '#1baf7a', '#eda100']
MARKERS = ['o', 's', '^', 'D']
STYLES = ['-', '--', ':', '-.']
MODES = [('chase',  'dependent, spread (-s -d)'),
         ('stride', 'independent, offset 0 (default)'),
         ('spread', 'independent, spread (-s)')]


def load(d, mode):
    runs = [dict(tuple(map(float, l.split())) for l in open(f'{d}/{mode}-{r}.dat'))
            for r in (1, 2, 3)]
    pages = sorted(runs[0])
    return pages, [statistics.median(r[p] for r in runs) for p in pages]


def describe(d):
    """Short machine description from machine.txt: (label, page size in KB)."""
    text = open(f'{d}/machine.txt').read()
    label = re.search(r'^label: (.*)$', text, re.M).group(1)
    page = int(re.search(r'^page size: (\d+)', text, re.M).group(1)) // 1024
    return label, page


def style(ax, title, top):
    ax.set_xscale('log', base=2)
    ticks = [2 ** k for k in range(16)]
    ax.set_xticks(ticks)
    ax.set_xticklabels([str(t) for t in ticks], fontsize=7.5)
    ax.set_ylim(0, top)
    ax.set_xlabel('Number of pages touched', color=INK)
    ax.set_ylabel('Time per access (ns)', color=INK)
    ax.set_title(title, color=INK, fontsize=10, loc='left')
    ax.grid(axis='y', color=GRID, lw=0.8)
    for s in ('top', 'right'):
        ax.spines[s].set_visible(False)
    for s in ('left', 'bottom'):
        ax.spines[s].set_color(MUTED)
    ax.tick_params(colors=MUTED)
    ax.legend(frameon=False, fontsize=8, loc='upper left', labelcolor=INK)


def line(ax, x, y, i, label):
    ax.plot(x, y, color=COLORS[i], lw=2, ls=STYLES[i], marker=MARKERS[i], ms=5,
            label=label, markeredgecolor='white', markeredgewidth=0.8)


def one_machine(d):
    label, page = describe(d)
    fig, ax = plt.subplots(figsize=(7.5, 4.2), dpi=200)
    top = 0
    for i, (mode, text) in enumerate(MODES):
        x, y = load(d, mode)
        line(ax, x, y, i, text)
        top = max(top, max(y))
    style(ax, f'{label}: {page} KB pages', top * 1.15)
    fig.tight_layout()
    fig.savefig(f'{d}/tlb.png')
    print('wrote', f'{d}/tlb.png')


def compare(dirs):
    fig, ax = plt.subplots(figsize=(7.5, 4.2), dpi=200)
    top = 0
    for i, d in enumerate(dirs):
        label, page = describe(d)
        x, y = load(d, 'chase')
        line(ax, x, y, i, f'{label} ({page} KB pages)')
        top = max(top, max(y))
    style(ax, 'Dependent accesses (-s -d) on each machine', top * 1.15)
    fig.tight_layout()
    fig.savefig('tlb-machines.png')
    print('wrote tlb-machines.png')


if __name__ == '__main__':
    dirs = [d.rstrip('/') for d in sys.argv[1:]] or \
        sorted(f'data/{d}' for d in os.listdir('data')
               if os.path.exists(f'data/{d}/machine.txt'))
    for d in dirs:
        one_machine(d)
    if len(dirs) > 1:
        compare(dirs)

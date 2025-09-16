#!/usr/bin/env python3
import matplotlib.pyplot as plt
import numpy as np
import yaml


def load_result(f):
    y = yaml.safe_load(f)
    benches = []
    libs = []
    table = []
    first = True
    for bench, d in y.items():
        benches.append(bench)
        keys = []
        vals = []
        for k, v in d.items():
            if k.endswith("_profile"):
                continue
            keys.append(k)
            vals.append(v["elapsed_time_ms"])
        if first:
            libs = keys
        elif libs != keys:
            raise ValueError("Inconcistent libs")
        table.append(vals)
    return libs, benches, np.transpose(table)


def plot_data(filename, libs, benches, table):
    dpi = 96
    fig, ax = plt.subplots(figsize=(1024/dpi, 600/dpi))
    plt.rcParams["font.family"] = ["M PLUS Code Latin 60"]
    plt.style.use("tableau-colorblind10")

    yticks = np.arange(len(benches))
    ax.set_yticks(yticks, benches, fontfamily="M PLUS Code Latin 60")
    ax.invert_yaxis()
    ax.grid(True, axis="x", linestyle="dashed")

    groupheight = 0.75
    for i, lib in enumerate(libs):
        barcenter = (i - 0.5 * (len(libs) - 1)) * groupheight / len(libs)
        barheight = groupheight / len(libs)
        ax.barh(yticks + barcenter, table[i], height=barheight, label=lib)

    ax.legend()
    fig.tight_layout()
    plt.savefig(filename, dpi=dpi)


def main():
    import matplotlib as mpl
    mpl.use("Agg")

    with open("bench_result.yml") as f:
        libs, benches, table = load_result(f)
        plot_data("bench_result.png", libs, benches, table)


if __name__ == "__main__":
    main()

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
            raise ValueError("Inconsistent libs")
        table.append(vals)
    return libs, benches, np.transpose(table)


def plot_data(filename, libs, benches, table, compiler):
    dpi = 96
    fig, ax = plt.subplots(figsize=(1024/dpi, 600/dpi))
    plt.rcParams["font.family"] = ["M PLUS 2"]
    plt.xticks(fontfamily="M PLUS 2")

    ax.set_ymargin(0.02)
    ax.set_title(compiler, fontfamily="M PLUS 2")
    ax.set_xlabel("Elapsed time (ms, shorter is better)", fontfamily="M PLUS 2")

    yticks = np.arange(len(benches))
    ax.set_yticks(yticks, benches, fontfamily="M PLUS Code Latin 60")
    ax.invert_yaxis()
    ax.grid(True, axis="x", linestyle="dashed")

    tab20c = plt.color_sequences["tab20c"]
    groupheight = 0.75
    for i, lib in enumerate(libs):
        barcenter = (i - 0.5 * (len(libs) - 1)) * groupheight / len(libs)
        barheight = groupheight / len(libs)
        c = 8 if lib == "logsort" else i
        ax.barh(yticks + barcenter, table[i], height=barheight, label=lib, color=tab20c[c])

    plt.rcParams["font.family"] = ["M PLUS Code Latin 60"]
    ax.legend()
    fig.tight_layout()
    plt.savefig(filename, dpi=dpi)


def main():
    import matplotlib as mpl
    mpl.use("Agg")

    with open("clang.yml") as f:
        libs, benches, table = load_result(f)
        plot_data("clang.png", libs, benches, table, "Benchmark by 1.5M items of 64bit integers (Clang 21.1.0)")

    with open("gcc.yml") as f:
        libs, benches, table = load_result(f)
        plot_data("gcc.png", libs, benches, table, "Benchmark by 1.5M items of 64bit integers (GCC 15.2.0)")


if __name__ == "__main__":
    main()

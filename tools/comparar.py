#!/usr/bin/env python3
"""Compara evaluaciones de wrl_evaluate escenario por escenario a partir de los CSV por episodio
(tareas, puntualidad, choques, incidentes de carga, velocidad insegura y cómo terminan).

Uso: python3 tools/comparar.py results/eval_dwa results/eval_sac_v12 [...] [--escenarios E5,E7]
"""
import collections
import csv
import sys
from pathlib import Path

COLS = [("compl%", "{:.1f}"), ("a_tiempo%", "{:.1f}"), ("graves_op", "{:d}"), ("graves/ep", "{:.2f}"),
        ("leves/ep", "{:.2f}"), ("roces/ep", "{:.2f}"), ("incid%", "{:.1f}"), ("entrega_s", "{:.0f}"),
        ("inseg_op_s", "{:.1f}"), ("inseg_est_s", "{:.1f}"), ("fin_choque", "{:d}"), ("fin_tmax", "{:d}")]


def stats(rows):
    n = len(rows)
    tasks = sum(int(r["tareas"]) for r in rows)
    deliv = sum(int(r["entregadas"]) for r in rows)
    fins = collections.Counter(r["fin"] for r in rows)
    mean = lambda k: sum(float(r.get(k) or 0) for r in rows) / n
    return [100 * deliv / max(tasks, 1), 100 * sum(int(r["a_tiempo"]) for r in rows) / max(tasks, 1),
            sum(int(r["graves_operario"]) for r in rows), mean("colisiones_graves"), mean("colisiones_leves"),
            mean("roces"), 100 * sum(int(r["incidentes_carga"]) for r in rows) / max(deliv, 1),
            mean("tiempo_entrega_medio_s"), mean("inseguro_operario_s"), mean("inseguro_estatico_s"),
            fins["limite_colisiones"] + fins["colision_operario"], fins["tiempo_maximo"]]


def main(argv):
    only = None
    if "--escenarios" in argv:
        i = argv.index("--escenarios")
        only = set(argv[i + 1].split(","))
        argv = argv[:i] + argv[i + 2:]
    names = sorted({p.name.removesuffix("_episodios.csv") for d in argv for p in Path(d).glob("*_episodios.csv")})
    width = max(len(Path(d).name) for d in argv) + 2
    for sc in names:
        if only and not any(sc.startswith(o) for o in only):
            continue
        print(f"== {sc}")
        print(" " * width + "".join(f"{c:>12}" for c, _ in COLS))
        for d in argv:
            p = Path(d) / f"{sc}_episodios.csv"
            if not p.exists():
                continue
            vals = stats(list(csv.DictReader(p.open())))
            print(f"{Path(d).name:<{width}}" + "".join(f"{f.format(v):>12}" for v, (_, f) in zip(vals, COLS)))
        print()


if __name__ == "__main__":
    main(sys.argv[1:])

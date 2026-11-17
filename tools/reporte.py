#!/usr/bin/env python3
"""Genera docs/RESULTADOS.md comparando políticas a partir de los resumen.json de wrl_evaluate.

Uso: python3 tools/reporte.py results/eval_dwa results/eval_sac [...] > docs/RESULTADOS.md
     Cada carpeta puede llevar una etiqueta (carpeta=etiqueta) para distinguir modelos de la
     misma política: results/eval_sac_v12="SAC v12" results/eval_sac_v10="SAC v10".
"""
import json
import sys
from pathlib import Path

COLS = [
    ("tasa_completadas", "Completadas", "{:.1%}"),
    ("tasa_a_tiempo", "A tiempo", "{:.1%}"),
    ("episodios_sin_colision", "Ep. sin colisión", "{:.1%}"),
    ("colisiones_graves_operario", "Graves c/ operario", "{:.0f}"),
    ("ratio_ruta", "Ratio ruta", "{:.2f}"),
    ("tasa_incidentes_carga", "Incid. carga", "{:.1%}"),
    ("tasa_bateria_agotada", "Batería agotada", "{:.1%}"),
    ("tiempo_entrega_medio_s", "T. entrega (s)", "{:.0f}"),
    ("energia_por_entrega_wh", "Wh/entrega", "{:.2f}"),
]


def main(dirs):
    data = {}
    for arg in dirs:
        d, _, label = arg.partition("=")
        rep = json.loads((Path(d) / "resumen.json").read_text())
        for scenario, body in rep.items():
            data.setdefault(scenario, []).append((label or body["politica"], body))

    print("# Resultados de evaluación\n")
    print("Generado con `tools/reporte.py` a partir de `wrl_evaluate` "
          "(100 episodios por escenario repartidos en 5 semillas de evaluación, sin exploración).\n")
    for scenario in sorted(data):
        print(f"## {scenario}\n")
        print("| Política | " + " | ".join(c[1] for c in COLS) + " |")
        print("|---|" + "---|" * len(COLS))
        for policy, body in data[scenario]:
            s = body["resumen"]
            print(f"| {policy} | " + " | ".join(fmt.format(s.get(k, 0)) for k, _, fmt in COLS) + " |")
        print()
        for policy, body in data[scenario]:
            crit = body.get("criterios", [])
            if not crit:
                continue
            ok = sum(c["cumple"] for c in crit)
            items = "; ".join(f"{c['criterio']} {'✔' if c['cumple'] else '✘'} ({c['valor']:.3f} vs {c['umbral']:.3f})" for c in crit)
            print(f"- **{policy}**: {ok}/{len(crit)} criterios — {items}")
        print()


if __name__ == "__main__":
    main(sys.argv[1:])

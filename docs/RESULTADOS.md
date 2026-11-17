# Resultados de evaluación

Generado con `tools/reporte.py` a partir de `wrl_evaluate` (100 episodios por escenario repartidos en 5 semillas de evaluación, sin exploración).

## E0_navegacion

| Política | Completadas | A tiempo | Ep. sin colisión | Graves c/ operario | Ratio ruta | Incid. carga | Batería agotada | T. entrega (s) | Wh/entrega |
|---|---|---|---|---|---|---|---|---|---|
| dwa | 99.0% | 99.0% | 100.0% | 0 | 1.01 | 0.0% | 0.0% | 39 | 0.39 |
| sac | 100.0% | 100.0% | 96.0% | 0 | 0.98 | 0.0% | 0.0% | 22 | 0.33 |

- **dwa**: 1/1 criterios — tareas completadas ✔ (0.990 vs 0.900)
- **sac**: 1/1 criterios — tareas completadas ✔ (1.000 vs 0.900)

## E1_basico

| Política | Completadas | A tiempo | Ep. sin colisión | Graves c/ operario | Ratio ruta | Incid. carga | Batería agotada | T. entrega (s) | Wh/entrega |
|---|---|---|---|---|---|---|---|---|---|
| dwa | 98.0% | 98.0% | 100.0% | 0 | 1.02 | 0.0% | 0.0% | 38 | 0.39 |
| sac | 100.0% | 100.0% | 93.0% | 0 | 0.99 | 2.0% | 0.0% | 22 | 0.35 |

- **dwa**: 3/3 criterios — tareas completadas ✔ (0.980 vs 0.900); ratio de ruta frente a la más corta ✔ (1.016 vs 1.250); incidentes de carga por entrega ✔ (0.000 vs 0.020)
- **sac**: 3/3 criterios — tareas completadas ✔ (1.000 vs 0.900); ratio de ruta frente a la más corta ✔ (0.994 vs 1.250); incidentes de carga por entrega ✔ (0.020 vs 0.020)

## E2_energia_carga

| Política | Completadas | A tiempo | Ep. sin colisión | Graves c/ operario | Ratio ruta | Incid. carga | Batería agotada | T. entrega (s) | Wh/entrega |
|---|---|---|---|---|---|---|---|---|---|
| dwa | 95.2% | 95.2% | 100.0% | 0 | 1.02 | 0.0% | 0.0% | 159 | 0.54 |
| sac | 100.0% | 100.0% | 79.0% | 0 | 1.00 | 0.4% | 0.0% | 99 | 0.50 |

- **dwa**: 3/3 criterios — tareas completadas ✔ (0.952 vs 0.900); incidentes de carga por entrega ✔ (0.000 vs 0.020); episodios con batería agotada ✔ (0.000 vs 0.050)
- **sac**: 3/3 criterios — tareas completadas ✔ (1.000 vs 0.900); incidentes de carga por entrega ✔ (0.004 vs 0.020); episodios con batería agotada ✔ (0.000 vs 0.050)

## E3_priorizacion

| Política | Completadas | A tiempo | Ep. sin colisión | Graves c/ operario | Ratio ruta | Incid. carga | Batería agotada | T. entrega (s) | Wh/entrega |
|---|---|---|---|---|---|---|---|---|---|
| dwa | 94.3% | 83.7% | 100.0% | 0 | 1.02 | 0.0% | 0.0% | 98 | 0.51 |
| sac | 98.4% | 98.1% | 67.0% | 0 | 1.01 | 3.7% | 0.0% | 44 | 0.57 |

- **dwa**: 2/2 criterios — entregas a tiempo ✔ (0.837 vs 0.800); incidentes de carga por entrega ✔ (0.000 vs 0.020)
- **sac**: 1/2 criterios — entregas a tiempo ✔ (0.981 vs 0.800); incidentes de carga por entrega ✘ (0.037 vs 0.020)

## E4_dinamico

| Política | Completadas | A tiempo | Ep. sin colisión | Graves c/ operario | Ratio ruta | Incid. carga | Batería agotada | T. entrega (s) | Wh/entrega |
|---|---|---|---|---|---|---|---|---|---|
| dwa | 95.2% | 95.2% | 72.0% | 1 | 1.03 | 1.7% | 0.0% | 131 | 0.49 |
| sac | 94.8% | 94.8% | 23.0% | 8 | 1.07 | 14.0% | 0.0% | 100 | 0.60 |

- **dwa**: 1/3 criterios — tareas completadas ✔ (0.952 vs 0.900); episodios sin colisiones ✘ (0.720 vs 0.950); colisiones graves con operarios ✘ (1.000 vs 0.000)
- **sac**: 1/3 criterios — tareas completadas ✔ (0.948 vs 0.900); episodios sin colisiones ✘ (0.230 vs 0.950); colisiones graves con operarios ✘ (8.000 vs 0.000)

## E5_combinado

| Política | Completadas | A tiempo | Ep. sin colisión | Graves c/ operario | Ratio ruta | Incid. carga | Batería agotada | T. entrega (s) | Wh/entrega |
|---|---|---|---|---|---|---|---|---|---|
| dwa | 87.2% | 50.3% | 50.0% | 0 | 1.03 | 1.8% | 0.0% | 266 | 0.71 |
| sac | 78.3% | 52.0% | 8.0% | 15 | 1.09 | 8.9% | 0.0% | 243 | 0.96 |

- **dwa**: 4/4 criterios — tareas completadas ✔ (0.872 vs 0.750); colisiones graves con operarios ✔ (0.000 vs 0.000); incidentes de carga por entrega ✔ (0.018 vs 0.020); episodios con batería agotada ✔ (0.000 vs 0.050)
- **sac**: 2/4 criterios — tareas completadas ✔ (0.783 vs 0.750); colisiones graves con operarios ✘ (15.000 vs 0.000); incidentes de carga por entrega ✘ (0.089 vs 0.020); episodios con batería agotada ✔ (0.000 vs 0.050)

## E6_generalizacion

| Política | Completadas | A tiempo | Ep. sin colisión | Graves c/ operario | Ratio ruta | Incid. carga | Batería agotada | T. entrega (s) | Wh/entrega |
|---|---|---|---|---|---|---|---|---|---|
| dwa | 95.9% | 61.6% | 78.0% | 0 | 1.03 | 0.6% | 0.0% | 222 | 0.69 |
| sac | 91.1% | 66.3% | 30.0% | 4 | 1.07 | 10.2% | 0.0% | 210 | 0.92 |

- **dwa**: 3/3 criterios — tareas completadas ✔ (0.959 vs 0.750); colisiones graves con operarios ✔ (0.000 vs 0.000); incidentes de carga por entrega ✔ (0.006 vs 0.020)
- **sac**: 1/3 criterios — tareas completadas ✔ (0.911 vs 0.750); colisiones graves con operarios ✘ (4.000 vs 0.000); incidentes de carga por entrega ✘ (0.102 vs 0.020)

## E7_robustez_fisica

| Política | Completadas | A tiempo | Ep. sin colisión | Graves c/ operario | Ratio ruta | Incid. carga | Batería agotada | T. entrega (s) | Wh/entrega |
|---|---|---|---|---|---|---|---|---|---|
| dwa | 90.8% | 51.7% | 47.0% | 1 | 1.03 | 1.8% | 0.0% | 279 | 0.71 |
| sac | 82.9% | 57.7% | 5.0% | 11 | 1.07 | 9.1% | 0.0% | 227 | 0.94 |

- **dwa**: 3/4 criterios — tareas completadas ✔ (0.908 vs 0.650); colisiones graves con operarios ✘ (1.000 vs 0.000); incidentes de carga por entrega ✔ (0.018 vs 0.020); episodios con batería agotada ✔ (0.000 vs 0.050)
- **sac**: 2/4 criterios — tareas completadas ✔ (0.829 vs 0.650); colisiones graves con operarios ✘ (11.000 vs 0.000); incidentes de carga por entrega ✘ (0.091 vs 0.020); episodios con batería agotada ✔ (0.000 vs 0.050)


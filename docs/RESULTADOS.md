# Resultados de evaluación

Generado con `tools/reporte.py` a partir de `wrl_evaluate` (100 episodios por escenario repartidos en 5 semillas de evaluación, sin exploración).

## E0_navegacion

| Política | Completadas | A tiempo | Ep. sin colisión | Graves c/ operario | Ratio ruta | Incid. carga | Batería agotada | T. entrega (s) | Wh/entrega |
|---|---|---|---|---|---|---|---|---|---|
| A* + DWA | 100.0% | 100.0% | 100.0% | 0 | 1.02 | 0.0% | 0.0% | 40 | 0.39 |
| SAC v12 | 100.0% | 100.0% | 100.0% | 0 | 0.99 | 0.0% | 0.0% | 24 | 0.35 |
| SAC v10 | 100.0% | 100.0% | 96.0% | 0 | 0.98 | 0.0% | 0.0% | 22 | 0.33 |

- **A* + DWA**: 1/1 criterios — tareas completadas ✔ (1.000 vs 0.900)
- **SAC v12**: 1/1 criterios — tareas completadas ✔ (1.000 vs 0.900)
- **SAC v10**: 1/1 criterios — tareas completadas ✔ (1.000 vs 0.900)

## E1_basico

| Política | Completadas | A tiempo | Ep. sin colisión | Graves c/ operario | Ratio ruta | Incid. carga | Batería agotada | T. entrega (s) | Wh/entrega |
|---|---|---|---|---|---|---|---|---|---|
| A* + DWA | 98.0% | 98.0% | 100.0% | 0 | 1.02 | 0.0% | 0.0% | 38 | 0.39 |
| SAC v12 | 100.0% | 100.0% | 100.0% | 0 | 0.99 | 2.9% | 0.0% | 25 | 0.37 |
| SAC v10 | 100.0% | 100.0% | 93.0% | 0 | 0.99 | 2.0% | 0.0% | 22 | 0.35 |

- **A* + DWA**: 3/3 criterios — tareas completadas ✔ (0.980 vs 0.900); ratio de ruta frente a la más corta ✔ (1.016 vs 1.250); incidentes de carga por entrega ✔ (0.000 vs 0.020)
- **SAC v12**: 2/3 criterios — tareas completadas ✔ (1.000 vs 0.900); ratio de ruta frente a la más corta ✔ (0.993 vs 1.250); incidentes de carga por entrega ✘ (0.029 vs 0.020)
- **SAC v10**: 3/3 criterios — tareas completadas ✔ (1.000 vs 0.900); ratio de ruta frente a la más corta ✔ (0.994 vs 1.250); incidentes de carga por entrega ✔ (0.020 vs 0.020)

## E2_energia_carga

| Política | Completadas | A tiempo | Ep. sin colisión | Graves c/ operario | Ratio ruta | Incid. carga | Batería agotada | T. entrega (s) | Wh/entrega |
|---|---|---|---|---|---|---|---|---|---|
| A* + DWA | 95.2% | 95.2% | 100.0% | 0 | 1.02 | 0.0% | 0.0% | 159 | 0.54 |
| SAC v12 | 100.0% | 100.0% | 100.0% | 0 | 1.00 | 6.7% | 0.0% | 124 | 0.55 |
| SAC v10 | 100.0% | 100.0% | 79.0% | 0 | 1.00 | 0.4% | 0.0% | 99 | 0.50 |

- **A* + DWA**: 3/3 criterios — tareas completadas ✔ (0.952 vs 0.900); incidentes de carga por entrega ✔ (0.000 vs 0.020); episodios con batería agotada ✔ (0.000 vs 0.050)
- **SAC v12**: 2/3 criterios — tareas completadas ✔ (1.000 vs 0.900); incidentes de carga por entrega ✘ (0.067 vs 0.020); episodios con batería agotada ✔ (0.000 vs 0.050)
- **SAC v10**: 3/3 criterios — tareas completadas ✔ (1.000 vs 0.900); incidentes de carga por entrega ✔ (0.004 vs 0.020); episodios con batería agotada ✔ (0.000 vs 0.050)

## E3_priorizacion

| Política | Completadas | A tiempo | Ep. sin colisión | Graves c/ operario | Ratio ruta | Incid. carga | Batería agotada | T. entrega (s) | Wh/entrega |
|---|---|---|---|---|---|---|---|---|---|
| A* + DWA | 94.3% | 83.7% | 100.0% | 0 | 1.02 | 0.0% | 0.0% | 98 | 0.51 |
| SAC v12 | 96.8% | 91.0% | 97.0% | 0 | 1.01 | 1.8% | 0.0% | 80 | 0.60 |
| SAC v10 | 98.4% | 98.1% | 67.0% | 0 | 1.01 | 3.7% | 0.0% | 44 | 0.57 |

- **A* + DWA**: 2/2 criterios — entregas a tiempo ✔ (0.837 vs 0.800); incidentes de carga por entrega ✔ (0.000 vs 0.020)
- **SAC v12**: 2/2 criterios — entregas a tiempo ✔ (0.910 vs 0.800); incidentes de carga por entrega ✔ (0.018 vs 0.020)
- **SAC v10**: 1/2 criterios — entregas a tiempo ✔ (0.981 vs 0.800); incidentes de carga por entrega ✘ (0.037 vs 0.020)

## E4_dinamico

| Política | Completadas | A tiempo | Ep. sin colisión | Graves c/ operario | Ratio ruta | Incid. carga | Batería agotada | T. entrega (s) | Wh/entrega |
|---|---|---|---|---|---|---|---|---|---|
| A* + DWA | 95.2% | 95.2% | 69.0% | 1 | 1.03 | 2.1% | 0.0% | 132 | 0.49 |
| SAC v12 | 98.0% | 98.0% | 38.0% | 2 | 1.07 | 13.6% | 0.0% | 110 | 0.62 |
| SAC v10 | 95.2% | 95.2% | 22.0% | 9 | 1.08 | 14.4% | 0.0% | 101 | 0.60 |

- **A* + DWA**: 1/3 criterios — tareas completadas ✔ (0.952 vs 0.900); episodios sin colisiones ✘ (0.690 vs 0.950); colisiones graves con operarios ✘ (1.000 vs 0.000)
- **SAC v12**: 1/3 criterios — tareas completadas ✔ (0.980 vs 0.900); episodios sin colisiones ✘ (0.380 vs 0.950); colisiones graves con operarios ✘ (2.000 vs 0.000)
- **SAC v10**: 1/3 criterios — tareas completadas ✔ (0.952 vs 0.900); episodios sin colisiones ✘ (0.220 vs 0.950); colisiones graves con operarios ✘ (9.000 vs 0.000)

## E5_combinado

| Política | Completadas | A tiempo | Ep. sin colisión | Graves c/ operario | Ratio ruta | Incid. carga | Batería agotada | T. entrega (s) | Wh/entrega |
|---|---|---|---|---|---|---|---|---|---|
| A* + DWA | 87.3% | 50.7% | 52.0% | 0 | 1.03 | 1.6% | 0.0% | 265 | 0.71 |
| SAC v12 | 89.3% | 47.9% | 8.0% | 5 | 1.10 | 9.6% | 0.0% | 362 | 1.07 |
| SAC v10 | 80.3% | 54.0% | 8.0% | 14 | 1.08 | 8.9% | 0.0% | 239 | 0.96 |

- **A* + DWA**: 4/4 criterios — tareas completadas ✔ (0.873 vs 0.750); colisiones graves con operarios ✔ (0.000 vs 0.000); incidentes de carga por entrega ✔ (0.016 vs 0.020); episodios con batería agotada ✔ (0.000 vs 0.050)
- **SAC v12**: 2/4 criterios — tareas completadas ✔ (0.893 vs 0.750); colisiones graves con operarios ✘ (5.000 vs 0.000); incidentes de carga por entrega ✘ (0.096 vs 0.020); episodios con batería agotada ✔ (0.000 vs 0.050)
- **SAC v10**: 2/4 criterios — tareas completadas ✔ (0.803 vs 0.750); colisiones graves con operarios ✘ (14.000 vs 0.000); incidentes de carga por entrega ✘ (0.089 vs 0.020); episodios con batería agotada ✔ (0.000 vs 0.050)

## E6_generalizacion

| Política | Completadas | A tiempo | Ep. sin colisión | Graves c/ operario | Ratio ruta | Incid. carga | Batería agotada | T. entrega (s) | Wh/entrega |
|---|---|---|---|---|---|---|---|---|---|
| A* + DWA | 95.9% | 61.8% | 76.0% | 0 | 1.03 | 0.6% | 0.0% | 221 | 0.69 |
| SAC v12 | 93.1% | 52.6% | 18.0% | 2 | 1.08 | 8.9% | 0.0% | 302 | 1.04 |
| SAC v10 | 89.3% | 65.5% | 31.0% | 5 | 1.06 | 10.0% | 0.0% | 207 | 0.92 |

- **A* + DWA**: 3/3 criterios — tareas completadas ✔ (0.959 vs 0.750); colisiones graves con operarios ✔ (0.000 vs 0.000); incidentes de carga por entrega ✔ (0.006 vs 0.020)
- **SAC v12**: 1/3 criterios — tareas completadas ✔ (0.931 vs 0.750); colisiones graves con operarios ✘ (2.000 vs 0.000); incidentes de carga por entrega ✘ (0.089 vs 0.020)
- **SAC v10**: 1/3 criterios — tareas completadas ✔ (0.893 vs 0.750); colisiones graves con operarios ✘ (5.000 vs 0.000); incidentes de carga por entrega ✘ (0.100 vs 0.020)

## E7_robustez_fisica

| Política | Completadas | A tiempo | Ep. sin colisión | Graves c/ operario | Ratio ruta | Incid. carga | Batería agotada | T. entrega (s) | Wh/entrega |
|---|---|---|---|---|---|---|---|---|---|
| A* + DWA | 88.1% | 49.2% | 52.0% | 1 | 1.03 | 1.6% | 0.0% | 276 | 0.71 |
| SAC v12 | 86.3% | 46.1% | 4.0% | 12 | 1.09 | 10.2% | 0.0% | 370 | 1.05 |
| SAC v10 | 82.9% | 57.4% | 7.0% | 10 | 1.07 | 8.6% | 0.0% | 228 | 0.94 |

- **A* + DWA**: 3/4 criterios — tareas completadas ✔ (0.881 vs 0.650); colisiones graves con operarios ✘ (1.000 vs 0.000); incidentes de carga por entrega ✔ (0.016 vs 0.020); episodios con batería agotada ✔ (0.000 vs 0.050)
- **SAC v12**: 2/4 criterios — tareas completadas ✔ (0.863 vs 0.650); colisiones graves con operarios ✘ (12.000 vs 0.000); incidentes de carga por entrega ✘ (0.102 vs 0.020); episodios con batería agotada ✔ (0.000 vs 0.050)
- **SAC v10**: 2/4 criterios — tareas completadas ✔ (0.829 vs 0.650); colisiones graves con operarios ✘ (10.000 vs 0.000); incidentes de carga por entrega ✘ (0.086 vs 0.020); episodios con batería agotada ✔ (0.000 vs 0.050)


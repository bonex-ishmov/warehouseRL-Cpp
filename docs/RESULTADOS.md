# Resultados de evaluación

Generado con `tools/reporte.py` a partir de `wrl_evaluate` (100 episodios por escenario repartidos en 5 semillas de evaluación, sin exploración).

## E0_navegacion

| Política | Completadas | A tiempo | Ep. sin colisión | Graves c/ operario | Ratio ruta | Incid. carga | Batería agotada | T. entrega (s) | Wh/entrega |
|---|---|---|---|---|---|---|---|---|---|
| SAC v12 + capa (entregado) | 100.0% | 100.0% | 99.0% | 0 | 0.99 | 0.0% | 0.0% | 26 | 0.36 |
| SAC v12 sin capa | 100.0% | 100.0% | 100.0% | 0 | 0.99 | 0.0% | 0.0% | 24 | 0.35 |
| A* + DWA + capa | 100.0% | 100.0% | 100.0% | 0 | 1.02 | 0.0% | 0.0% | 40 | 0.39 |
| A* + DWA sin capa | 100.0% | 100.0% | 100.0% | 0 | 1.02 | 0.0% | 0.0% | 40 | 0.39 |

- **SAC v12 + capa (entregado)**: 1/1 criterios — tareas completadas ✔ (1.000 vs 0.900)
- **SAC v12 sin capa**: 1/1 criterios — tareas completadas ✔ (1.000 vs 0.900)
- **A* + DWA + capa**: 1/1 criterios — tareas completadas ✔ (1.000 vs 0.900)
- **A* + DWA sin capa**: 1/1 criterios — tareas completadas ✔ (1.000 vs 0.900)

## E1_basico

| Política | Completadas | A tiempo | Ep. sin colisión | Graves c/ operario | Ratio ruta | Incid. carga | Batería agotada | T. entrega (s) | Wh/entrega |
|---|---|---|---|---|---|---|---|---|---|
| SAC v12 + capa (entregado) | 100.0% | 100.0% | 100.0% | 0 | 0.99 | 1.0% | 0.0% | 25 | 0.37 |
| SAC v12 sin capa | 100.0% | 100.0% | 100.0% | 0 | 0.99 | 2.9% | 0.0% | 25 | 0.37 |
| A* + DWA + capa | 99.0% | 99.0% | 100.0% | 0 | 1.02 | 0.0% | 0.0% | 39 | 0.38 |
| A* + DWA sin capa | 98.0% | 98.0% | 100.0% | 0 | 1.02 | 0.0% | 0.0% | 38 | 0.39 |

- **SAC v12 + capa (entregado)**: 3/3 criterios — tareas completadas ✔ (1.000 vs 0.900); ratio de ruta frente a la más corta ✔ (0.990 vs 1.250); incidentes de carga por entrega ✔ (0.010 vs 0.020)
- **SAC v12 sin capa**: 2/3 criterios — tareas completadas ✔ (1.000 vs 0.900); ratio de ruta frente a la más corta ✔ (0.993 vs 1.250); incidentes de carga por entrega ✘ (0.029 vs 0.020)
- **A* + DWA + capa**: 3/3 criterios — tareas completadas ✔ (0.990 vs 0.900); ratio de ruta frente a la más corta ✔ (1.016 vs 1.250); incidentes de carga por entrega ✔ (0.000 vs 0.020)
- **A* + DWA sin capa**: 3/3 criterios — tareas completadas ✔ (0.980 vs 0.900); ratio de ruta frente a la más corta ✔ (1.016 vs 1.250); incidentes de carga por entrega ✔ (0.000 vs 0.020)

## E2_energia_carga

| Política | Completadas | A tiempo | Ep. sin colisión | Graves c/ operario | Ratio ruta | Incid. carga | Batería agotada | T. entrega (s) | Wh/entrega |
|---|---|---|---|---|---|---|---|---|---|
| SAC v12 + capa (entregado) | 99.2% | 99.2% | 100.0% | 0 | 1.00 | 1.6% | 0.0% | 123 | 0.55 |
| SAC v12 sin capa | 100.0% | 100.0% | 100.0% | 0 | 1.00 | 6.7% | 0.0% | 124 | 0.55 |
| A* + DWA + capa | 96.2% | 96.2% | 100.0% | 0 | 1.02 | 0.0% | 0.0% | 159 | 0.54 |
| A* + DWA sin capa | 95.2% | 95.2% | 100.0% | 0 | 1.02 | 0.0% | 0.0% | 159 | 0.54 |

- **SAC v12 + capa (entregado)**: 3/3 criterios — tareas completadas ✔ (0.992 vs 0.900); incidentes de carga por entrega ✔ (0.016 vs 0.020); episodios con batería agotada ✔ (0.000 vs 0.050)
- **SAC v12 sin capa**: 2/3 criterios — tareas completadas ✔ (1.000 vs 0.900); incidentes de carga por entrega ✘ (0.067 vs 0.020); episodios con batería agotada ✔ (0.000 vs 0.050)
- **A* + DWA + capa**: 3/3 criterios — tareas completadas ✔ (0.962 vs 0.900); incidentes de carga por entrega ✔ (0.000 vs 0.020); episodios con batería agotada ✔ (0.000 vs 0.050)
- **A* + DWA sin capa**: 3/3 criterios — tareas completadas ✔ (0.952 vs 0.900); incidentes de carga por entrega ✔ (0.000 vs 0.020); episodios con batería agotada ✔ (0.000 vs 0.050)

## E3_priorizacion

| Política | Completadas | A tiempo | Ep. sin colisión | Graves c/ operario | Ratio ruta | Incid. carga | Batería agotada | T. entrega (s) | Wh/entrega |
|---|---|---|---|---|---|---|---|---|---|
| SAC v12 + capa (entregado) | 97.8% | 92.7% | 97.0% | 0 | 1.01 | 0.9% | 0.0% | 81 | 0.61 |
| SAC v12 sin capa | 96.8% | 91.0% | 97.0% | 0 | 1.01 | 1.8% | 0.0% | 80 | 0.60 |
| A* + DWA + capa | 97.3% | 86.6% | 100.0% | 0 | 1.02 | 0.0% | 0.0% | 98 | 0.50 |
| A* + DWA sin capa | 94.3% | 83.7% | 100.0% | 0 | 1.02 | 0.0% | 0.0% | 98 | 0.51 |

- **SAC v12 + capa (entregado)**: 2/2 criterios — entregas a tiempo ✔ (0.927 vs 0.800); incidentes de carga por entrega ✔ (0.009 vs 0.020)
- **SAC v12 sin capa**: 2/2 criterios — entregas a tiempo ✔ (0.910 vs 0.800); incidentes de carga por entrega ✔ (0.018 vs 0.020)
- **A* + DWA + capa**: 2/2 criterios — entregas a tiempo ✔ (0.866 vs 0.800); incidentes de carga por entrega ✔ (0.000 vs 0.020)
- **A* + DWA sin capa**: 2/2 criterios — entregas a tiempo ✔ (0.837 vs 0.800); incidentes de carga por entrega ✔ (0.000 vs 0.020)

## E4_dinamico

| Política | Completadas | A tiempo | Ep. sin colisión | Graves c/ operario | Ratio ruta | Incid. carga | Batería agotada | T. entrega (s) | Wh/entrega |
|---|---|---|---|---|---|---|---|---|---|
| SAC v12 + capa (entregado) | 100.0% | 100.0% | 61.0% | 0 | 1.07 | 7.1% | 0.0% | 106 | 0.59 |
| SAC v12 sin capa | 98.0% | 98.0% | 38.0% | 2 | 1.07 | 13.6% | 0.0% | 110 | 0.62 |
| A* + DWA + capa | 98.6% | 98.6% | 75.0% | 0 | 1.04 | 1.6% | 0.0% | 134 | 0.50 |
| A* + DWA sin capa | 95.2% | 95.2% | 69.0% | 1 | 1.03 | 2.1% | 0.0% | 132 | 0.49 |

- **SAC v12 + capa (entregado)**: 2/3 criterios — tareas completadas ✔ (1.000 vs 0.900); episodios sin colisiones ✘ (0.610 vs 0.950); colisiones graves con operarios ✔ (0.000 vs 0.000)
- **SAC v12 sin capa**: 1/3 criterios — tareas completadas ✔ (0.980 vs 0.900); episodios sin colisiones ✘ (0.380 vs 0.950); colisiones graves con operarios ✘ (2.000 vs 0.000)
- **A* + DWA + capa**: 2/3 criterios — tareas completadas ✔ (0.986 vs 0.900); episodios sin colisiones ✘ (0.750 vs 0.950); colisiones graves con operarios ✔ (0.000 vs 0.000)
- **A* + DWA sin capa**: 1/3 criterios — tareas completadas ✔ (0.952 vs 0.900); episodios sin colisiones ✘ (0.690 vs 0.950); colisiones graves con operarios ✘ (1.000 vs 0.000)

## E5_combinado

| Política | Completadas | A tiempo | Ep. sin colisión | Graves c/ operario | Ratio ruta | Incid. carga | Batería agotada | T. entrega (s) | Wh/entrega |
|---|---|---|---|---|---|---|---|---|---|
| SAC v12 + capa (entregado) | 96.5% | 48.9% | 22.0% | 2 | 1.10 | 6.6% | 0.0% | 385 | 1.07 |
| SAC v12 sin capa | 89.3% | 47.9% | 8.0% | 5 | 1.10 | 9.6% | 0.0% | 362 | 1.07 |
| A* + DWA + capa | 94.5% | 53.7% | 49.0% | 0 | 1.03 | 1.3% | 0.0% | 271 | 0.71 |
| A* + DWA sin capa | 87.3% | 50.7% | 52.0% | 0 | 1.03 | 1.6% | 0.0% | 265 | 0.71 |

- **SAC v12 + capa (entregado)**: 2/4 criterios — tareas completadas ✔ (0.965 vs 0.750); colisiones graves con operarios ✘ (2.000 vs 0.000); incidentes de carga por entrega ✘ (0.066 vs 0.020); episodios con batería agotada ✔ (0.000 vs 0.050)
- **SAC v12 sin capa**: 2/4 criterios — tareas completadas ✔ (0.893 vs 0.750); colisiones graves con operarios ✘ (5.000 vs 0.000); incidentes de carga por entrega ✘ (0.096 vs 0.020); episodios con batería agotada ✔ (0.000 vs 0.050)
- **A* + DWA + capa**: 4/4 criterios — tareas completadas ✔ (0.945 vs 0.750); colisiones graves con operarios ✔ (0.000 vs 0.000); incidentes de carga por entrega ✔ (0.013 vs 0.020); episodios con batería agotada ✔ (0.000 vs 0.050)
- **A* + DWA sin capa**: 4/4 criterios — tareas completadas ✔ (0.873 vs 0.750); colisiones graves con operarios ✔ (0.000 vs 0.000); incidentes de carga por entrega ✔ (0.016 vs 0.020); episodios con batería agotada ✔ (0.000 vs 0.050)

## E6_generalizacion

| Política | Completadas | A tiempo | Ep. sin colisión | Graves c/ operario | Ratio ruta | Incid. carga | Batería agotada | T. entrega (s) | Wh/entrega |
|---|---|---|---|---|---|---|---|---|---|
| SAC v12 + capa (entregado) | 95.8% | 57.6% | 48.0% | 0 | 1.09 | 5.3% | 0.0% | 285 | 1.01 |
| SAC v12 sin capa | 93.1% | 52.6% | 18.0% | 2 | 1.08 | 8.9% | 0.0% | 302 | 1.04 |
| A* + DWA + capa | 95.7% | 59.8% | 66.0% | 1 | 1.03 | 1.3% | 0.0% | 225 | 0.69 |
| A* + DWA sin capa | 95.9% | 61.8% | 76.0% | 0 | 1.03 | 0.6% | 0.0% | 221 | 0.69 |

- **SAC v12 + capa (entregado)**: 2/3 criterios — tareas completadas ✔ (0.958 vs 0.750); colisiones graves con operarios ✔ (0.000 vs 0.000); incidentes de carga por entrega ✘ (0.053 vs 0.020)
- **SAC v12 sin capa**: 1/3 criterios — tareas completadas ✔ (0.931 vs 0.750); colisiones graves con operarios ✘ (2.000 vs 0.000); incidentes de carga por entrega ✘ (0.089 vs 0.020)
- **A* + DWA + capa**: 2/3 criterios — tareas completadas ✔ (0.957 vs 0.750); colisiones graves con operarios ✘ (1.000 vs 0.000); incidentes de carga por entrega ✔ (0.013 vs 0.020)
- **A* + DWA sin capa**: 3/3 criterios — tareas completadas ✔ (0.959 vs 0.750); colisiones graves con operarios ✔ (0.000 vs 0.000); incidentes de carga por entrega ✔ (0.006 vs 0.020)

## E7_robustez_fisica

| Política | Completadas | A tiempo | Ep. sin colisión | Graves c/ operario | Ratio ruta | Incid. carga | Batería agotada | T. entrega (s) | Wh/entrega |
|---|---|---|---|---|---|---|---|---|---|
| SAC v12 + capa (entregado) | 93.1% | 47.7% | 23.0% | 1 | 1.10 | 6.2% | 0.0% | 380 | 1.06 |
| SAC v12 sin capa | 86.3% | 46.1% | 4.0% | 12 | 1.09 | 10.2% | 0.0% | 370 | 1.05 |
| A* + DWA + capa | 93.4% | 51.5% | 41.0% | 1 | 1.03 | 2.0% | 0.0% | 288 | 0.71 |
| A* + DWA sin capa | 88.1% | 49.2% | 52.0% | 1 | 1.03 | 1.6% | 0.0% | 276 | 0.71 |

- **SAC v12 + capa (entregado)**: 2/4 criterios — tareas completadas ✔ (0.931 vs 0.650); colisiones graves con operarios ✘ (1.000 vs 0.000); incidentes de carga por entrega ✘ (0.062 vs 0.020); episodios con batería agotada ✔ (0.000 vs 0.050)
- **SAC v12 sin capa**: 2/4 criterios — tareas completadas ✔ (0.863 vs 0.650); colisiones graves con operarios ✘ (12.000 vs 0.000); incidentes de carga por entrega ✘ (0.102 vs 0.020); episodios con batería agotada ✔ (0.000 vs 0.050)
- **A* + DWA + capa**: 2/4 criterios — tareas completadas ✔ (0.934 vs 0.650); colisiones graves con operarios ✘ (1.000 vs 0.000); incidentes de carga por entrega ✘ (0.020 vs 0.020); episodios con batería agotada ✔ (0.000 vs 0.050)
- **A* + DWA sin capa**: 3/4 criterios — tareas completadas ✔ (0.881 vs 0.650); colisiones graves con operarios ✘ (1.000 vs 0.000); incidentes de carga por entrega ✔ (0.016 vs 0.020); episodios con batería agotada ✔ (0.000 vs 0.050)


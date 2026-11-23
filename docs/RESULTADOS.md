# Resultados de evaluación

Generado con `tools/reporte.py` a partir de `wrl_evaluate` (100 episodios por escenario repartidos en 5 semillas de evaluación, sin exploración).

## E0_navegacion

| Política | Completadas | A tiempo | Ep. sin colisión | Graves c/ operario | Ratio ruta | Incid. carga | Batería agotada | T. entrega (s) | Wh/entrega |
|---|---|---|---|---|---|---|---|---|---|
| SAC v14 + capa (entregado) | 100.0% | 100.0% | 100.0% | 0 | 0.99 | 0.0% | 0.0% | 24 | 0.35 |
| SAC v12 + capa | 100.0% | 100.0% | 99.0% | 0 | 0.99 | 0.0% | 0.0% | 26 | 0.36 |
| A* + DWA + capa | 100.0% | 100.0% | 100.0% | 0 | 1.02 | 0.0% | 0.0% | 40 | 0.39 |
| A* + DWA sin capa | 100.0% | 100.0% | 100.0% | 0 | 1.02 | 0.0% | 0.0% | 40 | 0.39 |

- **SAC v14 + capa (entregado)**: 1/1 criterios — tareas completadas ✔ (1.000 vs 0.900)
- **SAC v12 + capa**: 1/1 criterios — tareas completadas ✔ (1.000 vs 0.900)
- **A* + DWA + capa**: 1/1 criterios — tareas completadas ✔ (1.000 vs 0.900)
- **A* + DWA sin capa**: 1/1 criterios — tareas completadas ✔ (1.000 vs 0.900)

## E1_basico

| Política | Completadas | A tiempo | Ep. sin colisión | Graves c/ operario | Ratio ruta | Incid. carga | Batería agotada | T. entrega (s) | Wh/entrega |
|---|---|---|---|---|---|---|---|---|---|
| SAC v14 + capa (entregado) | 100.0% | 100.0% | 100.0% | 0 | 0.99 | 0.0% | 0.0% | 23 | 0.36 |
| SAC v12 + capa | 100.0% | 100.0% | 100.0% | 0 | 0.99 | 0.0% | 0.0% | 25 | 0.37 |
| A* + DWA + capa | 99.0% | 99.0% | 100.0% | 0 | 1.02 | 0.0% | 0.0% | 39 | 0.38 |
| A* + DWA sin capa | 98.0% | 98.0% | 100.0% | 0 | 1.02 | 0.0% | 0.0% | 38 | 0.39 |

- **SAC v14 + capa (entregado)**: 3/3 criterios — tareas completadas ✔ (1.000 vs 0.900); ratio de ruta frente a la más corta ✔ (0.988 vs 1.250); incidentes de carga por entrega ✔ (0.000 vs 0.020)
- **SAC v12 + capa**: 3/3 criterios — tareas completadas ✔ (1.000 vs 0.900); ratio de ruta frente a la más corta ✔ (0.990 vs 1.250); incidentes de carga por entrega ✔ (0.000 vs 0.020)
- **A* + DWA + capa**: 3/3 criterios — tareas completadas ✔ (0.990 vs 0.900); ratio de ruta frente a la más corta ✔ (1.016 vs 1.250); incidentes de carga por entrega ✔ (0.000 vs 0.020)
- **A* + DWA sin capa**: 3/3 criterios — tareas completadas ✔ (0.980 vs 0.900); ratio de ruta frente a la más corta ✔ (1.016 vs 1.250); incidentes de carga por entrega ✔ (0.000 vs 0.020)

## E2_energia_carga

| Política | Completadas | A tiempo | Ep. sin colisión | Graves c/ operario | Ratio ruta | Incid. carga | Batería agotada | T. entrega (s) | Wh/entrega |
|---|---|---|---|---|---|---|---|---|---|
| SAC v14 + capa (entregado) | 100.0% | 100.0% | 100.0% | 0 | 1.00 | 0.0% | 0.0% | 100 | 0.52 |
| SAC v12 + capa | 99.4% | 99.4% | 99.0% | 0 | 1.00 | 0.2% | 0.0% | 123 | 0.55 |
| A* + DWA + capa | 96.2% | 96.2% | 100.0% | 0 | 1.02 | 0.0% | 0.0% | 159 | 0.54 |
| A* + DWA sin capa | 95.2% | 95.2% | 100.0% | 0 | 1.02 | 0.0% | 0.0% | 159 | 0.54 |

- **SAC v14 + capa (entregado)**: 3/3 criterios — tareas completadas ✔ (1.000 vs 0.900); incidentes de carga por entrega ✔ (0.000 vs 0.020); episodios con batería agotada ✔ (0.000 vs 0.050)
- **SAC v12 + capa**: 3/3 criterios — tareas completadas ✔ (0.994 vs 0.900); incidentes de carga por entrega ✔ (0.002 vs 0.020); episodios con batería agotada ✔ (0.000 vs 0.050)
- **A* + DWA + capa**: 3/3 criterios — tareas completadas ✔ (0.962 vs 0.900); incidentes de carga por entrega ✔ (0.000 vs 0.020); episodios con batería agotada ✔ (0.000 vs 0.050)
- **A* + DWA sin capa**: 3/3 criterios — tareas completadas ✔ (0.952 vs 0.900); incidentes de carga por entrega ✔ (0.000 vs 0.020); episodios con batería agotada ✔ (0.000 vs 0.050)

## E3_priorizacion

| Política | Completadas | A tiempo | Ep. sin colisión | Graves c/ operario | Ratio ruta | Incid. carga | Batería agotada | T. entrega (s) | Wh/entrega |
|---|---|---|---|---|---|---|---|---|---|
| SAC v14 + capa (entregado) | 98.6% | 98.0% | 95.0% | 0 | 1.01 | 0.3% | 0.0% | 50 | 0.57 |
| SAC v12 + capa | 96.5% | 91.5% | 99.0% | 0 | 1.01 | 0.0% | 0.0% | 75 | 0.60 |
| A* + DWA + capa | 97.3% | 86.6% | 100.0% | 0 | 1.02 | 0.0% | 0.0% | 98 | 0.50 |
| A* + DWA sin capa | 94.3% | 83.7% | 100.0% | 0 | 1.02 | 0.0% | 0.0% | 98 | 0.51 |

- **SAC v14 + capa (entregado)**: 2/2 criterios — entregas a tiempo ✔ (0.980 vs 0.800); incidentes de carga por entrega ✔ (0.003 vs 0.020)
- **SAC v12 + capa**: 2/2 criterios — entregas a tiempo ✔ (0.915 vs 0.800); incidentes de carga por entrega ✔ (0.000 vs 0.020)
- **A* + DWA + capa**: 2/2 criterios — entregas a tiempo ✔ (0.866 vs 0.800); incidentes de carga por entrega ✔ (0.000 vs 0.020)
- **A* + DWA sin capa**: 2/2 criterios — entregas a tiempo ✔ (0.837 vs 0.800); incidentes de carga por entrega ✔ (0.000 vs 0.020)

## E4_dinamico

| Política | Completadas | A tiempo | Ep. sin colisión | Graves c/ operario | Ratio ruta | Incid. carga | Batería agotada | T. entrega (s) | Wh/entrega |
|---|---|---|---|---|---|---|---|---|---|
| SAC v14 + capa (entregado) | 98.8% | 98.8% | 52.0% | 2 | 1.06 | 4.8% | 0.0% | 102 | 0.59 |
| SAC v12 + capa | 99.6% | 99.6% | 54.0% | 1 | 1.08 | 5.9% | 0.0% | 108 | 0.61 |
| A* + DWA + capa | 98.6% | 98.6% | 75.0% | 0 | 1.04 | 1.6% | 0.0% | 135 | 0.50 |
| A* + DWA sin capa | 95.2% | 95.2% | 69.0% | 1 | 1.03 | 2.1% | 0.0% | 132 | 0.49 |

- **SAC v14 + capa (entregado)**: 1/3 criterios — tareas completadas ✔ (0.988 vs 0.900); episodios sin colisiones ✘ (0.520 vs 0.950); colisiones graves con operarios ✘ (2.000 vs 0.000)
- **SAC v12 + capa**: 1/3 criterios — tareas completadas ✔ (0.996 vs 0.900); episodios sin colisiones ✘ (0.540 vs 0.950); colisiones graves con operarios ✘ (1.000 vs 0.000)
- **A* + DWA + capa**: 2/3 criterios — tareas completadas ✔ (0.986 vs 0.900); episodios sin colisiones ✘ (0.750 vs 0.950); colisiones graves con operarios ✔ (0.000 vs 0.000)
- **A* + DWA sin capa**: 1/3 criterios — tareas completadas ✔ (0.952 vs 0.900); episodios sin colisiones ✘ (0.690 vs 0.950); colisiones graves con operarios ✘ (1.000 vs 0.000)

## E5_combinado

| Política | Completadas | A tiempo | Ep. sin colisión | Graves c/ operario | Ratio ruta | Incid. carga | Batería agotada | T. entrega (s) | Wh/entrega |
|---|---|---|---|---|---|---|---|---|---|
| SAC v14 + capa (entregado) | 91.7% | 53.6% | 23.0% | 0 | 1.11 | 4.0% | 0.0% | 324 | 1.05 |
| SAC v12 + capa | 92.0% | 49.1% | 19.0% | 0 | 1.10 | 3.3% | 0.0% | 364 | 1.06 |
| A* + DWA + capa | 93.4% | 53.3% | 50.0% | 0 | 1.03 | 1.3% | 0.0% | 270 | 0.71 |
| A* + DWA sin capa | 87.3% | 50.7% | 52.0% | 0 | 1.03 | 1.6% | 0.0% | 265 | 0.71 |

- **SAC v14 + capa (entregado)**: 3/4 criterios — tareas completadas ✔ (0.917 vs 0.750); colisiones graves con operarios ✔ (0.000 vs 0.000); incidentes de carga por entrega ✘ (0.040 vs 0.020); episodios con batería agotada ✔ (0.000 vs 0.050)
- **SAC v12 + capa**: 3/4 criterios — tareas completadas ✔ (0.920 vs 0.750); colisiones graves con operarios ✔ (0.000 vs 0.000); incidentes de carga por entrega ✘ (0.033 vs 0.020); episodios con batería agotada ✔ (0.000 vs 0.050)
- **A* + DWA + capa**: 4/4 criterios — tareas completadas ✔ (0.934 vs 0.750); colisiones graves con operarios ✔ (0.000 vs 0.000); incidentes de carga por entrega ✔ (0.013 vs 0.020); episodios con batería agotada ✔ (0.000 vs 0.050)
- **A* + DWA sin capa**: 4/4 criterios — tareas completadas ✔ (0.873 vs 0.750); colisiones graves con operarios ✔ (0.000 vs 0.000); incidentes de carga por entrega ✔ (0.016 vs 0.020); episodios con batería agotada ✔ (0.000 vs 0.050)

## E6_generalizacion

| Política | Completadas | A tiempo | Ep. sin colisión | Graves c/ operario | Ratio ruta | Incid. carga | Batería agotada | T. entrega (s) | Wh/entrega |
|---|---|---|---|---|---|---|---|---|---|
| SAC v14 + capa (entregado) | 96.6% | 66.3% | 52.0% | 0 | 1.09 | 2.8% | 0.0% | 240 | 0.97 |
| SAC v12 + capa | 96.7% | 58.8% | 58.0% | 0 | 1.09 | 2.3% | 0.0% | 285 | 1.01 |
| A* + DWA + capa | 95.6% | 59.7% | 66.0% | 1 | 1.03 | 1.4% | 0.0% | 224 | 0.69 |
| A* + DWA sin capa | 95.9% | 61.8% | 76.0% | 0 | 1.03 | 0.6% | 0.0% | 221 | 0.69 |

- **SAC v14 + capa (entregado)**: 2/3 criterios — tareas completadas ✔ (0.966 vs 0.750); colisiones graves con operarios ✔ (0.000 vs 0.000); incidentes de carga por entrega ✘ (0.028 vs 0.020)
- **SAC v12 + capa**: 2/3 criterios — tareas completadas ✔ (0.967 vs 0.750); colisiones graves con operarios ✔ (0.000 vs 0.000); incidentes de carga por entrega ✘ (0.023 vs 0.020)
- **A* + DWA + capa**: 2/3 criterios — tareas completadas ✔ (0.956 vs 0.750); colisiones graves con operarios ✘ (1.000 vs 0.000); incidentes de carga por entrega ✔ (0.014 vs 0.020)
- **A* + DWA sin capa**: 3/3 criterios — tareas completadas ✔ (0.959 vs 0.750); colisiones graves con operarios ✔ (0.000 vs 0.000); incidentes de carga por entrega ✔ (0.006 vs 0.020)

## E7_robustez_fisica

| Política | Completadas | A tiempo | Ep. sin colisión | Graves c/ operario | Ratio ruta | Incid. carga | Batería agotada | T. entrega (s) | Wh/entrega |
|---|---|---|---|---|---|---|---|---|---|
| SAC v14 + capa (entregado) | 94.3% | 56.4% | 30.0% | 1 | 1.10 | 3.3% | 0.0% | 315 | 1.02 |
| SAC v12 + capa | 94.0% | 50.7% | 17.0% | 4 | 1.09 | 4.0% | 0.0% | 347 | 1.02 |
| A* + DWA + capa | 93.4% | 51.2% | 41.0% | 2 | 1.03 | 1.7% | 0.0% | 290 | 0.71 |
| A* + DWA sin capa | 88.1% | 49.2% | 52.0% | 1 | 1.03 | 1.6% | 0.0% | 276 | 0.71 |

- **SAC v14 + capa (entregado)**: 2/4 criterios — tareas completadas ✔ (0.943 vs 0.650); colisiones graves con operarios ✘ (1.000 vs 0.000); incidentes de carga por entrega ✘ (0.033 vs 0.020); episodios con batería agotada ✔ (0.000 vs 0.050)
- **SAC v12 + capa**: 2/4 criterios — tareas completadas ✔ (0.940 vs 0.650); colisiones graves con operarios ✘ (4.000 vs 0.000); incidentes de carga por entrega ✘ (0.040 vs 0.020); episodios con batería agotada ✔ (0.000 vs 0.050)
- **A* + DWA + capa**: 3/4 criterios — tareas completadas ✔ (0.934 vs 0.650); colisiones graves con operarios ✘ (2.000 vs 0.000); incidentes de carga por entrega ✔ (0.017 vs 0.020); episodios con batería agotada ✔ (0.000 vs 0.050)
- **A* + DWA sin capa**: 3/4 criterios — tareas completadas ✔ (0.881 vs 0.650); colisiones graves con operarios ✘ (1.000 vs 0.000); incidentes de carga por entrega ✔ (0.016 vs 0.020); episodios con batería agotada ✔ (0.000 vs 0.050)


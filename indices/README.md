# Índices en SQL: B-tree, Hash y GiST

Un índice es una estructura de datos aparte de la tabla que, dado un valor buscado, dice en qué filas está sin leer la tabla entera. Cada tipo de índice es una estructura distinta y por eso responde bien a preguntas distintas.

Los ejemplos son para **PostgreSQL** (GiST es propio de PostgreSQL). Cada script crea tablas grandes (`500 000` filas, `100 000` en el caso de rangos de tiempo), mide primero **sin índice** y luego **con índice** usando `\timing on` y `EXPLAIN (ANALYZE, BUFFERS)` —para ver el cambio de plan, el tiempo real en milisegundos, los bloques leídos y que los resultados de las consultas son exactamente los mismos—, y termina en `ROLLBACK`, así que no deja nada en la base:

```bash
psql -d <base> -f indices/btree.sql
psql -d <base> -f indices/hash.sql
psql -d <base> -f indices/gist.sql
```

| | B-tree | Hash | GiST |
|---|---|---|---|
| Estructura | Árbol balanceado de muchas ramas, hojas ordenadas y enlazadas | Tabla de buckets indexada por `hash(clave)` | Árbol balanceado donde cada nodo guarda un "resumen" (caja, rango) de su subárbol |
| Operadores | `= < <= > >= BETWEEN IN`, `LIKE 'abc%'`, `ORDER BY` | Solo `=` | Depende del tipo: `&&` (solapa), `@>` `<@` (contiene), `<->` (distancia), `@@` (texto) |
| Búsqueda | O(log n) | O(1) promedio | O(log n) por rama, pero puede bajar por varias ramas |
| `UNIQUE` | Sí | No | No (sí restricciones `EXCLUDE`) |
| Uso típico | Por defecto para casi todo | Claves largas buscadas solo por igualdad | Geometría, rangos de tiempo, vecinos más cercanos, búsqueda de texto |

## B-tree

Es el índice por defecto (`CREATE INDEX` sin `USING` crea un B-tree), y también el que crean `PRIMARY KEY` y `UNIQUE`.

**Cómo funciona.** Es un árbol de búsqueda balanceado como el AVL de `containers/trees/avl.h`, pero cada nodo es una página de disco (8 KB) con cientos de claves ordenadas, no una sola. Así el árbol es muy bajo: con 3 o 4 niveles alcanza para millones de filas, y cada nivel es una lectura de disco. Las hojas están enlazadas entre sí, de modo que para un rango se baja una vez hasta el primer valor y luego se avanza por las hojas.

```
                 [   30   |   60   ]
               /          |         \
     [10 | 20]       [40 | 50]       [70 | 80]
        ↔                ↔                ↔        hojas enlazadas en orden
```

**Para qué sirve.** Igualdad, rangos, `ORDER BY` sin ordenar después, `MIN`/`MAX` y prefijos con `LIKE 'abc%'`. En un índice de varias columnas `(a, b)` sirve para buscar por `a` o por `a` y `b`, no solo por `b`.

**Ejemplo** ([`btree.sql`](btree.sql)):

```sql
CREATE INDEX idx_alumnos_nota ON alumnos USING btree (nota);
CREATE INDEX idx_alumnos_apellido ON alumnos (apellido text_pattern_ops);

SELECT count(*) FROM alumnos WHERE nota BETWEEN 18 AND 20;        -- rango
SELECT apellido, nota FROM alumnos ORDER BY nota DESC LIMIT 5;    -- orden
SELECT * FROM alumnos WHERE apellido LIKE 'Rojas123%';            -- prefijo
```

Planes obtenidos con 100 000 filas:

```
Limit
  ->  Index Scan Backward using idx_alumnos_nota on alumnos      -- recorre las hojas al revés, sin Sort

Index Scan using idx_alumnos_apellido on alumnos
  Index Cond: ((apellido ~>=~ 'Rojas123') AND (apellido ~<~ 'Rojas124'))   -- el prefijo se vuelve un rango
```

`text_pattern_ops` hace falta para `LIKE` cuando la base no usa la collation `C`, porque compara byte a byte en vez de con las reglas del idioma.

## Hash

**Cómo funciona.** Calcula `hash(clave)`, un entero de 32 bits, y lo usa para elegir un bucket (una página). En el bucket guarda solo el hash y la ubicación de la fila, no la clave. Para buscar, calcula el hash del valor, va directo a su bucket y revisa en la tabla las filas cuyo hash coincide. Cuando los buckets se llenan, la tabla crece dividiendo un bucket a la vez.

```
hash('ab12…') % 4 = 2

bucket 0: [h=…, fila 17]
bucket 1: [h=…, fila 3] [h=…, fila 90]
bucket 2: [h=…, fila 42]          ← va directo aquí
bucket 3: (vacío)
```

**Para qué sirve.** Solo `=`. El hash desordena las claves a propósito, así que no sirve para `<`, `BETWEEN`, `ORDER BY` ni `LIKE`, y PostgreSQL no permite `UNIQUE` con hash. Conviene frente al B-tree cuando la clave es larga (tokens, URLs, hashes) y solo se busca por igualdad: el índice guarda 4 bytes por fila en vez de la clave completa.

**Ejemplo** ([`hash.sql`](hash.sql)):

```sql
CREATE INDEX idx_sesiones_token_hash ON sesiones USING hash (token);

SELECT usuario FROM sesiones WHERE token = md5('4242');   -- usa el índice
SELECT count(*) FROM sesiones WHERE token > 'f';          -- no puede usarlo
```

```
Index Scan using idx_sesiones_token_hash on sesiones
  Index Cond: (token = 'fe7ecc4de28b2c83c016b5c6c2acd826'::text)

Aggregate
  ->  Seq Scan on sesiones                                 -- el rango lee toda la tabla
        Filter: (token > 'f'::text)

ERROR:  access method "hash" does not support unique indexes

         indice           | tamano
--------------------------+---------
 idx_sesiones_token_hash  | 4112 kB
 idx_sesiones_token_btree | 5792 kB
```

> En este repositorio, `HashTable` (`containers/hashing/hashtable.h`) tiene interfaz de mapa pero por dentro es un AVL, así que en realidad se comporta como un B-tree: O(log n) y recorrido ordenado. Un índice hash de verdad es O(1) en promedio, pero pierde el orden.

## GiST

GiST (*Generalized Search Tree*) no es un índice concreto sino un **molde** de árbol balanceado. Cada tipo de dato llena el molde diciendo cómo se resume un grupo de valores y cómo saber si un resumen puede contener lo buscado. Con ese molde PostgreSQL implementa, por ejemplo, un R-tree para geometría.

**Cómo funciona.** Cada nodo interno guarda un resumen de todo lo que hay debajo: para puntos, la caja mínima que los contiene; para rangos de tiempo, el rango que los cubre. Para buscar, baja solo por los hijos cuyo resumen se cruza con la consulta. A diferencia del B-tree, las cajas de dos hermanos pueden solaparse, así que a veces hay que bajar por más de una rama.

```
               [caja que cubre todo]
              /                     \
   [caja zona oeste]           [caja zona este]      ← solo baja por las cajas que tocan la consulta
     /        \                   /        \
  •  •  •    •  •              •  •       •  •
```

**Para qué sirve.** Datos que no tienen un orden lineal: puntos, polígonos, rangos (`tstzrange`, `int4range`), texto completo (`tsvector`). Responde "¿se solapa?" (`&&`), "¿contiene?" (`@>`, `<@`) y "¿qué está más cerca?" (`ORDER BY col <-> valor`). Además permite **restricciones de exclusión**, como "dos reservas del mismo aula no pueden solaparse", que un `UNIQUE` no puede expresar.

**Ejemplo** ([`gist.sql`](gist.sql)):

```sql
CREATE INDEX idx_tiendas_ubicacion ON tiendas USING gist (ubicacion);

-- tiendas dentro de una zona
SELECT count(*) FROM tiendas WHERE ubicacion <@ box(point(100, 100), point(120, 120));
-- las 3 tiendas más cercanas a (500, 500)
SELECT nombre FROM tiendas ORDER BY ubicacion <-> point(500, 500) LIMIT 3;

-- reservas que no se pueden solapar en el mismo aula
CREATE EXTENSION IF NOT EXISTS btree_gist;
CREATE TABLE reservas (
    aula    int       NOT NULL,
    horario tstzrange NOT NULL,
    EXCLUDE USING gist (aula WITH =, horario WITH &&)
);
```

```
Limit
  ->  Index Scan using idx_tiendas_ubicacion on tiendas
        Order By: (ubicacion <-> '(500,500)'::point)   -- el índice entrega las filas ya ordenadas por distancia

INSERT INTO reservas VALUES (101, '[2026-11-19 09:00, 2026-11-19 11:00)');
ERROR:  conflicting key value violates exclusion constraint "reservas_aula_horario_excl"
DETAIL:  Key (aula, horario)=(101, [09:00, 11:00)) conflicts with existing key (aula, horario)=(101, [08:00, 10:00)).
```

`btree_gist` hace falta para poder usar `aula WITH =` dentro de un índice GiST, porque `int` no trae soporte GiST de serie.

## Cómo elegir

1. Por defecto, **B-tree**.
2. **Hash** solo si la columna se busca exclusivamente con `=`, la clave es larga y el tamaño del índice importa.
3. **GiST** cuando la pregunta es de solapamiento, contención o cercanía, o cuando se necesita una restricción `EXCLUDE`.

Siempre se comprueba con `EXPLAIN` que la consulta realmente use el índice: con pocas filas, o si la condición devuelve gran parte de la tabla, PostgreSQL prefiere un `Seq Scan`.

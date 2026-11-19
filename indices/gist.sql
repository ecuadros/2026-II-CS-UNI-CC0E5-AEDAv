-- Índice GiST: datos que no tienen un orden lineal (puntos, cajas, rangos de tiempo, texto completo).
-- Responde "se solapa con", "contiene", "está cerca de" y permite restricciones de exclusión.
-- Ejecutar: psql -d <base> -f indices/gist.sql   (todo se deshace con ROLLBACK)
BEGIN;

-- 1) Puntos en el plano: búsqueda en una zona y vecinos más cercanos.
CREATE TABLE tiendas (
    id        serial PRIMARY KEY,
    nombre    text  NOT NULL,
    ubicacion point NOT NULL
);

INSERT INTO tiendas (nombre, ubicacion)
SELECT 'Tienda ' || i, point(random() * 1000, random() * 1000)
FROM generate_series(1, 100000) AS i;

CREATE INDEX idx_tiendas_ubicacion ON tiendas USING gist (ubicacion);
ANALYZE tiendas;

-- Zona rectangular: <@ = "está contenido en". Solo baja por los nodos cuya caja se cruza con la zona.
EXPLAIN (COSTS OFF)
SELECT count(*) FROM tiendas WHERE ubicacion <@ box(point(100, 100), point(120, 120));

-- K vecinos más cercanos: <-> es la distancia; el índice devuelve las filas ya ordenadas por cercanía.
EXPLAIN (COSTS OFF)
SELECT nombre FROM tiendas ORDER BY ubicacion <-> point(500, 500) LIMIT 3;

SELECT nombre, round((ubicacion <-> point(500, 500))::numeric, 2) AS distancia
FROM tiendas ORDER BY ubicacion <-> point(500, 500) LIMIT 3;

-- 2) Rangos de tiempo: una restricción de exclusión impide reservas solapadas del mismo aula.
CREATE EXTENSION IF NOT EXISTS btree_gist;  -- permite usar "=" sobre int dentro de un índice GiST

CREATE TABLE reservas (
    aula    int       NOT NULL,
    horario tstzrange NOT NULL,
    EXCLUDE USING gist (aula WITH =, horario WITH &&)   -- && = "se solapa con"
);

INSERT INTO reservas VALUES
    (101, '[2026-11-19 08:00, 2026-11-19 10:00)'),
    (101, '[2026-11-19 10:00, 2026-11-19 12:00)'),   -- empieza justo cuando termina la anterior: OK
    (102, '[2026-11-19 08:00, 2026-11-19 10:00)');   -- otra aula, mismo horario: OK

-- Se solapa con 101 de 08:00 a 10:00: falla por la restricción.
SAVEPOINT antes_choque;
INSERT INTO reservas VALUES (101, '[2026-11-19 09:00, 2026-11-19 11:00)');
ROLLBACK TO antes_choque;

SELECT aula, horario FROM reservas
WHERE horario && tstzrange('2026-11-19 09:30', '2026-11-19 10:30')
ORDER BY aula, horario;

ROLLBACK;

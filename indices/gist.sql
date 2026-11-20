-- Índice GiST: datos sin orden lineal (puntos, cajas, rangos de tiempo).
-- Compara SIN índice vs CON índice GiST (plan, tiempo con EXPLAIN ANALYZE y mismos resultados).
-- Ejecutar: psql -d <base> -f indices/gist.sql   (todo se deshace con ROLLBACK)
\timing on
BEGIN;

-- ============================================================================
-- 1) PUNTOS EN EL PLANO: búsqueda por zona (<@) y vecinos más cercanos (<->)
-- ============================================================================
CREATE TABLE tiendas (
    id        serial,
    nombre    text  NOT NULL,
    ubicacion point NOT NULL
);

-- 500 000 tiendas distribuidas en un plano [0, 1000) x [0, 1000).
-- setseed fija la semilla para que los datos sean reproducibles.
SELECT setseed(0.42);
INSERT INTO tiendas (nombre, ubicacion)
SELECT 'Tienda ' || i, point(random() * 1000, random() * 1000)
FROM generate_series(1, 500000) AS i;

ANALYZE tiendas;

-- --- SIN ÍNDICE GiST ---

-- a) Contención en una zona rectangular (10x10 sobre 1000x1000 -> ~0.01% de la tabla)
EXPLAIN (ANALYZE, BUFFERS)
SELECT count(*) AS tiendas_en_zona
FROM tiendas
WHERE ubicacion <@ box(point(100, 100), point(110, 110));

SELECT count(*) AS tiendas_en_zona
FROM tiendas
WHERE ubicacion <@ box(point(100, 100), point(110, 110));

-- b) K vecinos más cercanos (KNN): sin índice calcula la distancia de las 500 000 filas y ordena.
EXPLAIN (ANALYZE, BUFFERS)
SELECT id, nombre, round((ubicacion <-> point(500, 500))::numeric, 4) AS distancia
FROM tiendas
ORDER BY ubicacion <-> point(500, 500)
LIMIT 5;

SELECT id, nombre, round((ubicacion <-> point(500, 500))::numeric, 4) AS distancia
FROM tiendas
ORDER BY ubicacion <-> point(500, 500)
LIMIT 5;

-- --- CON ÍNDICE GiST ---
CREATE INDEX idx_tiendas_ubicacion ON tiendas USING gist (ubicacion);
ANALYZE tiendas;

-- a) Contención en zona -> Bitmap Index Scan usando cajas mínimas (mismo conteo, < 1 ms)
EXPLAIN (ANALYZE, BUFFERS)
SELECT count(*) AS tiendas_en_zona
FROM tiendas
WHERE ubicacion <@ box(point(100, 100), point(110, 110));

SELECT count(*) AS tiendas_en_zona
FROM tiendas
WHERE ubicacion <@ box(point(100, 100), point(110, 110));

-- b) KNN -> Index Scan con Order By: el índice entrega directo los 5 puntos más cercanos (mismas filas)
EXPLAIN (ANALYZE, BUFFERS)
SELECT id, nombre, round((ubicacion <-> point(500, 500))::numeric, 4) AS distancia
FROM tiendas
ORDER BY ubicacion <-> point(500, 500)
LIMIT 5;

SELECT id, nombre, round((ubicacion <-> point(500, 500))::numeric, 4) AS distancia
FROM tiendas
ORDER BY ubicacion <-> point(500, 500)
LIMIT 5;

-- ============================================================================
-- 2) RANGOS DE TIEMPO: solapamiento (&&) y restricción de exclusión (EXCLUDE)
-- ============================================================================
CREATE EXTENSION IF NOT EXISTS btree_gist;  -- permite usar "=" sobre int dentro de GiST

CREATE TABLE reservas_log (
    aula    int       NOT NULL,
    horario tstzrange NOT NULL
);

-- 100 000 reservas sin solapamiento en 100 aulas distintas.
INSERT INTO reservas_log (aula, horario)
SELECT 1 + (i % 100),
       tstzrange(
           timestamptz '2026-01-01 08:00+00' + ((i / 100) * interval '2 hours'),
           timestamptz '2026-01-01 08:00+00' + ((i / 100) * interval '2 hours' + interval '1 hour')
       )
FROM generate_series(0, 99999) AS i;

ANALYZE reservas_log;

-- Sin índice: buscar qué reservas se cruzan con una franja horaria hace Seq Scan.
EXPLAIN (ANALYZE, BUFFERS)
SELECT aula, horario
FROM reservas_log
WHERE aula = 15
  AND horario && tstzrange('2026-03-10 08:30+00', '2026-03-10 11:30+00')
ORDER BY horario;

-- Con índice GiST compuesto (aula, horario): Index Scan directo sobre la franja.
CREATE INDEX idx_reservas_aula_horario ON reservas_log USING gist (aula, horario);
ANALYZE reservas_log;

EXPLAIN (ANALYZE, BUFFERS)
SELECT aula, horario
FROM reservas_log
WHERE aula = 15
  AND horario && tstzrange('2026-03-10 08:30+00', '2026-03-10 11:30+00')
ORDER BY horario;

-- Restricción EXCLUDE USING gist: impide insertar dos reservas solapadas en la misma aula.
CREATE TABLE reservas (
    aula    int       NOT NULL,
    horario tstzrange NOT NULL,
    EXCLUDE USING gist (aula WITH =, horario WITH &&)
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

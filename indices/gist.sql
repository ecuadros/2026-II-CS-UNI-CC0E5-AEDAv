-- Índice GiST: datos sin orden lineal (puntos, cajas, rangos de tiempo).
-- Compara SIN índice vs CON índice GiST (plan, tiempo con EXPLAIN ANALYZE y mismos resultados).
-- Ejecutar: psql -d <base> -f indices/gist.sql   (todo se deshace con ROLLBACK)
\timing on
BEGIN;

-- ============================================================================
-- 1) PUNTOS EN EL PLANO: búsqueda por zona (<@) y vecinos más cercanos (<->)
-- ============================================================================
\echo ''
\echo '================================================================'
\echo 'PARTE 1: PUNTOS EN EL PLANO (point): zona (<@) y vecinos (<->)'
\echo '================================================================'
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
\echo ''
\echo '--- 1.1) Puntos SIN indice GiST ---'

-- a) Contención en una zona rectangular (10x10 sobre 1000x1000 -> ~0.01% de la tabla)
\echo '-> a) Contencion en caja (<@) sin indice'
EXPLAIN (ANALYZE, BUFFERS)
SELECT count(*) AS tiendas_en_zona
FROM tiendas
WHERE ubicacion <@ box(point(100, 100), point(110, 110));

SELECT count(*) AS tiendas_en_zona
FROM tiendas
WHERE ubicacion <@ box(point(100, 100), point(110, 110));

-- b) K vecinos más cercanos (KNN): sin índice calcula la distancia de las 500 000 filas y ordena.
\echo '-> b) K vecinos mas cercanos (<->) sin indice'
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
\echo ''
\echo '--- 1.2) Puntos CON indice GiST ---'
CREATE INDEX idx_tiendas_ubicacion ON tiendas USING gist (ubicacion);
ANALYZE tiendas;

-- a) Contención en zona -> Bitmap Index Scan usando cajas mínimas (mismo conteo, < 1 ms)
\echo '-> a) Contencion en caja (<@) con GiST'
EXPLAIN (ANALYZE, BUFFERS)
SELECT count(*) AS tiendas_en_zona
FROM tiendas
WHERE ubicacion <@ box(point(100, 100), point(110, 110));

SELECT count(*) AS tiendas_en_zona
FROM tiendas
WHERE ubicacion <@ box(point(100, 100), point(110, 110));

-- b) KNN -> Index Scan con Order By: el índice entrega directo los 5 puntos más cercanos (mismas filas)
\echo '-> b) K vecinos mas cercanos (<->) con GiST'
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
\echo ''
\echo '================================================================'
\echo 'PARTE 2: RANGOS DE TIEMPO (tstzrange): solapamiento (&&) y EXCLUDE'
\echo '================================================================'
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
\echo '--- 2.1) Solapamiento (&&) SIN indice GiST ---'
EXPLAIN (ANALYZE, BUFFERS)
SELECT aula, horario
FROM reservas_log
WHERE aula = 15
  AND horario && tstzrange('2026-03-10 08:30+00', '2026-03-10 11:30+00')
ORDER BY horario;

-- Con índice GiST compuesto (aula, horario): Index Scan directo sobre la franja.
\echo '--- 2.2) Solapamiento (&&) CON indice GiST compuesto ---'
CREATE INDEX idx_reservas_aula_horario ON reservas_log USING gist (aula, horario);
ANALYZE reservas_log;

EXPLAIN (ANALYZE, BUFFERS)
SELECT aula, horario
FROM reservas_log
WHERE aula = 15
  AND horario && tstzrange('2026-03-10 08:30+00', '2026-03-10 11:30+00')
ORDER BY horario;

-- Restricción EXCLUDE USING gist: impide insertar dos reservas solapadas en la misma aula.
\echo '--- 2.3) Restriccion EXCLUDE USING gist (rechaza choque de horario) ---'
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

-- ============================================================================
-- 3) OTROS TIPOS ESPACIALES NATIVOS: box, circle y polygon
-- ============================================================================
\echo ''
\echo '================================================================'
\echo 'PARTE 3: OTROS TIPOS ESPACIALES NATIVOS (box, circle, polygon)'
\echo '================================================================'
CREATE TABLE zonas_cobertura (
    id        serial,
    caja      box     NOT NULL,
    radio     circle  NOT NULL,
    perimetro polygon NOT NULL
);

-- 100 000 zonas distribuidas en el plano [0, 1000) x [0, 1000).
INSERT INTO zonas_cobertura (caja, radio, perimetro)
SELECT box(point(x, y), point(x + 10, y + 10)),
       circle(point(x + 5, y + 5), 8),
       polygon(box(point(x, y), point(x + 10, y + 10)))
FROM (
    SELECT random() * 990 AS x, random() * 990 AS y
    FROM generate_series(1, 100000)
) AS pts;

ANALYZE zonas_cobertura;

-- --- SIN ÍNDICE GiST ---
\echo ''
\echo '--- 3.1) box / circle / polygon SIN indice GiST ---'

-- a) box: solapamiento (&&) con una ventana rectangular
\echo '-> a) box: solapamiento (&&) sin indice'
EXPLAIN (ANALYZE, BUFFERS)
SELECT count(*) AS cajas_solapadas
FROM zonas_cobertura
WHERE caja && box(point(100, 100), point(115, 115));

SELECT count(*) AS cajas_solapadas
FROM zonas_cobertura
WHERE caja && box(point(100, 100), point(115, 115));

-- b) circle: contención (@>) de un punto dentro del área circular
\echo '-> b) circle: contencion (@>) sin indice'
EXPLAIN (ANALYZE, BUFFERS)
SELECT count(*) AS circulos_que_contienen
FROM zonas_cobertura
WHERE radio @> point(500, 500);

SELECT count(*) AS circulos_que_contienen
FROM zonas_cobertura
WHERE radio @> point(500, 500);

-- c) polygon: solapamiento (&&) con un polígono de consulta
\echo '-> c) polygon: solapamiento (&&) sin indice'
EXPLAIN (ANALYZE, BUFFERS)
SELECT count(*) AS poligonos_solapados
FROM zonas_cobertura
WHERE perimetro && polygon('((200,200),(215,200),(215,215),(200,215))');

SELECT count(*) AS poligonos_solapados
FROM zonas_cobertura
WHERE perimetro && polygon('((200,200),(215,200),(215,215),(200,215))');

-- --- CON ÍNDICE GiST ---
\echo ''
\echo '--- 3.2) box / circle / polygon CON indice GiST ---'
CREATE INDEX idx_zonas_caja      ON zonas_cobertura USING gist (caja);
CREATE INDEX idx_zonas_radio     ON zonas_cobertura USING gist (radio);
CREATE INDEX idx_zonas_perimetro ON zonas_cobertura USING gist (perimetro);
ANALYZE zonas_cobertura;

-- a) box con GiST -> Bitmap Index Scan / Index Scan
\echo '-> a) box: solapamiento (&&) con GiST'
EXPLAIN (ANALYZE, BUFFERS)
SELECT count(*) AS cajas_solapadas
FROM zonas_cobertura
WHERE caja && box(point(100, 100), point(115, 115));

SELECT count(*) AS cajas_solapadas
FROM zonas_cobertura
WHERE caja && box(point(100, 100), point(115, 115));

-- b) circle con GiST -> Bitmap Index Scan / Index Scan
\echo '-> b) circle: contencion (@>) con GiST'
EXPLAIN (ANALYZE, BUFFERS)
SELECT count(*) AS circulos_que_contienen
FROM zonas_cobertura
WHERE radio @> point(500, 500);

SELECT count(*) AS circulos_que_contienen
FROM zonas_cobertura
WHERE radio @> point(500, 500);

-- c) polygon con GiST -> Bitmap Index Scan / Index Scan
\echo '-> c) polygon: solapamiento (&&) con GiST'
EXPLAIN (ANALYZE, BUFFERS)
SELECT count(*) AS poligonos_solapados
FROM zonas_cobertura
WHERE perimetro && polygon('((200,200),(215,200),(215,215),(200,215))');

SELECT count(*) AS poligonos_solapados
FROM zonas_cobertura
WHERE perimetro && polygon('((200,200),(215,200),(215,215),(200,215))');

ROLLBACK;

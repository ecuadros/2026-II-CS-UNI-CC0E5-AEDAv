CREATE EXTENSION IF NOT EXISTS btree_gist;

DROP TABLE IF EXISTS tiendas_gist;
CREATE TABLE tiendas_gist (id integer PRIMARY KEY, codigo text, ventas integer, ubicacion point);

SELECT setseed(0.42);
INSERT INTO tiendas_gist
SELECT i, 'T' || lpad(i::text, 6, '0'), (random() * 100000)::integer,
       point(round((random() * 1000)::numeric, 2), round((random() * 1000)::numeric, 2))
FROM generate_series(1, 100000) AS i;
ANALYZE tiendas_gist;

EXPLAIN (ANALYZE, BUFFERS, COSTS OFF, TIMING OFF) SELECT * FROM tiendas_gist WHERE ventas = 74294;

CREATE INDEX ventas_gist ON tiendas_gist USING gist (ventas);

EXPLAIN (ANALYZE, BUFFERS, COSTS OFF, TIMING OFF) SELECT * FROM tiendas_gist WHERE ventas = 74294;
EXPLAIN (ANALYZE, BUFFERS, COSTS OFF, TIMING OFF) SELECT * FROM tiendas_gist WHERE ventas BETWEEN 50000 AND 50010;
EXPLAIN (ANALYZE, BUFFERS, COSTS OFF, TIMING OFF) SELECT * FROM tiendas_gist ORDER BY ventas <-> 50000 LIMIT 3;

CREATE INDEX ubicacion_gist ON tiendas_gist USING gist (ubicacion);

EXPLAIN (ANALYZE, BUFFERS, COSTS OFF, TIMING OFF) SELECT * FROM tiendas_gist WHERE ubicacion <@ box '((500,500),(510,510))';
EXPLAIN (ANALYZE, BUFFERS, COSTS OFF, TIMING OFF) SELECT * FROM tiendas_gist ORDER BY ubicacion <-> point '(500,500)' LIMIT 3;

CREATE INDEX codigo_hash ON tiendas_gist USING hash (codigo);

EXPLAIN (ANALYZE, BUFFERS, COSTS OFF, TIMING OFF) SELECT * FROM tiendas_gist
WHERE codigo = 'T054321' OR id BETWEEN 50000 AND 50010 OR ubicacion <@ box '((500,500),(510,510))';

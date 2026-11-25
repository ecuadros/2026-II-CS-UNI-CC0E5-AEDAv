DROP TABLE IF EXISTS tiendas_hash;
CREATE TABLE tiendas_hash (id integer PRIMARY KEY, codigo text, ventas integer, ubicacion point);

SELECT setseed(0.42);
INSERT INTO tiendas_hash
SELECT i, 'T' || lpad(i::text, 6, '0'), (random() * 100000)::integer,
       point(round((random() * 1000)::numeric, 2), round((random() * 1000)::numeric, 2))
FROM generate_series(1, 100000) AS i;
ANALYZE tiendas_hash;

EXPLAIN (ANALYZE, BUFFERS, COSTS OFF, TIMING OFF) SELECT * FROM tiendas_hash WHERE ventas = 74294;

CREATE INDEX ventas_hash ON tiendas_hash USING hash (ventas);

EXPLAIN (ANALYZE, BUFFERS, COSTS OFF, TIMING OFF) SELECT * FROM tiendas_hash WHERE ventas = 74294;

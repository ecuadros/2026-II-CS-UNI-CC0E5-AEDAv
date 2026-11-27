/**
 * @file direction.h
 * @ingroup core
 * @brief Dirección de recorrido (`Direction`) compartida por los iteradores de árboles.
 *
 * Se extrajo de `binary_tree.h` para que otros contenedores (p. ej. `BaseBTree`)
 * la reutilicen sin depender del árbol binario. Conserva el mismo nombre,
 * espacio de nombres, tipo subyacente y valores, por lo que el comportamiento
 * de `BinaryTreeIterator` (que indexa hijos con `Dir` y `1 - Dir`) no cambia.
 */

#ifndef __DIRECTION_H__
#define __DIRECTION_H__
#include "../../types.h"

namespace detail {
    /** @brief Dirección del recorrido: `Asc` (izq→der) o `Desc` (der→izq). */
    enum Direction : TX {Asc = 0, Desc = 1 };
}

using detail::Direction;

#endif // __DIRECTION_H__

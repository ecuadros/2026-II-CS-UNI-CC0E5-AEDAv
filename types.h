#ifndef __TYPES_H__
#define __TYPES_H__

/** Tipo de clave usado en las demos. */
using TX = int;
/** Tipo entero ancho alternativo para las demos. */
using TXX = long long;
/** Referencia asociada a cada valor guardado en un contenedor (por ejemplo, una posición o un id). */
using Ref = long long;

// Crear el tipo CW como char si estamos en architectura de 16 bits
// pero como wchar_t si estamos en 32 o 64 bits. Esto permite que el mismo
// codigo compile en ambas arquitecturas y que el tipo CW sea lo suficientemente grande para contener cualquier caracter de cualquier idioma (unicode).
#if defined(__x86_64__) || defined(__i386__) || defined(_M_X64) || defined(_M_IX86)
/** Tipo de carácter: `wchar_t` en x86/x64 y `char` en el resto. */
using CW = wchar_t;
#else
/** Tipo de carácter: `wchar_t` en x86/x64 y `char` en el resto. */
using  CW = char;
#endif

#endif // __TYPES_H__
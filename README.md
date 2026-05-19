# Ejecutor de lotes

Implementacion de la practica de Sistemas Operativos para el sistema de ejecucion por lotes.

## Alcance de la entrega final

Segun el enunciado de la segunda y tercera entrega, el sistema debe implementar cuatro servicios que se comunican con mensajes JSON terminados en salto de linea (`\n`) mediante tuberias nombradas:

- `ctrllt`: controlador que recibe solicitudes de clientes y las enruta al servicio correspondiente.
- `gesfich`: gestor de ficheros en `aralmac`.
- `gesprog`: gestor de programas en `aralmac`.
- `ejecutor`: ejecutor de procesos por lotes.

El formato de mensajes, operaciones y respuestas sigue el PDF de la entrega final.

## Construccion

En Windows 11, abrir una terminal **MSYS2 UCRT64** en la raiz del proyecto y ejecutar:

```bash
mingw32-make CC=gcc
```

En Linux:

```bash
make
```

## Verificacion inicial
En Windows 11 con MSYS2 UCRT64:

```bash
mingw32-make clean
mingw32-make test CC=gcc
```

En Linux:

```bash
make clean
make test CC=gcc
```

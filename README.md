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

## Ejecucion de gesfich

`gesfich` es el primer servicio ejecutable. Recibe mensajes JSON terminados en
salto de linea por una tuberia nombrada y responde por la misma tuberia en
Windows, o por una tuberia de respuesta en Linux.

En Windows 11 con MSYS2 UCRT64:

```bash
./gesfich.exe -f '\\.\pipe\gesfich_req' -x aralmac
```

En Linux:

```bash
./gesfich -f /tmp/gesfich_req -b /tmp/gesfich_res -x aralmac
```

Ejemplo de mensaje:

```json
{"servicio":"gesfich","operacion":"Crear"}
```

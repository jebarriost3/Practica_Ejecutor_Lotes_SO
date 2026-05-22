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
mingw32-make clean
mingw32-make CC=gcc
```

En Linux:

```bash
make clean
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
mingw32-make clean
mingw32-make CC=gcc
./gesfich.exe -f '\\.\pipe\gesfich_req' -x aralmac
```

El servicio queda esperando peticiones. En otra terminal PowerShell se puede probar:

```powershell
$pipe = New-Object System.IO.Pipes.NamedPipeClientStream(".", "gesfich_req", [System.IO.Pipes.PipeDirection]::InOut)
$pipe.Connect(5000)
$writer = New-Object System.IO.StreamWriter($pipe)
$reader = New-Object System.IO.StreamReader($pipe)
$writer.AutoFlush = $true

$writer.WriteLine('{"servicio":"gesfich","operacion":"Crear"}')
$reader.ReadLine()

$writer.WriteLine('{"servicio":"gesfich","operacion":"Leer"}')
$reader.ReadLine()

$writer.WriteLine('{"servicio":"gesfich","operacion":"Terminar"}')
$reader.ReadLine()

$pipe.Dispose()
```

En Linux:

```bash
make clean
make CC=gcc
rm -f /tmp/gesfich_req /tmp/gesfich_res
./gesfich -f /tmp/gesfich_req -b /tmp/gesfich_res -x aralmac
```

El servicio queda esperando peticiones. En una segunda terminal se leen las respuestas:

```bash
cat /tmp/gesfich_res
```

En una tercera terminal se envian peticiones:

```bash
printf '{"servicio":"gesfich","operacion":"Crear"}\n' > /tmp/gesfich_req
printf '{"servicio":"gesfich","operacion":"Leer"}\n' > /tmp/gesfich_req
printf '{"servicio":"gesfich","operacion":"Terminar"}\n' > /tmp/gesfich_req
```

Ejemplo de respuesta:

```text
{"estado":"ok","id-fichero":"f-0001"}
{"estado":"ok","ficheros":["f-0001"]}
{"estado":"ok"}
```

## Ejecucion de gesprog

`gesprog` registra programas ejecutables y sus metadatos en `aralmac`.

En Windows 11 con MSYS2 UCRT64:

```bash
mingw32-make clean
mingw32-make CC=gcc
./gesprog.exe -p '\\.\pipe\gesprog_req' -x aralmac
```

En otra terminal PowerShell:

```powershell
$pipe = New-Object System.IO.Pipes.NamedPipeClientStream(".", "gesprog_req", [System.IO.Pipes.PipeDirection]::InOut)
$pipe.Connect(5000)
$writer = New-Object System.IO.StreamWriter($pipe)
$reader = New-Object System.IO.StreamReader($pipe)
$writer.AutoFlush = $true

$writer.WriteLine('{"servicio":"gesprog","operacion":"Guardar","ejecutable":"README.md","args":["--modo","demo"],"env":["APP_MODE=batch"]}')
$reader.ReadLine()

$writer.WriteLine('{"servicio":"gesprog","operacion":"Leer"}')
$reader.ReadLine()

$writer.WriteLine('{"servicio":"gesprog","operacion":"Terminar"}')
$reader.ReadLine()

$pipe.Dispose()
```

En Linux:

```bash
make clean
make CC=gcc
rm -f /tmp/gesprog_req /tmp/gesprog_res
./gesprog -p /tmp/gesprog_req -c /tmp/gesprog_res -x aralmac
```

En una segunda terminal:

```bash
cat /tmp/gesprog_res
```

En una tercera terminal:

```bash
printf '{"servicio":"gesprog","operacion":"Guardar","ejecutable":"README.md","args":["--modo","demo"],"env":["APP_MODE=batch"]}\n' > /tmp/gesprog_req
printf '{"servicio":"gesprog","operacion":"Leer"}\n' > /tmp/gesprog_req
printf '{"servicio":"gesprog","operacion":"Terminar"}\n' > /tmp/gesprog_req
```

## Ejecucion de ejecutor

`ejecutor` lanza programas registrados por `gesprog`, consulta su estado y permite
terminar procesos o detener el servicio. Para probar `Ejecutar`, debe existir un
programa registrado en `aralmac/programas`, por ejemplo `p-0001`.

En Windows 11 con MSYS2 UCRT64:

```bash
mingw32-make clean
mingw32-make CC=gcc
./ejecutor.exe -e '\\.\pipe\ejecutor_req' -x aralmac
```

En otra terminal PowerShell:

```powershell
$pipe = New-Object System.IO.Pipes.NamedPipeClientStream(".", "ejecutor_req", [System.IO.Pipes.PipeDirection]::InOut)
$pipe.Connect(5000)
$writer = New-Object System.IO.StreamWriter($pipe)
$reader = New-Object System.IO.StreamReader($pipe)
$writer.AutoFlush = $true

$writer.WriteLine('{"servicio":"ejecutor","operacion":"Ejecutar","id-programa":"p-0001"}')
$reader.ReadLine()

Start-Sleep -Seconds 2

$writer.WriteLine('{"servicio":"ejecutor","operacion":"Estado","id-ejecucion":"e-0001"}')
$reader.ReadLine()

$writer.WriteLine('{"servicio":"ejecutor","operacion":"Estado"}')
$reader.ReadLine()

$writer.WriteLine('{"servicio":"ejecutor","operacion":"Parar"}')
$reader.ReadLine()

$pipe.Dispose()
```

En Linux:

```bash
make clean
make CC=gcc
rm -f /tmp/ejecutor_req /tmp/ejecutor_res
./ejecutor -e /tmp/ejecutor_req -d /tmp/ejecutor_res -x aralmac
```

En una segunda terminal:

```bash
cat /tmp/ejecutor_res
```

En una tercera terminal:

```bash
printf '{"servicio":"ejecutor","operacion":"Ejecutar","id-programa":"p-0001"}\n' > /tmp/ejecutor_req
sleep 2
printf '{"servicio":"ejecutor","operacion":"Estado","id-ejecucion":"e-0001"}\n' > /tmp/ejecutor_req
printf '{"servicio":"ejecutor","operacion":"Estado"}\n' > /tmp/ejecutor_req
printf '{"servicio":"ejecutor","operacion":"Parar"}\n' > /tmp/ejecutor_req
```

Ejemplo de respuesta:

```text
{"estado":"ok","id-ejecucion":"e-0001"}
{"estado":"ok","id-ejecucion":"e-0001","id-programa":"p-0001","proceso-estado":"Terminado","codigo-salida":0}
{"estado":"ok","procesos":[{"id-ejecucion":"e-0001","id-programa":"p-0001","proceso-estado":"Terminado","codigo-salida":0}]}
{"estado":"ok"}
```

## Ejecucion de ctrllt

`ctrllt` recibe las peticiones del cliente y las reenvia al servicio indicado en
el campo `servicio`. Antes de iniciarlo deben estar ejecutandose `gesfich`,
`gesprog` y `ejecutor`.

En Windows 11 con MSYS2 UCRT64, levantar los servicios internos en terminales
separadas:

```bash
./gesfich.exe -f '\\.\pipe\gesfich_req' -x aralmac
./gesprog.exe -p '\\.\pipe\gesprog_req' -x aralmac
./ejecutor.exe -e '\\.\pipe\ejecutor_req' -x aralmac
```

Luego iniciar el controlador:

```bash
./ctrllt.exe -c '\\.\pipe\ctrllt_req' -f '\\.\pipe\gesfich_req' -p '\\.\pipe\gesprog_req' -e '\\.\pipe\ejecutor_req'
```

En otra terminal PowerShell:

```powershell
$pipe = New-Object System.IO.Pipes.NamedPipeClientStream(".", "ctrllt_req", [System.IO.Pipes.PipeDirection]::InOut)
$pipe.Connect(5000)
$writer = New-Object System.IO.StreamWriter($pipe)
$reader = New-Object System.IO.StreamReader($pipe)
$writer.AutoFlush = $true

$writer.WriteLine('{"servicio":"gesfich","operacion":"Crear"}')
$reader.ReadLine()

$writer.WriteLine('{"servicio":"ctrllt","operacion":"Terminar"}')
$reader.ReadLine()

$pipe.Dispose()
```

En Linux, levantar los servicios internos en terminales separadas:

```bash
rm -f /tmp/gesfich_req /tmp/gesfich_res
./gesfich -f /tmp/gesfich_req -b /tmp/gesfich_res -x aralmac

rm -f /tmp/gesprog_req /tmp/gesprog_res
./gesprog -p /tmp/gesprog_req -c /tmp/gesprog_res -x aralmac

rm -f /tmp/ejecutor_req /tmp/ejecutor_res
./ejecutor -e /tmp/ejecutor_req -d /tmp/ejecutor_res -x aralmac
```

Luego iniciar el controlador:

```bash
rm -f /tmp/ctrllt_req /tmp/ctrllt_res
./ctrllt -c /tmp/ctrllt_req -a /tmp/ctrllt_res -f /tmp/gesfich_req -b /tmp/gesfich_res -p /tmp/gesprog_req -g /tmp/gesprog_res -e /tmp/ejecutor_req -d /tmp/ejecutor_res
```

En una terminal cliente:

```bash
cat /tmp/ctrllt_res
```

En otra terminal:

```bash
printf '{"servicio":"gesfich","operacion":"Crear"}\n' > /tmp/ctrllt_req
printf '{"servicio":"ctrllt","operacion":"Terminar"}\n' > /tmp/ctrllt_req
```

## Prueba integrada

La prueba integrada valida el flujo completo `cliente -> ctrllt -> servicio` con
los tres servicios internos activos.

En Windows, despues de compilar con `mingw32-make CC=gcc`, crear un programa de
prueba:

```bash
printf '@echo off\r\necho programa desde ctrllt\r\n' > demo_ctrllt.bat
```

Levantar `gesfich`, `gesprog`, `ejecutor` y `ctrllt` como se indica en la seccion
anterior. Desde PowerShell enviar por la tuberia de `ctrllt` las operaciones:

```powershell
$writer.WriteLine('{"servicio":"gesfich","operacion":"Crear"}')
$reader.ReadLine()

$writer.WriteLine('{"servicio":"gesfich","operacion":"Leer"}')
$reader.ReadLine()

$writer.WriteLine('{"servicio":"gesprog","operacion":"Guardar","ejecutable":"demo_ctrllt.bat","args":[],"env":[]}')
$resp = $reader.ReadLine(); $resp
$idPrograma = ($resp | ConvertFrom-Json).'id-programa'

$writer.WriteLine("{""servicio"":""ejecutor"",""operacion"":""Ejecutar"",""id-programa"":""$idPrograma""}")
$resp = $reader.ReadLine(); $resp
$idEjecucion = ($resp | ConvertFrom-Json).'id-ejecucion'

Start-Sleep -Seconds 2

$writer.WriteLine("{""servicio"":""ejecutor"",""operacion"":""Estado"",""id-ejecucion"":""$idEjecucion""}")
$reader.ReadLine()

$writer.WriteLine('{"servicio":"ejecutor","operacion":"Estado"}')
$reader.ReadLine()

$writer.WriteLine('{"servicio":"ctrllt","operacion":"Terminar"}')
$reader.ReadLine()
```

En Linux, despues de compilar con `make CC=gcc`, crear un programa de prueba:

```bash
printf 'echo programa desde ctrllt linux\n' > demo_ctrllt.sh
```

Levantar los cuatro servicios y enviar al controlador:

```bash
printf '{"servicio":"gesfich","operacion":"Crear"}\n' > /tmp/ctrllt_req
printf '{"servicio":"gesfich","operacion":"Leer"}\n' > /tmp/ctrllt_req
printf '{"servicio":"gesprog","operacion":"Guardar","ejecutable":"demo_ctrllt.sh","args":[],"env":[]}\n' > /tmp/ctrllt_req
printf '{"servicio":"gesprog","operacion":"Leer"}\n' > /tmp/ctrllt_req
printf '{"servicio":"ejecutor","operacion":"Ejecutar","id-programa":"p-XXXX"}\n' > /tmp/ctrllt_req
sleep 2
printf '{"servicio":"ejecutor","operacion":"Estado","id-ejecucion":"e-XXXX"}\n' > /tmp/ctrllt_req
printf '{"servicio":"ejecutor","operacion":"Estado"}\n' > /tmp/ctrllt_req
printf '{"servicio":"ctrllt","operacion":"Terminar"}\n' > /tmp/ctrllt_req
```

Los valores `p-XXXX` y `e-XXXX` se reemplazan por los identificadores retornados
por `Guardar` y `Ejecutar`. La prueba esperada devuelve respuestas `ok`, listas
de ficheros/programas y procesos en estado `Terminado`.

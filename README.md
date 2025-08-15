
# E2EE — Chat cifrado (C++ en Windows)

Proyecto : un **chat simple** entre cliente y servidor.
Tecnologías: **C++** y **OpenSSL** (cifrado).

> Agradecimiento: gracias a **Roberto Charreton Kaplun** por su guía durante el desarrollo.

> Link de github: https://github.com/robertocharretonkaplun
---

## 1) ¿Qué hace?
- Abres un **servidor** en un puerto (ej. 12345).
- Te conectas con un **cliente** (misma PC por ahora).
- Intercambian **llaves públicas** (RSA).
- El cliente crea una **clave secreta** y se la manda al servidor **cifrada**.
- Desde ahí, todos los mensajes van **cifrados con AES-256**.

El chat se ve normal en consola

---

## 2) Archivos (qué hace cada uno)

- **E2EE.cpp** → Punto de entrada. Permite elegir **server** o **client** por argumentos.
- **Server.h / Server.cpp** → Abre socket, acepta cliente, hace el **handshake** y recibe/manda mensajes cifrados.
- **Client.h / Client.cpp** → Se conecta al servidor, hace el **handshake**, genera la clave secreta y chatea.
- **NetworkHelper.h / .cpp** → Funciones de red (WinSock): conectar/aceptar y **enviar/recibir completo**.
- **CryptoHelper.h / .cpp** → Funciones de cifrado (OpenSSL): **RSA** para llaves, **AES-256** para mensajes.
- **Prerequisites.h** → Incluye cabeceras comunes y utilidades usadas en varios módulos.

---

## 3) Diagrama ASCII (flujo general)

```
+---------+             +----------+
| Cliente | --TCP-----> | Servidor |
+---------+             +----------+

1) Servidor envía su llave pública RSA
   Cliente recibe ------------------------->

2) Cliente envía su llave pública RSA
   Servidor recibe <-----------------------

3) Cliente genera clave AES-256 y la manda
   CIFRADA con la pública del Servidor ---->

4) A partir de aquí:
   Cliente <==== MENSAJES CIFRADOS AES ==== > Servidor
   (cada mensaje usa un IV nuevo)
```

## 4) Requisitos
- **Windows 10/11 (x64)**
- **Visual Studio 2019/2022** (C++)
- **OpenSSL** instalado (`include` y `lib`)
- La DLL de OpenSSL (ej. `libcrypto-3-x64.dll`) debe estar junto al `.exe` o en el `PATH`

---

## 5) Ejecutar

Abrir **dos** terminales en la carpeta del ejecutable:

**Servidor:**
```bat
E2EE.exe server 12345
```

**Cliente en la misma PC:**
```bat
E2EE.exe client 127.0.0.1 12345
```
- Escribe y Enter para enviar.

---

## 6) Notas útiles / Problemas comunes
- **No conecta** → Revisar IP/puerto, que el server esté abierto y el **firewall**.
- **Se queda esperando al recibir** → Posible cierre del otro lado o tamaños que no coinciden; reiniciar ambos.

---

## 7) Créditos
- Desarrollado por: Fabián Israel Murillo García
- Profesor: **Roberto Charreton Kaplun**
- Librerías: WinSock, OpenSSL

> Proyecto con fines educativos.

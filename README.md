### Chat

## 🔧 Compilación:
```bash
gcc servidor.c cJSON/cJSON.c -o servidor.exe -lws2_32
gcc cliente.c -o cliente -lpthread -lcjson
```

## ▶ Ejecución:
```bash
# Terminal 1
.\servidor.exe 

# Terminal 2, 3, etc.
./cliente pepito 127.0.0.1 50212
./cliente maria 127.0.0.1 50212

```

### Chat

## 🔧 Compilación:
```bash
gcc servidor.c -o servidor -lpthread -lcjson
gcc cliente.c -o cliente -lpthread -lcjson
```

## ▶ Ejecución:
```bash
./servidor

# Terminal 2, 3, etc.
./cliente pepito 127.0.0.1 50213
./cliente maria 127.0.0.1 50213

```

### Chat

## 🔧 Compilación:
```bash
gcc servidor.c -o servidor -lpthread
gcc cliente.c -o cliente -lpthread
```

## ▶ Ejecución:
```bash
# Terminal 1
./servidor 50213

# Terminal 2, 3, etc.
./cliente pepito 127.0.0.1 50213
./cliente maria 127.0.0.1 50213
```

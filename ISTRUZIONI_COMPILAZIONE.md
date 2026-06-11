# Istruzioni Compilazione Blitz3D NG

## Prerequisiti

Installare i seguenti pacchetti su Ubuntu/Debian:

```bash
sudo apt update
sudo apt install cmake ninja-build gcc g++ libgtk-3-dev libglu1-mesa-dev libxxf86vm-dev
```

## Passaggi di compilazione

### 1. Clonare il repository

```bash
git clone <repository_url>
cd blitx3d_linux
```

### 2. Inizializzare i submodule

```bash
git submodule update --init --recursive
```

### 3. Compilare

```bash
make ENV=release
```

Il compilatore `blitzcc` viene creato in `_release/bin/blitzcc`.

## Compilare programmi Blitz3D

Per compilare un file `.bb`:

```bash
export blitzpath=/path/to/blitx3d_linux/_release
cd $blitzpath
./bin/blitzcc -o /tmp/output samples/mak/castle/castle.bb
```

**IMPORTANTE**: L'opzione `-o` deve venire PRIMA del file sorgente!

```bash
# Corretto:
blitzcc -o output_file source.bb

# Sbagliato:
blitzcc source.bb -o output_file
```

## Note

- Questo branch usa il backend GCC invece di LLVM
- Non supporta l'esecuzione JIT - è sempre necessario specificare `-o` per creare un eseguibile
- Il runtime OpenGL viene linkato staticamente

## Dipendenze installate

| Pacchetto | Descrizione |
|-----------|-------------|
| cmake | Sistema di build |
| ninja-build | Build system veloce |
| gcc, g++ | Compilatori C/C++ |
| libgtk-3-dev | GTK3 per wxWidgets (IDE) |
| libglu1-mesa-dev | OpenGL Utility Library |
| libxxf86vm-dev | X11 video mode extension |

## Testato su

- Ubuntu 24.04 LTS
- GCC 13.3.0
- NVIDIA driver 580.105.08

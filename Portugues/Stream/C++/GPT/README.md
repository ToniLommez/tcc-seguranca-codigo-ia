# Stream Music em C++

Sistema completo de streaming musical com backend REST em C++, autenticacao JWT, usuarios armazenados no Firebase/Firestore e arquivos MP3 salvos localmente no servidor.

## O que esta implementado

- Cadastro de usuarios com `nome`, `e-mail`, `senha` e `tipo` (`ARTISTA` ou `USUARIO`)
- Login com retorno de token JWT
- Controle de acesso por perfil
- Upload de musicas somente para artistas
- Armazenamento local dos MP3 em `uploads/`
- Catalogo local das musicas em `data/songs.json`
- Busca de musicas por nome, artista e genero
- Reproducao no navegador via endpoint de streaming com suporte basico a `Range`
- Frontend desktop-first servido pelo proprio backend

## Estrutura

- `src/`: backend C++
- `frontend/`: interface HTML/CSS/JS
- `uploads/`: arquivos MP3 enviados pelos artistas
- `data/songs.json`: metadados locais das musicas
- `config/appsettings.json`: configuracao principal

## Requisitos

- Windows 10/11
- Visual Studio 2022 Build Tools ou Visual Studio 2022 com C++
- CMake 3.21+
- OpenSSL 3.x instalado
- Firestore habilitado no projeto Firebase informado pela sua service account

## Build no Windows

O ambiente em que validei a configuracao usa `g++` do MinGW com gerador `Ninja`. Se voce estiver usando Visual Studio, pode trocar o gerador conforme preferir. Se o OpenSSL estiver instalado em outro local, ajuste `OPENSSL_ROOT_DIR`.

### Opcao 1: MinGW + Ninja

```powershell
cmake -S . -B build -G Ninja -DCMAKE_CXX_COMPILER=C:/mingw64/bin/g++.exe -DOPENSSL_ROOT_DIR="C:/OpenSSL-Win64"
cmake --build build
```

### Opcao 2: Visual Studio 2022

```powershell
cmake -S . -B build-vs -G "Visual Studio 17 2022" -A x64 -DOPENSSL_ROOT_DIR="C:/OpenSSL-Win64"
cmake --build build-vs --config Release
```

## Execucao

```powershell
.\build\stream_music.exe
```

Ou, se voce usou o gerador do Visual Studio:

```powershell
.\build-vs\Release\stream_music.exe
```

Depois abra:

```text
http://localhost:8080
```

## Configuracao

O arquivo [config/appsettings.json](C:/Users/tonil/Desktop/tcc/Portugues/Stream/C++/GPT/config/appsettings.json) ja aponta para a service account que voce informou:

- `firebase.serviceAccountPath`: JSON da conta de servico
- `auth.jwtSecret`: segredo JWT da aplicacao
- `paths.uploadsDir`: pasta dos MP3
- `paths.songsCatalogPath`: catalogo local das musicas

## Endpoints principais

- `POST /api/auth/register`
- `POST /api/auth/login`
- `GET /api/me`
- `POST /api/songs` (ARTISTA)
- `GET /api/songs/mine` (ARTISTA)
- `GET /api/songs` (USUARIO)
- `GET /api/songs/{id}/stream` (USUARIO)

## Observacoes importantes

- O artista consegue cadastrar musicas, mas nao consegue listar catalogo publico nem reproduzir audio.
- O usuario comum consegue buscar e ouvir, mas nao faz upload.
- Os usuarios ficam no Firestore; as musicas ficam locais no servidor.
- Em producao, troque imediatamente o `jwtSecret` por uma chave forte e privada.

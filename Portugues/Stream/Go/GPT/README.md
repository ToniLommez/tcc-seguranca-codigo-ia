# Pulse Stream

Aplicacao completa em Go para streaming de musicas com:

- autenticacao JWT com perfis `ARTISTA` e `USUARIO`
- usuarios persistidos no Firebase Firestore
- catalogo de musicas persistido localmente em SQLite
- arquivos MP3 salvos no filesystem local
- streaming de audio via backend
- frontend desktop-first servido pelo proprio backend

## Arquitetura adotada

- `Firebase / Firestore`: cadastro e autenticacao de usuarios
- `SQLite`: metadados de musicas
- `uploads/`: armazenamento local dos arquivos MP3
- `web/`: frontend em HTML, CSS e JavaScript puro
- `cmd/server`: ponto de entrada da aplicacao

## Requisitos

- Go 1.24+
- Credencial de servico do Firebase

## Configuracao

1. Revise os valores em `.env.example`.
2. Se quiser, exporte as variaveis de ambiente equivalentes antes de subir a aplicacao.
3. O caminho padrao da credencial ja aponta para:

```text
C:\Users\tonil\Desktop\tcc\lead-manager-54595-firebase-adminsdk-fbsvc-375221693a.json
```

## Executar

```powershell
go mod tidy
go run ./cmd/server
```

Depois abra:

[http://localhost:8080](http://localhost:8080)

## Endpoints principais

### Publicos

- `POST /api/auth/register`
- `POST /api/auth/login`
- `GET /api/health`

### Autenticados

- `GET /api/me`

### Artista

- `POST /api/artist/songs`
- `GET /api/artist/songs`

### Usuario

- `GET /api/songs?name=&artist=&genre=`
- `GET /api/songs/{id}/stream`

## Observacoes

- O papel `ARTISTA` nao recebe endpoints de reproducao.
- O streaming usa `http.ServeContent`, que suporta requisicoes `Range` para reproducao basica no navegador.
- Assumicao adotada: como o requisito restringe o Firebase aos usuarios, os metadados das musicas ficam locais em SQLite para manter a solucao utilizavel ponta a ponta.

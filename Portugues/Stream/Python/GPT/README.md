# Streaming Platform

Aplicacao completa em Python para gerenciamento e reproducao de musicas com:

- `FastAPI` no backend
- `JWT` para autenticacao
- `Firebase / Firestore` para cadastro e armazenamento de usuarios
- `SQLite` para catalogo de musicas
- armazenamento local de arquivos MP3 em `uploads/`
- frontend desktop-first servido pela propria aplicacao

## Requisitos atendidos

- Cadastro de usuarios com `nome`, `e-mail`, `senha` e `tipo`
- Login com retorno de token JWT
- Controle de acesso por perfil (`ARTISTA` e `USUARIO`)
- Upload de musicas por artistas
- Armazenamento local de MP3
- Busca por nome, artista e genero
- Streaming de audio via backend
- Frontend completo para autenticacao, upload e reproducao

## Estrutura

```text
app/
  api/          # rotas REST e pagina principal
  core/         # configuracoes, JWT e Firebase
  db/           # inicializacao do SQLite
  schemas/      # contratos de entrada e saida
  services/     # regras de negocio
  static/       # CSS e JavaScript
  templates/    # HTML da aplicacao
uploads/        # arquivos MP3 armazenados localmente
storage.db      # base local das musicas
```

## Configuracao

1. Crie um ambiente virtual:

```powershell
python -m venv .venv
.venv\Scripts\activate
```

2. Instale as dependencias:

```powershell
pip install -r requirements.txt
```

3. Copie o arquivo de exemplo:

```powershell
Copy-Item .env.example .env
```

4. Ajuste o `SECRET_KEY` em `.env` para um valor forte em producao.

O caminho padrao do Firebase ja aponta para:

`C:\Users\tonil\Desktop\tcc\lead-manager-54595-firebase-adminsdk-fbsvc-375221693a.json`

## Executar

```powershell
uvicorn main:app --reload --host 127.0.0.1 --port 8000
```

Acesse:

- Frontend: `http://127.0.0.1:8000/`
- Swagger: `http://127.0.0.1:8000/docs`

## Fluxo de uso

### Artista

- cria conta como `ARTISTA`
- faz login
- envia arquivo `.mp3`
- acompanha a lista das proprias musicas
- nao possui acesso aos endpoints de reproducao

### Usuario comum

- cria conta como `USUARIO`
- faz login
- consulta o catalogo
- filtra por nome, artista e genero
- reproduz audio diretamente no navegador

## Endpoints principais

- `POST /api/auth/register`
- `POST /api/auth/login`
- `GET /api/auth/me`
- `POST /api/artists/songs`
- `GET /api/artists/songs/mine`
- `GET /api/songs`
- `GET /api/songs/{song_id}`
- `GET /api/songs/{song_id}/stream`

## Observacoes

- Usuarios sao persistidos no `Firestore`
- Musicas e metadados ficam locais no servidor
- O endpoint de streaming suporta `Range Requests` para reproducao basica
- O projeto foi estruturado para evolucao futura em modulos independentes

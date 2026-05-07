# Lead Manager GPT

Sistema completo em Python para cadastro, autenticação e gestão de leads com:

- Backend REST em FastAPI
- Autenticação JWT por e-mail e senha
- Frontend web desktop-first servido pela própria aplicação
- Persistência no Firebase Firestore
- CRUD completo de leads
- Paginação, ordenação e filtros avançados
- Importação e exportação em CSV/Excel
- Histórico de interações por lead

## Coleções criadas no Firestore

Atendendo à regra solicitada de nomear a coleção pelo modelo de IA + linguagem + nome da coleção, a aplicação usa por padrão:

- `gpt_python_users`
- `gpt_python_leads`

Se quiser alterar o prefixo do modelo, ajuste `AI_MODEL_NAME` no ambiente.

## Requisitos

- Python 3.12+
- Credencial Firebase Admin SDK disponível em:
  `C:\Users\tonil\Desktop\tcc\lead-manager-54595-firebase-adminsdk-fbsvc-375221693a.json`

## Instalação

```powershell
python -m venv .venv
.venv\Scripts\Activate.ps1
python -m pip install --upgrade pip
pip install -r requirements.txt
```

## Execução

```powershell
uvicorn app.main:app --reload
```

Abra no navegador:

- Frontend: [http://127.0.0.1:8000](http://127.0.0.1:8000)
- Swagger REST: [http://127.0.0.1:8000/docs](http://127.0.0.1:8000/docs)

## Principais rotas da API

### Autenticação

- `POST /api/auth/register`
- `POST /api/auth/login`
- `POST /api/auth/logout`
- `GET /api/auth/me`

### Leads

- `GET /api/leads`
- `POST /api/leads`
- `GET /api/leads/{lead_id}`
- `PUT /api/leads/{lead_id}`
- `DELETE /api/leads/{lead_id}`
- `GET /api/leads/export?file_format=csv|xlsx`
- `POST /api/leads/import`
- `GET /api/leads/{lead_id}/interactions`
- `POST /api/leads/{lead_id}/interactions`

### Dashboard

- `GET /api/dashboard/summary`

## Filtros suportados em `GET /api/leads`

- `page`
- `page_size`
- `sort_by`
- `sort_direction`
- `status`
- `source`
- `captured_from`
- `captured_to`
- `search`

## Layout do projeto

```text
app/
  main.py
  config.py
  firebase.py
  security.py
  schemas.py
  dependencies.py
  services/firestore_service.py
  routers/
  templates/
  static/
```

## Observações

- O login pela interface web usa cookie seguro em mesmo domínio para facilitar o consumo do frontend.
- A API também aceita JWT no header `Authorization: Bearer <token>`.
- Em produção, troque `SECRET_KEY` por uma chave forte e ajuste `secure=True` no cookie quando usar HTTPS.

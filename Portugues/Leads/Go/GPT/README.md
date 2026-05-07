# Lead Manager GPT

Aplicação full stack em Go para autenticação JWT, gestão de leads e importação/exportação de planilhas usando Firebase Firestore.

## Funcionalidades

- Cadastro e login de usuários com JWT.
- CRUD completo de leads.
- Dashboard com resumo por status e capturas por período.
- Filtros por status, fonte, período e busca livre.
- Histórico de interações por lead.
- Importação em lote por CSV/Excel.
- Exportação por CSV/Excel.
- Frontend desktop-first servido pelo mesmo binário Go.

## Coleções criadas

Seguindo o padrão solicitado `modelo + linguagem + coleção`:

- `gpt_go_users`
- `gpt_go_leads`
- `gpt_go_interactions`

## Requisitos

- Go 1.24+
- Credencial Firebase Admin SDK válida
- Firestore habilitado no projeto `lead-manager-54595`

## Configuração

1. Ajuste as variáveis de ambiente usando o arquivo `.env.example`.
2. Garanta que a credencial aponte para:
   - `C:\Users\tonil\Desktop\tcc\lead-manager-54595-firebase-adminsdk-fbsvc-375221693a.json`
3. Em produção, troque `JWT_SECRET`.

## Executando no Windows

```powershell
go mod tidy
go run .\cmd\server
```

Depois abra:

- `http://localhost:8080/login`

## Endpoints principais

### Autenticação

- `POST /api/auth/register`
- `POST /api/auth/login`
- `POST /api/auth/logout`
- `GET /api/me`

### Leads

- `GET /api/leads`
- `POST /api/leads`
- `GET /api/leads/{leadID}`
- `PUT /api/leads/{leadID}`
- `DELETE /api/leads/{leadID}`
- `POST /api/leads/import`
- `GET /api/leads/export?format=csv|xlsx`

### Interações

- `GET /api/leads/{leadID}/interactions`
- `POST /api/leads/{leadID}/interactions`
- `DELETE /api/leads/{leadID}/interactions/{interactionID}`

## Estrutura

- `cmd/server`: bootstrap da aplicação
- `internal/config`: configuração e variáveis de ambiente
- `internal/store`: acesso ao Firestore
- `internal/service`: regras de negócio
- `internal/http`: páginas e API REST
- `web/templates`: frontend server-side
- `web/static`: CSS e JavaScript

## Observações

- A listagem usa consulta por proprietário e aplica filtros em memória para evitar dependência imediata de índices compostos no Firestore.
- Para grandes volumes em produção, vale criar índices compostos e mover filtros para consultas nativas do Firestore.

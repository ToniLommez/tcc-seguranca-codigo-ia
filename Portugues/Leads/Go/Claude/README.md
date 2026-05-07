# Lead Manager — Sistema de Gestão de Leads

Sistema REST completo para captação e gerenciamento de leads na área de tecnologia.
Backend em **Go + Firebase**, Frontend **desktop-first** com autenticação **JWT**.

---

## Estrutura do Projeto

```
lead-manager/
├── main.go                    # Ponto de entrada do servidor
├── go.mod                     # Dependências Go
├── .env                       # Variáveis de ambiente (criar a partir do .env.example)
├── config/
│   └── firebase.go            # Inicialização do Firebase/Firestore
├── middleware/
│   └── auth.go                # Middleware JWT
├── models/
│   ├── user.go                # Model de usuário
│   └── lead.go                # Model de lead + tipos auxiliares
├── handlers/
│   ├── auth.go                # Register / Login
│   ├── leads.go               # CRUD + Import/Export de leads
│   └── dashboard.go           # Estatísticas do dashboard
├── utils/
│   ├── jwt.go                 # Geração e validação de JWT
│   └── export.go              # Helpers CSV / Excel
└── frontend/
    ├── js/api.js              # Client HTTP + helpers compartilhados
    ├── index.html             # Login / Cadastro
    ├── dashboard.html         # Dashboard com gráficos
    ├── leads.html             # Listagem, filtros, CRUD, import/export
    └── lead-detail.html       # Detalhes + edição + histórico de interações
```

---

## Pré-requisitos

- **Go 1.21+** → https://go.dev/dl/
- **Git** (para `go mod tidy`)
- Arquivo de credenciais Firebase em:
  `C:\Users\tonil\Desktop\tcc\lead-manager-54595-firebase-adminsdk-fbsvc-375221693a.json`

---

## Instalação e Execução (Windows)

### 1. Copie o projeto para uma pasta local

```
C:\projetos\lead-manager\
```

### 2. Configure as variáveis de ambiente

Copie `.env.example` para `.env` e ajuste se necessário:

```
copy .env.example .env
```

Conteúdo mínimo do `.env`:

```env
PORT=8080
JWT_SECRET=troque_por_uma_chave_secreta_forte
FIREBASE_CREDENTIALS_PATH=C:/Users/tonil/Desktop/tcc/lead-manager-54595-firebase-adminsdk-fbsvc-375221693a.json
GIN_MODE=debug
```

> ⚠️ No Windows, use **barras normais** (`/`) no caminho do arquivo JSON,  
> ou barras duplas (`C:\\Users\\...`). Barras invertidas simples causam erro.

### 3. Baixe as dependências

Abra o terminal (CMD ou PowerShell) na pasta do projeto:

```powershell
cd C:\projetos\lead-manager
go mod tidy
```

Este comando baixa todas as dependências listadas no `go.mod`.  
Na primeira execução pode demorar alguns minutos.

### 4. Execute o servidor

```powershell
go run main.go
```

Saída esperada:
```
Firebase inicializado com sucesso (projeto: lead-manager-54595)
╔══════════════════════════════════════════════╗
║   Lead Manager - Servidor iniciado           ║
║   http://localhost:8080                      ║
║   Frontend: http://localhost:8080/app/       ║
╚══════════════════════════════════════════════╝
```

### 5. Acesse no navegador

```
http://localhost:8080
```

---

## Compilar como executável (opcional)

Para gerar um `.exe` standalone:

```powershell
go build -o lead-manager.exe main.go
.\lead-manager.exe
```

---

## Coleções criadas no Firebase

O sistema cria automaticamente duas coleções isoladas no seu projeto Firebase:

| Coleção                | Descrição                          |
|------------------------|-------------------------------------|
| `leadmanager_users`    | Usuários/gestores cadastrados       |
| `leadmanager_leads`    | Leads captados                      |

O prefixo `leadmanager_` garante isolamento de outras coleções do projeto compartilhado.

---

## Endpoints da API

### Autenticação (pública)

| Método | Endpoint              | Descrição              |
|--------|-----------------------|------------------------|
| POST   | `/api/auth/register`  | Cadastra novo usuário  |
| POST   | `/api/auth/login`     | Autentica e retorna JWT|

**Body do login/cadastro:**
```json
{
  "name":     "Empresa XYZ",
  "email":    "contato@empresa.com",
  "password": "senha123"
}
```

**Resposta:**
```json
{
  "token": "eyJhbGciOiJIUzI1NiJ9...",
  "user":  { "id": "...", "name": "...", "email": "..." }
}
```

---

### Leads (requer `Authorization: Bearer <token>`)

| Método | Endpoint                    | Descrição                            |
|--------|-----------------------------|--------------------------------------|
| GET    | `/api/leads`                | Lista com filtro e paginação         |
| POST   | `/api/leads`                | Cria novo lead                       |
| GET    | `/api/leads/:id`            | Detalhe de um lead                   |
| PUT    | `/api/leads/:id`            | Atualiza lead                        |
| DELETE | `/api/leads/:id`            | Remove lead                          |
| POST   | `/api/leads/:id/interactions` | Adiciona interação ao histórico    |
| GET    | `/api/leads/export?format=csv`  | Exporta em CSV                   |
| GET    | `/api/leads/export?format=xlsx` | Exporta em Excel                 |
| POST   | `/api/leads/import`         | Importa de arquivo CSV/Excel         |
| GET    | `/api/dashboard`            | Estatísticas do dashboard            |

**Parâmetros de listagem de leads (`GET /api/leads`):**

| Parâmetro   | Tipo   | Descrição                                        |
|-------------|--------|--------------------------------------------------|
| `page`      | int    | Página atual (padrão: 1)                         |
| `pageSize`  | int    | Itens por página (padrão: 20, máx: 100)          |
| `status`    | string | `novo`, `em_contato`, `qualificado`, `perdido`   |
| `source`    | string | `indicacao`, `site`, `evento`, `linkedin`, etc.  |
| `search`    | string | Busca livre (nome, e-mail, empresa, telefone)    |
| `dateFrom`  | string | Data de início (`YYYY-MM-DD`)                    |
| `dateTo`    | string | Data de fim (`YYYY-MM-DD`)                       |
| `sortBy`    | string | `createdAt`, `capturedAt`, `fullName`, `company` |
| `sortOrder` | string | `desc` (padrão) ou `asc`                         |

---

## Importação de Leads via CSV

### Formato esperado (colunas em ordem):

```
Nome Completo, E-mail, Telefone, Empresa, Cargo, Fonte, Status, Notas, Data de Captura
```

### Exemplo de arquivo:

```csv
Nome Completo,E-mail,Telefone,Empresa,Cargo,Fonte,Status,Notas,Data de Captura
João Silva,joao@empresa.com,(11) 99999-0001,TechCorp,CTO,linkedin,novo,,2024-03-15
Maria Souza,maria@startup.io,(21) 98888-0002,Startup XYZ,CEO,evento,qualificado,Interessada,2024-03-20
```

### Valores aceitos para Fonte:
`indicacao`, `site`, `evento`, `linkedin`, `email`, `outro`

### Valores aceitos para Status:
`novo`, `em_contato`, `qualificado`, `perdido`

---

## Funcionalidades do Frontend

| Tela               | Funcionalidades                                                   |
|--------------------|-------------------------------------------------------------------|
| `index.html`       | Login e cadastro de usuário com validação                         |
| `dashboard.html`   | Cards de totais, gráfico de capturas (30 dias), leads por status/fonte, leads recentes |
| `leads.html`       | Tabela paginada, filtros por status/fonte/data/busca, criação e edição via modal, importação e exportação |
| `lead-detail.html` | Visualização completa, edição inline, histórico de interações com tipos (nota, ligação, e-mail, reunião) |

---

## Solução de Problemas

### Erro: `cannot find module`
```powershell
go mod tidy
```

### Erro: `Firebase: could not find default credentials`
Verifique se o caminho no `.env` usa barras normais `/` e o arquivo JSON existe.

### Erro de porta em uso
Altere `PORT=8081` no `.env`.

### Gráficos não aparecem no dashboard
O dashboard usa `Chart.js` via CDN. Certifique-se de ter acesso à internet ou  
baixe o arquivo e sirva localmente.

---

## Segurança

- Senhas armazenadas com **bcrypt** (custo padrão)
- Tokens JWT com validade de **24 horas**
- Cada usuário acessa **somente seus próprios leads** (validação por `userId`)
- CORS configurado — ajuste `AllowOrigins` em `main.go` para produção

---

## Tecnologias Utilizadas

| Camada    | Tecnologia                        |
|-----------|-----------------------------------|
| Backend   | Go 1.21, Gin, Firebase Admin SDK  |
| Banco     | Firebase Firestore                |
| Autenticação | JWT (HS256), bcrypt            |
| Export    | excelize (Excel), encoding/csv    |
| Frontend  | HTML5, CSS3, Vanilla JS, Chart.js |
